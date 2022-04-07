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
#include "channel.hh"


Channel *canal = new Channel("banana");
Lock *lock = new Lock("asdasd");
Condition *condicion = new Condition("asd",lock);

void threadSender(void *name_)
{
    char *name = (char *)name_;

   printf("[%s]Mandando %d\n", name, name[6]-49);
   canal->Send(name[6]-49);
}

void threadReceiver(void *name_)
{
    char *name = (char *)name_;
    printf("Comeinza ejecucion de recibir de %s\n", name);

    int* a = (int*)malloc(sizeof(int));
    canal->Receive(a);
    printf("[%s]Recibí %d\n", name, *a);
    free(a);
}

#define NUM_THREAD 2

void ThreadTestChannel()
{

    Thread *newThreadS1 = new Thread("Sender1", true, 0);
    newThreadS1->Fork(threadSender, (void *)"Sender1");

    Thread *newThreadR1 = new Thread("Reciver1", true, 0);
    newThreadR1->Fork(threadReceiver, (void *)"Reciver1");

    Thread *newThreadS2 = new Thread("Sender2", true, 0);
    newThreadS2->Fork(threadSender, (void *)"Sender2");

    Thread *newThreadR2 = new Thread("Reciver2", true, 0);
    newThreadR2->Fork(threadReceiver, (void *)"Reciver2");

    
    newThreadS1->Join();
    printf("Hilo 1 termino\n");
    newThreadS2->Join();
    printf("Hilo 2 termino\n");
    newThreadR1->Join();
    printf("Hilo 3 termino\n");
    newThreadR2->Join();
    printf("Hilo 4 termino\n");

    printf("Todos terminaron de mander acorrecta\n");

}