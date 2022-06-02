/// Routines to manage a coremap.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#include "coremap.hh"
#include "system.hh"
#include <stdlib.h>
#include <stdio.h>

Coremap::Coremap(unsigned nitems)
{
    ASSERT(nitems > 0);
    addressInfo = new AddressInfoEntry[nitems];
    bitmap = new Bitmap(nitems);
    lock = new Lock("coremapLock");
    DEBUG('p', "Inicializando coremap\n");
    //for(int i = 0; i < nitems; i++) {
    //    DEBUG('p', "Inicializando coremap\n");
    //    addressInfo[i].thread = nullptr;
    //    addressInfo[i].vpn = -1;
    //}
}

/// De-allocate a bitmap.
Coremap::~Coremap()
{
    delete bitmap;
    delete lock;
    delete [] addressInfo;
}

void 
Coremap::Clear(unsigned which)
{
    lock->Acquire();
    bitmap->Clear(which);
    lock->Release();
}
int
Coremap::Find(int vpn)
{
    lock->Acquire();
    int physicalAddr = bitmap->Find();
    if (physicalAddr == -1) {
        DEBUG('p', "Memory full, need to swap\n");
    }
    lock->Release();
    return physicalAddr;
}

unsigned
Coremap::CountClear() const
{
    lock->Acquire();
    return bitmap->CountClear();
    lock->Release();
}

void
Coremap::Print() const
{
    bitmap->Print();
}