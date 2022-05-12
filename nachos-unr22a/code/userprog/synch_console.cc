/// Use a semaphore to synchronize the interrupt handlers with the pending
/// requests.  And, because the physical disk can only handle one operation
/// at a time, use a lock to enforce mutual exclusion.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "synch_console.hh"

static void ConsoleReadAvail(void *arg) {
    ASSERT(arg != nullptr);
    SynchConsole *sc = (SynchConsole *) arg;
    sc->ReadAvail();
};

static void ConsoleWriteDone(void *arg) {
    ASSERT(arg != nullptr);
    SynchConsole *sc = (SynchConsole *) arg;
    sc->WriteDone();
};


SynchConsole::SynchConsole(const char *in, const char *out)
{
    readSemaphore = new Semaphore("read", 0);
    writeSemaphore = new Semaphore("write", 0);
    lock = new Lock("lock");
    console = new Console(in, out, ConsoleReadAvail, ConsoleWriteDone, this);
}

SynchConsole::~SynchConsole()
{
    delete console;
    delete lock;
    delete readSemaphore;
    delete writeSemaphore;
}

char
SynchConsole::GetChar()
{
    readSemaphore->P();
    char ch = console->GetChar();

    return ch;
}

void
SynchConsole::PutChar(char ch)
{
    lock->Acquire();
    console->PutChar(ch);
    writeSemaphore->P();
    lock->Release();
}

void
SynchConsole::ReadAvail()
{
    readSemaphore->V();
}

void
SynchConsole::WriteDone()
{
    writeSemaphore->V();
}