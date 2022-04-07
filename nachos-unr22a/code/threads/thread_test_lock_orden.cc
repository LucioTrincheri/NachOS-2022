/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#define SEMAPHORE_TEST = 1

#include "thread_test_simple.hh"
#include "system.hh"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "lock.hh"


Lock *lock2 = new Lock("Lock");

void esperandoLock(void *name_)
{
    printf("Inicio de proceso, Prioridad: %d, Nombre: %s\n", currentThread->GetPriority(), (char*) name_);
    lock2->Acquire();

    currentThread->Yield();

    printf("Fin de proceso, Prioridad: %d, PrioridadOriginal: %d\n", currentThread->GetPriority(), currentThread->GetOriginalPriority());

    lock2->Release();
}


void pedirLock(void *name_)
{
    printf("Inicio de proceso, Prioridad: %d, Nombre: %s\n", currentThread->GetPriority(), (char*) name_);

    lock2->Acquire();

    printf("Fin de proceso, Prioridad: %d, PrioridadOriginal: %d\n", currentThread->GetPriority(), currentThread->GetOriginalPriority());

    lock2->Release();
}

//* Ejecutar sin rs para testear.
/*
    En este test se ve como los procesos que esperan el lock se reorganizan para que luego de la liberacion del mismo
    se ejecute el de mas prioridad.
*/

void ThreadTestLockOrden()
{
    Thread *newThread1 = new Thread("14", true, 14);
    newThread1->Fork(esperandoLock, (void *)"14");

    currentThread->Yield();

    Thread *newThread2 = new Thread("3", true, 3);
    newThread2->Fork(pedirLock, (void *)"3");

    currentThread->Yield();

    Thread *newThread3 = new Thread("1", true, 1);
    newThread3->Fork(pedirLock, (void *)"1");
    
    newThread1->Join();
    newThread2->Join();
    newThread3->Join();

    printf("Todos terminaron de mander acorrecta\n");

}