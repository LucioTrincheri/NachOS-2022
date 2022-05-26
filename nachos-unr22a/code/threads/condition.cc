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


#include "condition.hh"
#include "system.hh"
#include <stdio.h>


/// Dummy functions -- so we can compile our later assignments.
///
/// Note -- without a correct implementation of `Condition::Wait`, the test
/// case in the network assignment will not work!

Condition::Condition(const char *debugName, Lock *conditionLock)
{
    name = debugName;
    cond = conditionLock;
    waiting = new List<Semaphore*>;
}

Condition::~Condition()
{
    //tal vez algo mas vaya aca si hay list o algo
    delete waiting; // esto se deeberia encargar de llamar ~Semaphore para cada item
}

const char *
Condition::GetName() const
{
    return name;
}

void
Condition::Wait()
{
    Semaphore *semaphore = new Semaphore(currentThread->GetName(), 0);
    //* Se almacenan los semaforos en orden dependiendo la prioridad del thread, para al despertarlos hacer que se levante primero el de mayor prioridad.
    waiting->SortedInsert(semaphore, currentThread->GetPriority());
    cond->Release();

    // sleep
    semaphore->P();
    //! Se libera el semaforo y se lo elimina de la lista.
    waiting->Remove(semaphore);
    delete semaphore;
    cond->Acquire();
}

// Puede que sea necesairo tener el lock antes de llamar a signal o broadcast
void
Condition::Signal()
{
    ASSERT(cond->IsHeldByCurrentThread());
    Semaphore *semaphore = waiting->Pop();
    if (semaphore != nullptr) {
        semaphore->V();
    }    
}

void
Condition::Broadcast()
{
    ASSERT(cond->IsHeldByCurrentThread());
    Semaphore *semaphore;
    while (!waiting->IsEmpty()) {
        semaphore = waiting->Pop();
        semaphore->V();
    }  
}
