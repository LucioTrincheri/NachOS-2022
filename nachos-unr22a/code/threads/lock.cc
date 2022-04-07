/// Routines for synchronizing threads.
///
/// The implementation for this primitive does not come with base Nachos.
/// It is left to the student.
///
/// When implementing this module, keep in mind that any implementation of a
/// synchronization routine needs some primitive atomic operation.  The
/// semaphore implementation, for example, disables interrupts in order to
/// achieve this; another way could be leveraging an already existing
/// primitive.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "lock.hh"
#include "system.hh"
#include <stdlib.h>
#include <stdio.h>


/// Dummy functions -- so we can compile our later assignments.

Lock::Lock(const char *debugName)
{
    // tal vez strcpy
    name = debugName;
    semaphore = new Semaphore("Semaforo", 1);
    lockOwner = nullptr;
}

Lock::~Lock()
{
    semaphore->~Semaphore();
}

const char *
Lock::GetName() const
{
    return name;
}

void
Lock::Acquire()
{
    ASSERT(!IsHeldByCurrentThread());
    //* Herencia de prioridad. Utilizada para solucionar el problema de inversion de prioridades en locks. 
    if (lockOwner && lockOwner->GetPriority() > currentThread->GetPriority()) {
        lockOwner->SetPriorityHerencia(currentThread->GetPriority());
    }
    semaphore->P();
    lockOwner = currentThread;
}

void
Lock::Release()
{
    ASSERT(IsHeldByCurrentThread());
    //* Si fue actualizada su prioridad.
    lockOwner->SetPriorityHerencia(lockOwner->GetOriginalPriority());
    semaphore->V();
    lockOwner = nullptr;
}

bool
Lock::IsHeldByCurrentThread() const
{
    return lockOwner == currentThread;
}
