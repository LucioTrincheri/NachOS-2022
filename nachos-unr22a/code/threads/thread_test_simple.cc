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
#include "semaphore.hh"

// Otra forma seria pasar la referencia por argumento a cada thread pero requiere crear una estructura para el 
// name y es mas simple asi.
#ifdef SEMAPHORE_TEST
Semaphore *sem = new Semaphore("Semaforo", 1); // un hilo a la vez en este caso
#endif

/// Loop 10 times, yielding the CPU to another ready thread each iteration.
///
/// * `name` points to a string with a thread name, just for debugging
///   purposes.
void SimpleThread(void *name_)
{
    // Reinterpret arg `name` as a string.
    char *name = (char *)name_;

    // If the lines dealing with interrupts are commented, the code will
    // behave incorrectly, because printf execution may cause race
    // conditions.
    for (unsigned num = 0; num < 10; num++)
    {
#ifdef SEMAPHORE_TEST
        DEBUG('t', "Thread %s hace P\n", name);
        sem->P();
#endif
        printf("*** Thread `%s` is running: iteration %u\n", name, num);
#ifdef SEMAPHORE_TEST
        DEBUG('t', "Thread %s hace V\n", name);
        sem->V();
#endif
        currentThread->Yield();
    }
    printf("!!! Thread `%s` has finished\n", name);
}

/// Set up a ping-pong between several threads.
///
/// Do it by launching one thread which calls `SimpleThread`, and finally
/// calling `SimpleThread` on the current thread.

#define NUM_THREAD 4 // 4 mas el que los forkea = 5

void ThreadTestSimple()
{
    for (int i = 0; i < NUM_THREAD; i++)
    {
        char *name = new char[1];
        // + 50 para pasarlo a ascci y saltar 0 y 1
        name[0] = i + 50;
        Thread *newThread = new Thread(name, false, 0);
        newThread->Fork(SimpleThread, (void *)name);
    }
    SimpleThread((void *)"1");
}