/// Routines for managing the disk file header (in UNIX, this would be called
/// the i-node).
///
/// The file header is used to locate where on disk the file's data is
/// stored.  We implement this as a fixed size table of pointers -- each
/// entry in the table points to the disk sector containing that portion of
/// the file data (in other words, there are no indirect or doubly indirect
/// blocks). The table size is chosen so that the file header will be just
/// big enough to fit in one disk sector,
///
/// Unlike in a real system, we do not keep track of file permissions,
/// ownership, last modification date, etc., in the file header.
///
/// A file header can be initialized in two ways:
///
/// * for a new file, by modifying the in-memory data structure to point to
///   the newly allocated data blocks;
/// * for a file already on disk, by reading the file header from disk.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_header.hh"
#include "threads/system.hh"

#include <ctype.h>
#include <stdio.h>


/// Initialize a fresh file header for a newly created file.  Allocate data
/// blocks for the file out of the map of free disk blocks.  Return false if
/// there are not enough free blocks to accomodate the new file.
///
/// * `freeMap` is the bit map of free disk sectors.
/// * `fileSize` is the bit map of free disk sectors.
bool
FileHeader::Allocate(Bitmap *freeMap, unsigned fileSize)
{
    ASSERT(freeMap != nullptr);

    if (fileSize > MAX_FILE_SIZE_W_INDIR) {
        return false;
    }

    raw.numBytes = fileSize;

    // Cantidad de sectores de data
    unsigned dataSectorCount = DivRoundUp(raw.numBytes, SECTOR_SIZE);
    // Cantidad de sectores dedicados a indirección. El maximo es la cantidad de sectores que se pueden direccionar desde un header (NUM_DIRECT).
    unsigned indirectionSectorCount = (raw.numBytes <= MAX_FILE_SIZE) ? 0 : DivRoundUp(dataSectorCount, NUM_DIRECT);

    // raw.numSectors mayor a NUM_DIRECT si hay indireccion.
    raw.numSectors = dataSectorCount + indirectionSectorCount;

	// Si no hay suficiente sectores o el tamaño del archivo es mayor al tamaño máximo con indirección.
    if (freeMap->CountClear() < raw.numSectors or MAX_FILE_SIZE_W_INDIR < fileSize)
        return false;  // Not enough space.

	// memset(raw.dataSectors, 0, sizeof(unsigned) * NUM_DIRECT); // ? No deberia ser necesario

    // En este caso raw.numBytes = dataSectorCount <= y no hay indirección.
    if(raw.numBytes <= MAX_FILE_SIZE){
        for (unsigned i = 0; i < raw.numSectors; i++)
            raw.dataSectors[i] = freeMap->Find();
    // En este caso raw.numBytes = dataSectorCount + indirectionSectorCount, por lo que hacemos indirección.
    indirTable = std::vector<FileHeader*>(indirectionSectorCount);
    }else{
        // Amount of bytes that still have to be allocated.
        unsigned remainingBytes = raw.numBytes;

        // Allocate the header tables.
        for(unsigned i = 0; i < indirectionSectorCount; i++){
            raw.dataSectors[i] = freeMap->Find();
            FileHeader *dataHeader = new FileHeader;

            // nextBlock is the amount of bytes the current FileHeader will
            // store.
            unsigned nextBlock;
            if(remainingBytes <= MAX_FILE_SIZE)
                nextBlock = remainingBytes;
            else{
                // Allocate as many bytes as possible.
                nextBlock = MAX_FILE_SIZE;
                remainingBytes -= MAX_FILE_SIZE;
            }

            dataHeader->Allocate(freeMap, nextBlock);
            // Save the new FileHeader to the indirTable
            indirTable.push_back(dataHeader);
        }
    }

    return true;
}

/// De-allocate all the space allocated for data blocks for this file.
///
/// * `freeMap` is the bit map of free disk sectors.
void
FileHeader::Deallocate(Bitmap *freeMap)
{
    ASSERT(freeMap != nullptr);

    // Por cada FileHeader en la tabla de indirección, llamo a Deallocate otra vez.
    for(FileHeader* fh : indirTable){
        fh->Deallocate(freeMap);
        delete fh;
    }
    indirTable.clear();

    // Borro los sectores que pertenezcan a este nivel de indirección.
    unsigned sectorLimit;
    if(raw.numBytes > MAX_FILE_SIZE)
        sectorLimit = (raw.numBytes <= MAX_FILE_SIZE) ? 0 : DivRoundUp(DivRoundUp(raw.numBytes, SECTOR_SIZE), NUM_DIRECT); //? Ver como hacer con lo de los DivRoundUp
    else
        sectorLimit = raw.numSectors;

    for (unsigned i = 0; i < sectorLimit; i++) {
            ASSERT(freeMap->Test(raw.dataSectors[i]));
            freeMap->Clear(raw.dataSectors[i]);
    }
}

/// Fetch contents of file header from disk.
///
/// * `sector` is the disk sector containing the file header.
void
FileHeader::FetchFrom(unsigned sector)
{
    // Leo del nivel actual de indirección.
    synchDisk->ReadSector(sector, (char *) &raw);

    unsigned indirectionSectorCount = (raw.numBytes <= MAX_FILE_SIZE) ? 0 : DivRoundUp(DivRoundUp(raw.numBytes, SECTOR_SIZE), NUM_DIRECT);
    indirTable = std::vector<FileHeader*>(indirectionSectorCount);

    // Leo todo los FileHeaders del siguiente nivel de indirección.
    for(unsigned i = 0; i < indirectionSectorCount; i++){
        FileHeader* dataHeader = new FileHeader;
        dataHeader->FetchFrom(raw.dataSectors[i]);
        indirTable[i] = dataHeader;
    }
}

/// Write the modified contents of the file header back to disk.
///
/// * `sector` is the disk sector to contain the file header.
void
FileHeader::WriteBack(unsigned sector)
{
    // Guardo el nivel actual de indirección.
    synchDisk->WriteSector(sector, (char *) &raw);

    // Guardo todo los FileHeaders del siguiente nivel de indirección.
    for(unsigned i = 0; i < indirTable.size(); i++)
        indirTable[i]->WriteBack(raw.dataSectors[i]);
}



/// Return which disk sector is storing a particular byte within the file.
/// This is essentially a translation from a virtual address (the offset in
/// the file) to a physical address (the sector where the data at the offset
/// is stored).
///
/// * `offset` is the location within the file of the byte in question.
unsigned
FileHeader::ByteToSector(unsigned offset)
{
    if(raw.numBytes > MAX_FILE_SIZE){
        unsigned index = offset/MAX_FILE_SIZE;
        return indirTable[index]->ByteToSector(offset % MAX_FILE_SIZE);
    }else
        return raw.dataSectors[offset/SECTOR_SIZE];
}


/// Return the number of bytes in the file.
unsigned
FileHeader::FileLength() const
{
    return raw.numBytes;
}

/// Print the contents of the file header, and the contents of all the data
/// blocks pointed to by the file header.
void
FileHeader::Print(const char *title)
{
    char *data = new char [SECTOR_SIZE];

    if (title == nullptr) {
        printf("File header:\n");
    } else {
        printf("%s file header:\n", title);
    }

    printf("    size: %u bytes\n"
           "    block indexes: ",
           raw.numBytes);

    for (unsigned i = 0; i < raw.numSectors; i++) {
        printf("%u ", raw.dataSectors[i]);
    }
    printf("\n");

    for (unsigned i = 0, k = 0; i < raw.numSectors; i++) {
        printf("    contents of block %u:\n", raw.dataSectors[i]);
        synchDisk->ReadSector(raw.dataSectors[i], data);
        for (unsigned j = 0; j < SECTOR_SIZE && k < raw.numBytes; j++, k++) {
            if (isprint(data[j])) {
                printf("%c", data[j]);
            } else {
                printf("\\%X", (unsigned char) data[j]);
            }
        }
        printf("\n");
    }
    delete [] data;
}

RawFileHeader *
FileHeader::GetRaw()
{
    return &raw;
}

void
FileHeader::SetRaw(RawFileHeader nRaw) {
    raw = nRaw;
    indirTable = std::vector<FileHeader*>(0);

}


int min (int a, int b) {
    if (a < b) {
        return a;
    } else {
        return b;
    }
}

bool
FileHeader::Extend(Bitmap *freeMap, unsigned extendSize){
    if(extendSize == 0){
        return true;
    }
    // Variables viejas
    unsigned oldNumBytes = raw.numBytes;
    unsigned oldNumSectors = raw.numSectors;

    // Nuevas variables
    raw.numBytes += extendSize;

    // Cantidad de sectores de data
    unsigned dataSectorCount = DivRoundUp(raw.numBytes, SECTOR_SIZE);
    // Cantidad de sectores dedicados a indirección. El maximo es la cantidad de sectores que se pueden direccionar desde un header (NUM_DIRECT).
    unsigned indirectionSectorCount = (raw.numBytes <= MAX_FILE_SIZE) ? 0 : DivRoundUp(dataSectorCount, NUM_DIRECT);
    // raw.numSectors mayor a NUM_DIRECT si hay indireccion.
    raw.numSectors = dataSectorCount + indirectionSectorCount;

	// Si no hay suficiente sectores o el tamaño del archivo es mayor al tamaño máximo con indirección.
    if (freeMap->CountClear() < raw.numSectors - oldNumSectors or MAX_FILE_SIZE_W_INDIR < raw.numBytes){
        raw.numBytes = oldNumBytes;
        raw.numSectors = oldNumSectors;
        return false;  // Not enough space.
    }

    unsigned remainingBytes = extendSize;
    // No habia indireccion anteriormente
    if (oldNumBytes <= MAX_FILE_SIZE) {
        // Aun entra en el fileHeader actual
        if (raw.numBytes <= MAX_FILE_SIZE) {
            for (unsigned i = oldNumSectors; i < raw.numSectors; i++) {
                raw.dataSectors[i] = freeMap->Find();
            }
            remainingBytes = 0; // Entra en el header actual, queda en 0.
        // Si no entra en el FileHeader actual, hay que hacer indirección.
        } else {
            unsigned freeBytesOfLastSector = SECTOR_SIZE - (oldNumBytes % SECTOR_SIZE);
            remainingBytes -= freeBytesOfLastSector;
            for (unsigned i = oldNumSectors; i < NUM_DIRECT; i++){
                raw.dataSectors[i] = freeMap->Find();
                remainingBytes -= SECTOR_SIZE;
            }
            // El header actual esta lleno, pero faltan bytes. Hay que crear un nuevo header igual al actual, vaciar el actual y usarlo como FileHeader de indireccion.
            // Nuevo FileHeader copia
            FileHeader *fh = new FileHeader;
            // Copio los contenidos del actual a la copia y coloco los valores correctos.
            fh->SetRaw(raw);
            RawFileHeader* rfh = fh->GetRaw();
            rfh->numBytes = MAX_FILE_SIZE;
            rfh->numSectors = NUM_DIRECT;
            // Actualizo los valores del FileHeader para que actue como indireccion.
            raw.dataSectors[0] = freeMap->Find();
            indirTable.push_back(fh);
            //indirTable[0] = fh;
        }
    }

    // Ahora, con indirección se expande para poder almacenar la cantidad de bytes restante.
    if (remainingBytes > 0) {
        FileHeader *fh = indirTable[indirTable.size() - 1];
        RawFileHeader *rfh = fh->GetRaw();

        unsigned freeBytesInLastTable = MAX_FILE_SIZE - rfh->numBytes;

        if (freeBytesInLastTable != 0) {
            fh->Extend(freeMap, std::min(remainingBytes, freeBytesInLastTable));
        }

        remainingBytes -= freeBytesInLastTable;

        if (remainingBytes > 0) {
            for (unsigned i = indirTable.size(); i < indirectionSectorCount; i++){
                raw.dataSectors[i] = freeMap->Find();
                FileHeader *dataHeader = new FileHeader;

                // nextBlock is the amount of bytes the current FileHeader will
                // store.
                unsigned nextBlock;
                if(remainingBytes <= MAX_FILE_SIZE)
                    nextBlock = remainingBytes;
                else{
                    // Allocate as many bytes as possible.
                    nextBlock = MAX_FILE_SIZE;
                    remainingBytes -= MAX_FILE_SIZE;
                }

                dataHeader->Allocate(freeMap, nextBlock);
                // Save the new FileHeader to the indirTable
                indirTable.push_back(dataHeader);
            }
        }
    }

    return true;
}
