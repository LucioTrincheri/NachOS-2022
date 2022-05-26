/// Entry points into the Nachos kernel from user programs.
///
/// There are two kinds of things that can cause control to transfer back to
/// here from user code:
///
/// * System calls: the user code explicitly requests to call a procedure in
///   the Nachos kernel.  Right now, the only function we support is `Halt`.
///
/// * Exceptions: the user code does something that the CPU cannot handle.
///   For instance, accessing memory that does not exist, arithmetic errors,
///   etc.
///
/// Interrupts (which can also cause control to transfer from user code into
/// the Nachos kernel) are handled elsewhere.
///
/// For now, this only handles the `Halt` system call.  Everything else core-
/// dumps.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "transfer.hh"
#include "syscall.h"
#include "filesys/directory_entry.hh"
#include "filesys/open_file.hh"
#include "threads/system.hh"
#include "executable.hh"
#include "address_space.hh"
#include "threads/lock.hh"
#include "args.hh"
#include "synch_console.hh"
#include "machine.hh"
static SynchConsole *synchConsole = nullptr;

#include <stdio.h>

//TODO poner
#define RETURN(value) machine->WriteRegister(2, value);

void InitSynchConsole() {
    if (synchConsole == nullptr) {
        synchConsole = new SynchConsole(nullptr, nullptr);
    };
}

static void
IncrementPC()
{
    unsigned pc;

    pc = machine->ReadRegister(PC_REG);
    machine->WriteRegister(PREV_PC_REG, pc);
    pc = machine->ReadRegister(NEXT_PC_REG);
    machine->WriteRegister(PC_REG, pc);
    pc += 4;
    machine->WriteRegister(NEXT_PC_REG, pc);
}

/// Do some default behavior for an unexpected exception.
///
/// NOTE: this function is meant specifically for unexpected exceptions.  If
/// you implement a new behavior for some exception, do not extend this
/// function: assign a new handler instead.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
static void
DefaultHandler(ExceptionType et)
{
    int exceptionArg = machine->ReadRegister(2);

    fprintf(stderr, "Unexpected user mode exception: %s, arg %d.\n",
            ExceptionTypeToString(et), exceptionArg);
    ASSERT(false);
}

void
RunProgram(void *argsParentThread)
{
    currentThread->space->InitRegisters();
    currentThread->space->RestoreState();
    
    unsigned userArgc = 0;
    int userArgv = 0;
    // Si se pasaron argumentos al programa, los escribimos en el stack
    // y obtenemos su direccion de memoria correspondiente a cada arg.
    if (argsParentThread != nullptr) {
        // Al escribir los argumentos pasados del padre en nuevo thread,
        // obtenemos la cantidad.
        userArgc = WriteArgs((char **)argsParentThread);
        userArgv = machine->ReadRegister(STACK_REG);
    }

    // Ahora, para correr el programa de usuario como un programa normal,
    // escribimos los valores argc y argv como son usuales en un programa.
    // Cantidad de argumentos
    machine->WriteRegister(4, userArgc);
    // Direccion de memoria de los argumentos
    machine->WriteRegister(5, userArgv);

    // Para pasar los argumentos, debemos restar 24 al STACK_REG ya que
    // ocupamos lugar escribiendo los argumentos en el STACK.
    int sp = machine->ReadRegister(STACK_REG);
    machine->WriteRegister(STACK_REG, sp - 24);
    
    machine->Run();
}

/// Handle a system call exception.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
///
/// The calling convention is the following:
///
/// * system call identifier in `r2`;
/// * 1st argument in `r4`;
/// * 2nd argument in `r5`;
/// * 3rd argument in `r6`;
/// * 4th argument in `r7`;
/// * the result of the system call, if any, must be put back into `r2`.
///
/// And do not forget to increment the program counter before returning. (Or
/// else you will loop making the same system call forever!)
static void
SyscallHandler(ExceptionType _et)
{
    int scid = machine->ReadRegister(2);

    switch (scid) {

        case SC_HALT:
            DEBUG('e', "Shutdown, initiated by user program.\n");
            interrupt->Halt();
            break;

        case SC_CREATE: {

            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr, filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n", FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, -1);
                break;
            }
            DEBUG('e', "`Create` requested for file `%s`.\n", filename);

            if (fileSystem->Create(filename, 0)) {
                DEBUG('e', "File created `%s`.\n", filename);
                machine->WriteRegister(2, 0);
            } else {
                DEBUG('e', "Error: could not create file `%s`.\n", filename);
                machine->WriteRegister(2, -1);
            };

            break;
        }

        case SC_REMOVE: {
            DEBUG('e', "`Remove` requested\n");

            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr, filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n", FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, -1);
                break;
            }

            if (fileSystem->Remove(filename)) {
                DEBUG('e', "Remove file `%s`.\n", filename);
                // Se retorna 0
                machine->WriteRegister(2, 0);
            } else {
                DEBUG('e', "Error: could not remove file `%s`.\n", filename);
                // Se retorna -1
                machine->WriteRegister(2, -1);
            };

            break;
        }

        case SC_EXIT: {
            int status = machine->ReadRegister(4);
            DEBUG('e', "`Exit` requested with code %d.\n", status);
            
            if (currentThread->space != nullptr) {
                delete currentThread->space;
                currentThread->space = nullptr;
            }

            currentThread->Finish(status); // Esto pone al thread como threadToBeDestroyed, lo cual el scheduler llama a ~Thread, lo cual libera el stack.
            ASSERT(false);
        }

        case SC_READ: {
            // int Read(char *buffer, int size, OpenFileId id);
            DEBUG('e', "`Read` requested.\n");
            int bufferAddr = machine->ReadRegister(4);
            int bufferSize = machine->ReadRegister(5);

            if (bufferAddr == 0) {
                DEBUG('e', "Error: address to buffer is null.\n");
                machine->WriteRegister(2, -1);
                break;
            }
            if (bufferSize < 0) {
                DEBUG('e', "Error: size to read is negative.\n");
                machine->WriteRegister(2, -1);
                break;
            }
            OpenFileId fileId = machine->ReadRegister(6);
            if (fileId < 0) {
                DEBUG('e', "Error: OpenFileId is negative.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            if (fileId == CONSOLE_OUTPUT) {
                DEBUG('e', "Error: OpenFileId is CONSOLE_OUTPUT. Trying to read from output.\n");
                machine->WriteRegister(2, -1);
                break;
            }
            if (!currentThread->HasOpenFileId(fileId)) {
                DEBUG('e', "Error: Not exists open file with the given file id.\n");
                machine->WriteRegister(2, -1);
                break;
            }
            
            char buffer[bufferSize + 1];
            int sizeRead = 0;

            if (fileId == CONSOLE_INPUT) {
                InitSynchConsole();
                do {
                    char c = synchConsole->GetChar();
                    buffer[sizeRead++] = c;
                } while (sizeRead < bufferSize /*No se sabe si es necesario checkear por EOF o similares*/);

            } else {
                OpenFile *openFile = currentThread->GetOpenFile(fileId);
                sizeRead = openFile->Read(buffer, bufferSize);
            }

            if (sizeRead > 0){
                WriteBufferToUser(buffer, bufferAddr, bufferSize);
            } 
            // En este punto no puede haber ningun error, por lo que se devulve siempre el largo leido.
            machine->WriteRegister(2, sizeRead);
            break;
        }
        
        case SC_WRITE: {
            // int Write(const char *buffer, int size, OpenFileId id);
            DEBUG('e', "`Write` requested.\n");
            int bufferAddr = machine->ReadRegister(4);
            int bufferSize = machine->ReadRegister(5);

            if (bufferAddr == 0) {
                DEBUG('e', "Error: address to buffer is null.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            if (bufferSize < 0) {
                DEBUG('e', "Error: size to write is negative.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            OpenFileId fileId = machine->ReadRegister(6);
            if (fileId < 0) {
                DEBUG('e', "Error: OpenFileId is negative.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            if (fileId == CONSOLE_INPUT) {
                DEBUG('e', "Error: OpenFileId is CONSOLE_INPUT. Trying to write in input.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            if (!currentThread->HasOpenFileId(fileId)) {
                DEBUG('e', "Error: Not exists open file with the given file id.\n");
                machine->WriteRegister(2, -1);
                break;
            }
            
            char buffer[bufferSize + 1];
            ReadBufferFromUser(bufferAddr, buffer, sizeof buffer);

            int sizeWrite = 0;
            if (fileId == CONSOLE_OUTPUT) {
                InitSynchConsole();
                do {
                    synchConsole->PutChar(buffer[sizeWrite++]);
                } while (sizeWrite < bufferSize /*No se sabe si es necesario checkear por EOF o similares*/);

            } else {
                OpenFile *openFile = currentThread->GetOpenFile(fileId);
                sizeWrite = openFile->Write(buffer, bufferSize);
            }
             
            // En este punto no puede haber ningun error, por lo que se devulve siempre el largo escrito.
            machine->WriteRegister(2, sizeWrite);
            break;
            
        }

        case SC_OPEN: {
            DEBUG('e', "`Open` requested.\n");
            int fileNameAddr = machine->ReadRegister(4);

            if (fileNameAddr == 0) {
                DEBUG('e', "Error: File name address invalid.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            char fileName[FILE_NAME_MAX_LEN + 1];

            if (!ReadStringFromUser(fileNameAddr, fileName, sizeof fileName)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n", FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, -1);
                break;
            }
            DEBUG('e', "Request to open %s.\n", fileName);
            
            OpenFile* openFile = fileSystem->Open(fileName);

            if (!openFile) {
                DEBUG('e', "Error: File does not exist.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            OpenFileId fileId = currentThread->StoreOpenFile(openFile);
            
            if (fileId < 0) {
                DEBUG('e', "Error: Could not open the file.\n");
                delete openFile;
                machine->WriteRegister(2, -1);
                break;
            } else {
                machine->WriteRegister(2, fileId);
            }

            break;

        }
        case SC_CLOSE: {
            int fid = machine->ReadRegister(4);
            DEBUG('e', "`Close` requested for id %u.\n", fid);
            break;

            if (fid < 0) {
                DEBUG('e', "Error: FileId does is negative.\n");
                machine->WriteRegister(2, -1);
                break;
            }
            
            if (currentThread->RemoveOpenFile(fid)) {
                machine->WriteRegister(2, 0);
            } else {
                DEBUG('e', "Error: Could not close the file or file not open.\n");
                machine->WriteRegister(2, -1);
                break;
            }
            break;
        }

        case SC_JOIN: {
            DEBUG('e', "`Join` requested.\n");
            SpaceId id = machine->ReadRegister(4);
            if (id < 0) {
                DEBUG('e', "Error: Invalid process id.\n");
                machine->WriteRegister(2, -1);
                break;
            }
            
            userThreadsLock->Acquire();
            Thread *thread = userThreads->Get(id);
            userThreadsLock->Release();

            if (thread == nullptr) {
                DEBUG('e', "Error: Invalid user thread.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            int returnValue = thread->Join();
            DEBUG('e', "Thread joined\n");
            machine->WriteRegister(2, returnValue);
            break;
        }

        case SC_EXEC: {
            // SpaceId Exec(char *name);
            DEBUG('e', "`Exec` requested.\n");
            int filenameAddr = machine->ReadRegister(4);
            
            if (filenameAddr == 0) {
                DEBUG('e', "Error: File name address invalid.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr, filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n", FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, -1);
                break;
            }
            DEBUG('e', "File read: %s\n", filename);
            OpenFile* openFile = fileSystem->Open(filename);

            if (!openFile) {
                DEBUG('e', "Error: File does not exist.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            // Esto puede llegar a ser en rara ocasión un problema.
            Executable exe (openFile);
            if (!exe.CheckMagic()) {
                DEBUG('e', "Error: File is not an executable. %s\n", filename);
                machine->WriteRegister(2, -1);
                break;
            }

            // userThreadsLock->Acquire(); // Puede ser necesario para que otro user thread no cambie full memory. Falso
            AddressSpace *addrSpc = new AddressSpace(openFile); //Puede ser que falle si no hay mas memoria fisica.
            // delete openFile;

            if (addrSpc->fullMemory) {
                DEBUG('e', "Error: Insufficient memory size for address space.\n");
                machine->WriteRegister(2, -1);
                //delete openFile;
                delete addrSpc; // Puede no ser necesario
                break;
            }
            // pasamos en exec un argumento mas que es si es joineable o no el thread.
            bool joinable = machine->ReadRegister(6);
            Thread *thread = new Thread("filename", joinable, 0);

            userThreadsLock->Acquire();
            int pid = userThreads->Add(thread);

            if (pid == -1){
                DEBUG('e', "Error: Too many processes.\n");
                machine->WriteRegister(2, -1);
                delete thread;
                delete addrSpc;
                break;
            }

            thread->space = addrSpc;
            userThreadsLock->Release();
            
            char **argv;
            int args = machine->ReadRegister(5);
            if (args != 0) {
                argv = SaveArgs(args);
            }
            thread->Fork(&RunProgram, (void *)argv);
            machine->WriteRegister(2, pid);

            break;
        }

        case SC_PS: {
            DEBUG('e', "`Ps` requested.\n");
            scheduler->Print(); // No se que tan correcto es esto
            break;
        }

        default:
            fprintf(stderr, "Unexpected system call: id %d.\n", scid);
            ASSERT(false);

    }

    IncrementPC();
}

unsigned int getVPN (int vaddr)
{
    return (unsigned) vaddr / PAGE_SIZE;
}

// indice para la FIFO de la TLB
static unsigned int iTLB = 0;

static void
PageFaultHandler(ExceptionType _et) {

    stats->TLBMisses ++;
    DEBUG('p', "TBLMisses plus one in PageHandler\n");
	// rellenar la TLB con una entrada validad para la pagina quefallo
    DEBUG('p', "%s\n", ExceptionTypeToString(_et));

	// vpn en el registro BadVAddr
	int vaddr = machine->ReadRegister(BAD_VADDR_REG);
	unsigned int vpn = getVPN(vaddr); // sacarle el tamaño del desplazamiento.

	// para saber cual i hago FIFO
    if (currentThread->space->GetPageTable()[vpn].physicalPage == -1) {
        DEBUG('p', "Must be -1: %d\n", currentThread->space->GetPageTable()[vpn].physicalPage);
        currentThread->space->LoadPage(vpn);
    }
    DEBUG('p', "Physical page addr: %d\n", currentThread->space->GetPageTable()[vpn].physicalPage);
	machine->GetMMU()->tlb[iTLB++%TLB_SIZE] = currentThread->space->GetPageTable()[vpn];
}

// TODO Check this
static void ReadOnlyHandler(ExceptionType _et)
{
    DEBUG('e', "Tried to read from a read only page");

    ASSERT(false);
    return;
}


/// By default, only system calls have their own handler.  All other
/// exception types are assigned the default handler.
void
SetExceptionHandlers()
{
    machine->SetHandler(NO_EXCEPTION,            &DefaultHandler);
    machine->SetHandler(SYSCALL_EXCEPTION,       &SyscallHandler);
    machine->SetHandler(PAGE_FAULT_EXCEPTION,    &PageFaultHandler);
    machine->SetHandler(READ_ONLY_EXCEPTION,     &ReadOnlyHandler);
    machine->SetHandler(BUS_ERROR_EXCEPTION,     &DefaultHandler);
    machine->SetHandler(ADDRESS_ERROR_EXCEPTION, &DefaultHandler);
    machine->SetHandler(OVERFLOW_EXCEPTION,      &DefaultHandler);
    machine->SetHandler(ILLEGAL_INSTR_EXCEPTION, &DefaultHandler);
}
