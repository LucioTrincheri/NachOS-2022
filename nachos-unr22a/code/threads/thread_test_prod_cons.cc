/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "thread_test_prod_cons.hh"
#include "lock.hh"
#include "condition.hh"
#include "system.hh"

#include <stdio.h>

#define TAM_GONDOLA 10
#define OPERACIONES 50

int harina = 0;
Lock *cantHarina = new Lock("noVacia");

Condition *noVaciaCond = new Condition("noVaciaCond", cantHarina);
Condition *noLlenaCond = new Condition("noLlenaCond", cantHarina);

void
productorDeHarina(void *n_)
{
    int i = 0;
    while (i < OPERACIONES)
    {
        cantHarina->Acquire();
        while(harina == TAM_GONDOLA) noLlenaCond->Wait();

        harina ++;
        printf("Productor agrega uno de harina, actual: %d\n", harina);
        i ++;
        noVaciaCond->Signal();
        cantHarina->Release();

    }
    printf("Productor termina\n");
}

void
consumidorDeHarina(void *n_)
{
    int i = 0;
    while (i < OPERACIONES)
    {
        cantHarina->Acquire();
        while(harina == 0) noVaciaCond->Wait();
        harina --;
        printf("Consumidor retira uno de harina, actual: %d\n", harina);
        i ++;
        noLlenaCond->Signal();
        cantHarina->Release();
    }
    printf("Consumidor termina\n");
}

void
ThreadTestProdCons()
{
    char *name = new char [16];
    sprintf(name, "Consumirdor");
    Thread *t = new Thread(name, true, 0);
    unsigned *n = new unsigned;
    t->Fork(consumidorDeHarina, (void*) n);
    

    char *name2 = new char [16];
    sprintf(name2, "Productor");
    Thread *t2 = new Thread(name2, true, 0);
    t2->Fork(productorDeHarina, (void*) n);

    t->Join();
    t2->Join();

    printf("Harina final: %d.\n",
           harina);
}
