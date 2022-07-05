/// Routines to manage the overall operation of the file system.  Implements
/// routines to map from textual file names to files.
///
/// Each file in the file system has:
/// * a file header, stored in a sector on disk (the size of the file header
///   data structure is arranged to be precisely the size of 1 disk sector);
/// * a number of data blocks;
/// * an entry in the file system directory.
///
/// The file system consists of several data structures:
/// * A bitmap of free disk sectors (cf. `bitmap.h`).
/// * A directory of file names and file headers.
///
/// Both the bitmap and the directory are represented as normal files.  Their
/// file headers are located in specific sectors (sector 0 and sector 1), so
/// that the file system can find them on bootup.
///
/// The file system assumes that the bitmap and directory files are kept
/// “open” continuously while Nachos is running.
///
/// For those operations (such as `Create`, `Remove`) that modify the
/// directory and/or bitmap, if the operation succeeds, the changes are
/// written immediately back to disk (the two files are kept open during all
/// this time).  If the operation fails, and we have modified part of the
/// directory and/or bitmap, we simply discard the changed version, without
/// writing it back to disk.
///
/// Our implementation at this point has the following restrictions:
///
/// * there is no synchronization for concurrent accesses;
/// * files have a fixed size, set when the file is created;
/// * files cannot be bigger than about 3KB in size;
/// * there is no hierarchical directory structure, and only a limited number
///   of files can be added to the system;
/// * there is no attempt to make the system robust to failures (if Nachos
///   exits in the middle of an operation that modifies the file system, it
///   may corrupt the disk).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_system.hh"
#include "file_header.hh"
#include "lib/bitmap.hh"
#include "threads/lock.hh"
#include "file_access_controller.hh"
#include "threads/system.hh"

#include <stdio.h>
#include <string.h>


/// Sectors containing the file headers for the bitmap of free sectors, and
/// the directory of files.  These file headers are placed in well-known
/// sectors, so that they can be located on boot-up.
static const unsigned FREE_MAP_SECTOR = 0;
static const unsigned DIRECTORY_SECTOR = 1;



/// Initialize the file system.  If `format == true`, the disk has nothing on
/// it, and we need to initialize the disk to contain an empty directory, and
/// a bitmap of free sectors (with almost but not all of the sectors marked
/// as free).
///
/// If `format == false`, we just have to open the files representing the
/// bitmap and the directory.
///
/// * `format` -- should we initialize the disk?
FileSystem::FileSystem(bool format)
{
    DEBUG('f', "Initializing the file system.\n");
    if (format) {
        Bitmap     *freeMap = new Bitmap(NUM_SECTORS);
        Directory  *dir     = new Directory(NUM_DIR_ENTRIES);
        FileHeader *mapH    = new FileHeader;
        FileHeader *dirH    = new FileHeader;

        DEBUG('f', "Formatting the file system.\n");

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
        freeMap->Mark(FREE_MAP_SECTOR);
        freeMap->Mark(DIRECTORY_SECTOR);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        ASSERT(mapH->Allocate(freeMap, FREE_MAP_FILE_SIZE));
        ASSERT(dirH->Allocate(freeMap, DIRECTORY_FILE_SIZE));

        // Flush the bitmap and directory `FileHeader`s back to disk.
        // We need to do this before we can `Open` the file, since open reads
        // the file header off of disk (and currently the disk has garbage on
        // it!).

        DEBUG('f', "Writing headers back to disk.\n");
        mapH->WriteBack(FREE_MAP_SECTOR);
        dirH->WriteBack(DIRECTORY_SECTOR);

        // OK to open the bitmap and directory files now.
        // The file system operations assume these two files are left open
        // while Nachos is running.

        freeMapFile   = new OpenFile(FREE_MAP_SECTOR);
        directoryFile = new OpenFile(DIRECTORY_SECTOR);

        // Once we have the files “open”, we can write the initial version of
        // each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        dir->Add("..", DIRECTORY_SECTOR, true); // Root has root as parent.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
        freeMap->WriteBack(freeMapFile);     // flush changes to disk
        dir->WriteBack(directoryFile);

        if (debug.IsEnabled('f')) {
            freeMap->Print();
            dir->Print();

            delete freeMap;
            delete dir;
            delete mapH;
            delete dirH;
        }
    } else {
        // If we are not formatting the disk, just open the files
        // representing the bitmap and directory; these are left open while
        // Nachos is running.
        freeMapFile   = new OpenFile(FREE_MAP_SECTOR);
        directoryFile = new OpenFile(DIRECTORY_SECTOR);
    }
    openFileList = new OpenFileList();
    freeMapLock = new Lock("freeMapLock");
    fileSystemLock = new Lock("fileSystemLock");
    //caurrentDirectory = DIRECTORY_SECTOR; // Esto es el sector al directorio actual.
    root = DIRECTORY_SECTOR;
}

FileSystem::~FileSystem()
{
    delete freeMapFile;
    delete directoryFile;
    delete openFileList;
    delete freeMapLock;
    delete fileSystemLock;
}


void
FileSystem::AcquireCurrentDirectoryLock(Directory *dir)
{
    fileSystemLock->Acquire();
    OpenFile *CDFile = new OpenFile(currentThread->currentDirectory);
    dir->FetchFrom(CDFile);
    delete CDFile;
    dir->directoryLock->Acquire();
    fileSystemLock->Release();
}

/// Create a file in the Nachos file system (similar to UNIX `create`).
/// Since we cannot increase the size of files dynamically, we have to give
/// `Create` the initial size of the file.
///
/// The steps to create a file are:
/// 1. Make sure the file does not already exist.
/// 2. Allocate a sector for the file header.
/// 3. Allocate space on disk for the data blocks for the file.
/// 4. Add the name to the directory.
/// 5. Store the new file header on disk.
/// 6. Flush the changes to the bitmap and the directory back to disk.
///
/// Return true if everything goes ok, otherwise, return false.
///
/// Create fails if:
/// * file is already in directory;
/// * no free space for file header;
/// * no free entry for file in directory;
/// * no free space for data blocks for the file.
///
/// Note that this implementation assumes there is no concurrent access to
/// the file system!
///
/// * `name` is the name of file to be created.
/// * `initialSize` is the size of file to be created.
bool
FileSystem::Create(const char *path, unsigned initialSize)
{
    ASSERT(initialSize < MAX_FILE_SIZE_W_INDIR);

    int oldCurrentDirectory = currentThread->currentDirectory;

    char* name = MoveToDirectory(path);
    if (name == nullptr) {
        DEBUG('f', "Can't create file in path\n");
        currentThread->currentDirectory = oldCurrentDirectory;
        return false;
    }

    DEBUG('f', "Creating file %s, size %u\n", name, initialSize);

    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    AcquireCurrentDirectoryLock(dir);

    bool success;

    if (dir->Find(name) != -1) {
        success = false;  // File is already in directory.
    } else {
        Bitmap *freeMap = new Bitmap(NUM_SECTORS);
        freeMapLock->Acquire();
        freeMap->FetchFrom(freeMapFile);
        int sector = freeMap->Find();
          // Find a sector to hold the file header.
        if (sector == -1) {
            success = false;  // No free block for file header.
        } else {
            if (!dir->Add(name, sector)) {
                success = false;  // No space in directory.
            } else {
                FileHeader *h = new FileHeader;
                success = h->Allocate(freeMap, initialSize);
                // Fails if no space on disk for data.
                if (success) {
                    // Everything worked, flush all changes back to disk.
                    h->WriteBack(sector);
                    OpenFile *currentFile = new OpenFile(currentThread->currentDirectory);
                    dir->WriteBack(currentFile);
                    freeMap->WriteBack(freeMapFile);
                    delete currentFile;
                }
                delete h;
            }
        }
        freeMapLock->Release();
        delete freeMap;
    }
    dir->directoryLock->Release();
    currentThread->currentDirectory = oldCurrentDirectory;
    delete dir;
    return success;
}

/// Create a file in the Nachos file system (similar to UNIX `create`).
/// Since we cannot increase the size of files dynamically, we have to give
/// `Create` the initial size of the file.
///
/// The steps to create a file are:
/// 1. Make sure the file does not already exist.
/// 2. Allocate a sector for the file header.
/// 3. Allocate space on disk for the data blocks for the file.
/// 4. Add the name to the directory.
/// 5. Store the new file header on disk.
/// 6. Flush the changes to the bitmap and the directory back to disk.
///
/// Return true if everything goes ok, otherwise, return false.
///
/// Create fails if:
/// * file is already in directory;
/// * no free space for file header;
/// * no free entry for file in directory;
/// * no free space for data blocks for the file.
///
/// Note that this implementation assumes there is no concurrent access to
/// the file system!
///
/// * `name` is the name of file to be created.
/// * `initialSize` is the size of file to be created.
bool
FileSystem::CreateDir(const char *path)
{ 
    int oldCurrentDirectory = currentThread->currentDirectory;
    char* name = MoveToDirectory(path);

    if (name == nullptr) {
        DEBUG('f', "Can't create directory in path\n");
        currentThread->currentDirectory = oldCurrentDirectory;
        return false;
    }
    
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    AcquireCurrentDirectoryLock(dir);
    
    bool success;
    
    if (dir->FindDir(name) != -1) {
        success = false;  // Directory is already in directory.
    } else {
        Bitmap *freeMap = new Bitmap(NUM_SECTORS);
        freeMapLock->Acquire();
        freeMap->FetchFrom(freeMapFile);
        int sector = freeMap->Find();
          // Find a sector to hold the file header.
        if (sector == -1) {
            success = false;  // No free block for file header.
        } else {
            if (!dir->Add(name, sector, true)) {
                success = false;  // No space in directory.
            } else {
                // Creamos un directorio que fetchea del sector asignado mas arriba
                FileHeader *newFirH = new FileHeader;
                success = newFirH->Allocate(freeMap, DIRECTORY_FILE_SIZE);
                // Fails if no space on disk for data.
                if(success) {
                    // Everything worked, flush all changes back to disk.
                    newFirH->WriteBack(sector);
                    OpenFile *currentFile = new OpenFile(currentThread->currentDirectory); 
                    dir->WriteBack(currentFile);
                    Directory *newDir = new Directory(NUM_DIR_ENTRIES);
                    OpenFile *CDFile = new OpenFile(sector);
                    newDir->Add("..", currentThread->currentDirectory, true);
                    newDir->WriteBack(CDFile);
                    freeMap->WriteBack(freeMapFile);
                    delete newDir;
                    delete CDFile;
                    delete currentFile;
                }
                delete newFirH;
            }
        }
        freeMapLock->Release();
        delete freeMap;
    }
    dir->directoryLock->Release();
    delete dir;
    
    currentThread->currentDirectory = oldCurrentDirectory;
    DEBUG('f', "Current directory after fallback %u\n", currentThread->currentDirectory);
    return success;
}

/// Open a file for reading and writing.
///
/// To open a file:
/// 1. Find the location of the file's header, using the directory.
/// 2. Bring the header into memory.
///
/// * `name` is the text name of the file to be opened.

OpenFile *
FileSystem::Open(const char *path)
{

    int oldCurrentDirectory = currentThread->currentDirectory;
    char* name = MoveToDirectory(path);
    
    if (name == nullptr) {
        DEBUG('f', "Can't open file in path\n");
        currentThread->currentDirectory = oldCurrentDirectory;
        return nullptr;
    }

    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    OpenFile  *openFile = nullptr;

    DEBUG('f', "Opening file %s\n", name);
    AcquireCurrentDirectoryLock(dir);
    int sector = dir->Find(name);
    DEBUG('f', "Sector: %d\n", sector);
    if (sector >= 0) {
        DEBUG('f', "sector > 0\n");
        openFileList->Acquire();
        FileAccessController* accessController = openFileList->AddOpenFile(sector);

        if (accessController != nullptr) {
            DEBUG('f', "sector > 0\n");
            openFile = new OpenFile(sector, accessController); // Esto busca en el sector, pero puede ser que no exista mas para abrir
        }
        openFileList->Release();
    }
    dir->directoryLock->Release();
    delete dir;
    currentThread->currentDirectory = oldCurrentDirectory;
    return openFile;  // Return null if not found.
}

/// Delete a file from the file system.
///
/// This requires:
/// 1. Remove it from the directory.
/// 2. Delete the space for its header.
/// 3. Delete the space for its data blocks.
/// 4. Write changes to directory, bitmap back to disk.
///
/// Return true if the file was deleted, false if the file was not in the
/// file system.
///
/// * `name` is the text name of the file to be removed.
bool
FileSystem::Remove(const char *path)
{
    int oldCurrentDirectory = currentThread->currentDirectory;
    char* name = MoveToDirectory(path);
    
    if (name == nullptr) {
        DEBUG('f', "Can't remove file in path\n");
        currentThread->currentDirectory = oldCurrentDirectory;
        return false;
    }

    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    AcquireCurrentDirectoryLock(dir);
    int sector = dir->Find(name);
    if (sector == -1) {
       delete dir;
       dir->directoryLock->Release();
       currentThread->currentDirectory = oldCurrentDirectory;
       return false;  // file not found
    }
    dir->Remove(name);
    OpenFile * currentFile = new OpenFile(currentThread->currentDirectory);
    dir->WriteBack(currentFile);
    dir->directoryLock->Release();
    delete currentFile;
    delete dir;
    openFileList->Acquire();
    // Si SetToBeRemoved es falso, el archivo no puede ser eliminado aun, esta en uso (pero se setea para ser eleminado en el futuro).
    if(openFileList->SetToBeRemoved(sector)) {
        openFileList->RemoveOpenFile(sector);
        openFileList->Release();
        currentThread->currentDirectory = oldCurrentDirectory;
        return DeleteFromDisk(sector); // Esto solo sucede si el archivo no lo tiene nadie abierto durante el Remove
    }
    openFileList->Release();
    currentThread->currentDirectory = oldCurrentDirectory;
    return true;
}

/// Delete a file from the file system.
///
/// This requires:
/// 1. Remove it from the directory.
/// 2. Delete the space for its header.
/// 3. Delete the space for its data blocks.
/// 4. Write changes to directory, bitmap back to disk.
///
/// Return true if the file was deleted, false if the file was not in the
/// file system.
///
/// * `name` is the text name of the file to be removed.
bool
FileSystem::RemoveDir(const char *path)
{
    int oldCurrentDirectory = currentThread->currentDirectory;
    char* name = MoveToDirectory(path);

    if (name == nullptr) {
        DEBUG('f', "Can't remove directory in path\n");
        currentThread->currentDirectory = oldCurrentDirectory;
        return false;
    }

    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    AcquireCurrentDirectoryLock(dir);
    int sector = dir->FindDir(name);
    if (sector == -1) {
        dir->directoryLock->Release();
        delete dir;
        currentThread->currentDirectory = oldCurrentDirectory;
        return false;  // directory not found
    }

    OpenFile *toRemoveFile = new OpenFile(sector);
    Directory *dirToRemove = new Directory(NUM_DIR_ENTRIES);
    dirToRemove->FetchFrom(toRemoveFile);

    const RawDirectory * raw = dirToRemove->GetRaw();
    unsigned countInUse = 0;
    for(unsigned i = 0 ; i < raw->tableSize; i++) {
        if (raw->table[i].inUse) 
            countInUse++;
    }

    if (countInUse > 1) {
        delete toRemoveFile;
        delete dirToRemove;
        dir->directoryLock->Release();
        delete dir;
        currentThread->currentDirectory = oldCurrentDirectory;
        DEBUG('f', "Can't remove directory in path because it has elements\n");
        return false;
    }

    dir->Remove(name);
    OpenFile * currentFile = new OpenFile(currentThread->currentDirectory);
    dir->WriteBack(currentFile);
    dir->directoryLock->Release();
    delete currentFile;
    delete dir;
    
    dirToRemove->Remove("..");
    currentThread->currentDirectory = oldCurrentDirectory;
    return DeleteFromDisk(sector);
}

// Dado un FileHeader, eliminamos el archivo al que corresponde
bool
FileSystem::DeleteFromDisk(int sector)
{
    FileHeader *fileH = new FileHeader;
    Bitmap *freeMap = new Bitmap(NUM_SECTORS);

    freeMapLock->Acquire();
    fileH->FetchFrom(sector);
    

    // Lockeamos porque dos personas modifican freeMap al mismo tiempo
    freeMap->FetchFrom(freeMapFile);

    fileH->Deallocate(freeMap);  // Remove data blocks.
    freeMap->Clear(sector);      // Remove header block.

    freeMap->WriteBack(freeMapFile);  // Flush to disk.
    freeMapLock->Release();
    delete fileH;
    delete freeMap;
    return true;
}

bool
FileSystem::Close(int sector)
{
    DEBUG('f', "Closing file sector: %u \n", sector);
    openFileList->Acquire();
    int instances = openFileList->CloseOpenFile(sector);
    bool toBeRemoved = openFileList->GetToBeRemoved(sector);
    openFileList->Release();
    // Si la cantidad de instancias es menor a 0, el archivo no existe al momento de cerrarlo.
    if (instances < 0) {
        return false;
    }
    if (instances != 0) {
        return true;
    }
    // Si toBeRemoved es false, retornamos que el archivo fue cerrado correctamente.
    if (!toBeRemoved) {
        return true;
    }

    // Si toBeRemoved es true, se remueve de la lista de procesos abiertos y se borra del disco.
    openFileList->Acquire();
    openFileList->RemoveOpenFile(sector);
    openFileList->Release();

    return DeleteFromDisk(sector);
}



/// List all the files in the file system directory.
void
FileSystem::List()
{
    Directory *dir = new Directory(NUM_DIR_ENTRIES);

    dir->FetchFrom(directoryFile);
    dir->List();
    delete dir;
}

static bool
AddToShadowBitmap(unsigned sector, Bitmap *map)
{
    ASSERT(map != nullptr);

    if (map->Test(sector)) {
        DEBUG('f', "Sector %u was already marked.\n", sector);
        return false;
    }
    map->Mark(sector);
    DEBUG('f', "Marked sector %u.\n", sector);
    return true;
}

static bool
CheckForError(bool value, const char *message)
{
    if (!value) {
        DEBUG('f', "Error: %s\n", message);
    }
    return !value;
}

static bool
CheckSector(unsigned sector, Bitmap *shadowMap)
{
    if (CheckForError(sector < NUM_SECTORS,
                      "sector number too big.  Skipping bitmap check.")) {
        return true;
    }
    return CheckForError(AddToShadowBitmap(sector, shadowMap),
                         "sector number already used.");
}

static bool
CheckFileHeader(const RawFileHeader *rh, unsigned num, Bitmap *shadowMap)
{
    ASSERT(rh != nullptr);

    bool error = false;

    DEBUG('f', "Checking file header %u.  File size: %u bytes, number of sectors: %u.\n",
          num, rh->numBytes, rh->numSectors);
    error |= CheckForError(rh->numSectors >= DivRoundUp(rh->numBytes,
                                                        SECTOR_SIZE),
                           "sector count not compatible with file size.");
    error |= CheckForError(rh->numSectors < NUM_DIRECT,
                           "too many blocks.");
    for (unsigned i = 0; i < rh->numSectors; i++) {
        unsigned s = rh->dataSectors[i];
        error |= CheckSector(s, shadowMap);
    }
    return error;
}

static bool
CheckBitmaps(const Bitmap *freeMap, const Bitmap *shadowMap)
{
    bool error = false;
    for (unsigned i = 0; i < NUM_SECTORS; i++) {
        DEBUG('f', "Checking sector %u. Original: %u, shadow: %u.\n",
              i, freeMap->Test(i), shadowMap->Test(i));
        error |= CheckForError(freeMap->Test(i) == shadowMap->Test(i),
                               "inconsistent bitmap.");
    }
    return error;
}

static bool
CheckDirectory(const RawDirectory *rd, Bitmap *shadowMap)
{
    ASSERT(rd != nullptr);
    ASSERT(shadowMap != nullptr);

    bool error = false;
    unsigned nameCount = 0;
    const char *knownNames[NUM_DIR_ENTRIES];

    for (unsigned i = 0; i < NUM_DIR_ENTRIES; i++) {
        DEBUG('f', "Checking direntry: %u.\n", i);
        const DirectoryEntry *e = &rd->table[i];

        if (e->inUse) {
            if (strlen(e->name) > FILE_NAME_MAX_LEN) {
                DEBUG('f', "Filename too long.\n");
                error = true;
            }

            // Check for repeated filenames.
            DEBUG('f', "Checking for repeated names.  Name count: %u.\n",
                  nameCount);
            bool repeated = false;
            for (unsigned j = 0; j < nameCount; j++) {
                DEBUG('f', "Comparing \"%s\" and \"%s\".\n",
                      knownNames[j], e->name);
                if (strcmp(knownNames[j], e->name) == 0) {
                    DEBUG('f', "Repeated filename.\n");
                    repeated = true;
                    error = true;
                }
            }
            if (!repeated) {
                knownNames[nameCount] = e->name;
                DEBUG('f', "Added \"%s\" at %u.\n", e->name, nameCount);
                nameCount++;
            }

            // Check sector.
            error |= CheckSector(e->sector, shadowMap);

            // Check file header.
            FileHeader *h = new FileHeader;
            const RawFileHeader *rh = h->GetRaw();
            h->FetchFrom(e->sector);
            error |= CheckFileHeader(rh, e->sector, shadowMap);
            delete h;
        }
    }
    return error;
}

bool
FileSystem::Check()
{
    DEBUG('f', "Performing filesystem check\n");
    bool error = false;

    Bitmap *shadowMap = new Bitmap(NUM_SECTORS);
    shadowMap->Mark(FREE_MAP_SECTOR);
    shadowMap->Mark(DIRECTORY_SECTOR);

    DEBUG('f', "Checking bitmap's file header.\n");

    FileHeader *bitH = new FileHeader;
    const RawFileHeader *bitRH = bitH->GetRaw();
    bitH->FetchFrom(FREE_MAP_SECTOR);
    DEBUG('f', "  File size: %u bytes, expected %u bytes.\n"
               "  Number of sectors: %u, expected %u.\n",
          bitRH->numBytes, FREE_MAP_FILE_SIZE,
          bitRH->numSectors, FREE_MAP_FILE_SIZE / SECTOR_SIZE);
    error |= CheckForError(bitRH->numBytes == FREE_MAP_FILE_SIZE,
                           "bad bitmap header: wrong file size.");
    error |= CheckForError(bitRH->numSectors == FREE_MAP_FILE_SIZE / SECTOR_SIZE,
                           "bad bitmap header: wrong number of sectors.");
    error |= CheckFileHeader(bitRH, FREE_MAP_SECTOR, shadowMap);
    delete bitH;

    DEBUG('f', "Checking directory.\n");

    FileHeader *dirH = new FileHeader;
    const RawFileHeader *dirRH = dirH->GetRaw();
    dirH->FetchFrom(DIRECTORY_SECTOR);
    error |= CheckFileHeader(dirRH, DIRECTORY_SECTOR, shadowMap);
    delete dirH;

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    const RawDirectory *rdir = dir->GetRaw();
    dir->FetchFrom(directoryFile);
    error |= CheckDirectory(rdir, shadowMap);
    delete dir;

    // The two bitmaps should match.
    DEBUG('f', "Checking bitmap consistency.\n");
    error |= CheckBitmaps(freeMap, shadowMap);
    delete shadowMap;
    delete freeMap;

    DEBUG('f', error ? "Filesystem check failed.\n"
                     : "Filesystem check succeeded.\n");

    return !error;
}

/// Print everything about the file system:
/// * the contents of the bitmap;
/// * the contents of the directory;
/// * for each file in the directory:
///   * the contents of the file header;
///   * the data in the file.
void
FileSystem::Print()
{
    FileHeader *bitH    = new FileHeader;
    FileHeader *dirH    = new FileHeader;
    Bitmap     *freeMap = new Bitmap(NUM_SECTORS);
    Directory  *dir     = new Directory(NUM_DIR_ENTRIES);

    /*
    printf("--------------------------------\n");
    bitH->FetchFrom(FREE_MAP_SECTOR);
    bitH->Print("Bitmap");
    printf("--------------------------------\n");
    dirH->FetchFrom(DIRECTORY_SECTOR);
    dirH->Print("Directory");
    */

    printf("--------------------------------\n");
    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();

    printf("--------------------------------\n");
    dir->FetchFrom(directoryFile);
    dir->Print();
    printf("--------------------------------\n");

    delete bitH;
    delete dirH;
    delete freeMap;
    delete dir;
}

Bitmap *
FileSystem::AcquireFreeMap()
{
    freeMapLock->Acquire();

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);

    return freeMap;
}

Bitmap*
FileSystem::GetCurrentFreeMap()
{
    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);

    return freeMap;
}

/// Marks the end of the freeMap usage. The lock is released, the
/// memory is freed and the changes are saved to disk.
void
FileSystem::ReleaseFreeMap(Bitmap *freeMap_)
{
    freeMap_->WriteBack(freeMapFile);
    delete freeMap_;

    freeMapLock -> Release();
}

// resolvedor de paths -> dir indicado
// se le pasa el inicio
int 
FileSystem::PathResolver(char *path, unsigned tempDirectory)
{
    char ini[FILE_NAME_MAX_LEN] = {0}, *rest;
    if (strchr(path, '/') == nullptr) {
        return tempDirectory;
    }

    int len = (strchr(path, '/') - path) * sizeof(char);
    strncpy(ini, path, len);
    rest = path + len + 1;

    // Encontramos / como primer caracter.
    if (strcmp(ini, "") == 0) {
        return PathResolver(rest, root);
    } else {
        Directory *dir = new Directory(NUM_DIR_ENTRIES);
        OpenFile *CDOpenFile = new OpenFile(tempDirectory);
        dir->FetchFrom(CDOpenFile);
        if (strcmp(ini, "..") == 0) {
            int parent = dir->FindDir("..");
            delete dir;
            delete CDOpenFile;
            return PathResolver(rest, parent);
        } else {
            int sector = dir->FindDir(ini);
            if (sector >= 0) {
                delete dir;
                delete CDOpenFile;
                return PathResolver(rest, sector);
            } 
            return -1;
        }
    }
}

char *
FileSystem::GetAfterLastSlash(const char* path)
{
    if (strrchr(path, '/') == nullptr) {
        return (char*) path;
    }

    return (char *) strrchr(path, '/') + 1;
}

bool
FileSystem::CD(const char *path)
{
    ASSERT(path != nullptr);

    DEBUG('f', "Moving to %s", path);

    // empieza con / entonces es absoluto
    // sino es desde el dir actual (relativa).
    // tambien name puede ser ..
    // usr/loot/pedro

    int newDirectory = PathResolver((char *) path, currentThread->currentDirectory);

    DEBUG('f', "Directory given by PathResolver %i\n", newDirectory);
    if (newDirectory == -1) {
        return false;
    }

    if(strcmp(GetAfterLastSlash(path), "") == 0) {
        currentThread->currentDirectory = newDirectory;
        return true;
    }

    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    OpenFile *CDOpenFile = new OpenFile(newDirectory);
    dir->FetchFrom(CDOpenFile);
    newDirectory = dir->FindDir(GetAfterLastSlash(path));
    
    currentThread->currentDirectory = newDirectory;
    delete dir;
    delete CDOpenFile;
    return true;
}

char *
FileSystem::MoveToDirectory(const char* path)
{
    int newDirectory = PathResolver((char *)path, currentThread->currentDirectory);
    if (newDirectory == -1) {
        DEBUG('f', "Incorrect path\n");
        return nullptr;
    }
    currentThread->currentDirectory = newDirectory;
    char* name = GetAfterLastSlash(path);

    if (name == nullptr || strcmp(name, "..") == 0) {
        return nullptr;
    }

    return name;
}