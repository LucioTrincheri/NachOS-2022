#include "open_file_list.hh"
#include "threads/lock.hh"
#include "file_access_controller.hh"

#include <stdio.h>
#include <string.h>

OpenFileList::OpenFileList()
{
    listLock = new Lock("OpenFileListLock");
    first = last = nullptr;
}

OpenFileList::~OpenFileList()
{
    OpenFileListEntry* aux;

    while(first != nullptr){
  		aux = first->next;
        delete first->accessController;
  		delete first;
  		first = aux;
	}

    delete listLock;
}

FileAccessController *
OpenFileList::AddOpenFile(int sector)
{
    OpenFileListEntry *entry = FindOpenFile(sector);
    if (entry != nullptr) {
        DEBUG('f', "Abro entry en AddOpenFile\n");
        entry->openInstances++;
    } else {
        entry = CreateOpenFileEntry(sector);
        if (first == nullptr) {
            first = last = entry;
        } else {
            last->next = entry;
            last = entry;
        }
    }
    return entry->accessController;
}

int
OpenFileList::CloseOpenFile(int sector)
{
    OpenFileListEntry *entry = FindOpenFile(sector);
    if (entry != nullptr) {
        entry->openInstances--;
        return entry->openInstances;
    }
    return -1;
}

bool
OpenFileList::SetToBeRemoved(int sector)
{
    OpenFileListEntry *entry = FindOpenFile(sector);
    if (entry != nullptr) {
        entry->toBeRemoved = true;
        if(entry->openInstances == 0) {
            return true;
        }
    } 
    return false;
}

bool
OpenFileList::GetToBeRemoved(int sector)
{
    OpenFileListEntry *entry = FindOpenFile(sector);
    if (entry != nullptr) {
        return entry->toBeRemoved;
    }
    return false;
}

// Si esta funcion se llama y openInstances es distinto de 0, hay algun error grave en la logica.
void
OpenFileList::RemoveOpenFile(int sector)
{
    OpenFileListEntry *aux, *prev = nullptr;
    for (aux = first; aux != nullptr && aux->sector != sector; prev = aux, aux = aux->next);

    // El elemento no existe o la lista es vacia
    if (aux == nullptr) {
        return;
    }

    // Si la lista tiene un unico elemento, se elemina y se reasigna first y last
    if (first == last){
        first = last = nullptr;
        delete aux->accessController;
        delete aux;
        return;
    }

    // Casos de primer elemento, ultimo elemento y elemento en el medio.
    if (aux == first) {
        first = first->next;
        delete aux->accessController;
        delete aux;
        return;
    }

    if (aux == last) {
        last = prev;
    }

    prev->next = aux->next;
    delete aux->accessController;
    delete aux;
    return;
}

OpenFileListEntry*
OpenFileList::FindOpenFile(int sector)
{
    OpenFileListEntry *aux;
    for (aux = first; aux != nullptr && aux->sector != sector; aux = aux->next);
    return aux;
}

void
OpenFileList::Acquire()
{
    listLock->Acquire();
}

void
OpenFileList::Release()
{
    listLock->Release();
}


// Privados ------------------
// Esta funcion se llama por create y solo se invoca si el archivo (sector) no existe.
OpenFileListEntry*
OpenFileList::CreateOpenFileEntry(int sector)
{
    OpenFileListEntry* entry = new OpenFileListEntry;
	entry->sector = sector;
	entry->openInstances = 1;
	entry->toBeRemoved = false;
    entry->accessController = new FileAccessController();
    entry->next = nullptr;

    return entry;
}


void
OpenFileList::PrintList()
{
    OpenFileListEntry *aux;
    for (aux = first; aux != nullptr; aux = aux->next) {
        Print(aux);
    }
}

void
OpenFileList::Print(OpenFileListEntry *toPrint)
{
    printf("--------------------------------\n");
    printf("Sector fh: %u\n",toPrint->sector);
    printf("Cantidad de instancias abiertas: %u\n",toPrint->openInstances);
    printf("A ser removido: %u\n",toPrint->toBeRemoved);
    if(toPrint->next == nullptr){
        printf("nullptr\n");
    } else {
        printf("%p\n",(void *)toPrint->next);
    }
}