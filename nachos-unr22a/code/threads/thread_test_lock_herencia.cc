/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#define SEMAPHORE_TEST = 1

#include "thread_test_lock_orden.hh"
#include "system.hh"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "lock.hh"


Lock *lock3 = new Lock("Lock");

void esperandoLock1(void *name_)
{

    lock3->Acquire();

    currentThread->Yield();

    printf("Este print va a ser despues del print de sinLock\n");
    printf("%d, %d\n", currentThread->GetPriority(), currentThread->GetOriginalPriority());

    lock3->Release();
}

void sinLock1(void *name_)
{
    
    printf("Comeinza ejecucion de 5\n");
    currentThread->Yield();
    printf("Esto deberia ser el ultimo mensaje\n");
}

void pedirLock1(void *name_)
{

    lock3->Acquire();

    printf("Esto se deberia printear antes de las estadisticas del thread con identidad 5\n");
    printf("%d, %d\n", currentThread->GetPriority(), currentThread->GetOriginalPriority());

    lock3->Release();
}

//* Ejecutar sin rs para testear.

/*

    En este test vemos como un proceso de mayor prioridad al que posee el lock no se ejecuta antes de este mientras en la espera del lock
    haya un proceso con mayor prioridad.

*/
void ThreadTestLock()
{

    Thread *newThread1 = new Thread("1", true, 14);
    newThread1->Fork(esperandoLock1, (void *)"1");


    Thread *newThread2 = new Thread("2", true, 3);
    newThread2->Fork(sinLock1, (void *)"2");

    currentThread->Yield();

    Thread *newThread3 = new Thread("3", true, 1);
    newThread3->Fork(pedirLock1, (void *)"3");
    
    newThread1->Join();
    newThread2->Join();
    newThread3->Join();

    printf("Todos terminaron de mander acorrecta\n");

}