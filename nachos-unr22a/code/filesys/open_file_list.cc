#include "open_file_list.hh"
//#include "threads/lock.hh"

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
  		delete first;
  		first = aux;
	}

    delete listLock;
}

bool
OpenFileList::AddOpenFile(int sector)
{
    listLock->Acquire();
    OpenFileListEntry *entry = FindOpenFile(sector);
    if (entry != nullptr){
        entry->openInstances++;
        listLock->Release();
        return true;
    }

    entry = CreateOpenFileEntry(sector);
    if (first == nullptr) {
        first = last = entry;
    } else {
        last->next = entry;
        last = entry;
    }

    listLock->Release();
    return true;
}

int
OpenFileList::CloseOpenFile(int sector)
{
    OpenFileListEntry *entry = FindOpenFile(sector);
    if (entry != nullptr){
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
        delete aux;
        return;
    }

    // Casos de primer elemento, ultimo elemento y elemento en el medio.
    if (aux == first) {
        first = first->next;
        delete aux;
        return;
    }

    if (aux == last) {
        last = prev;
    }

    prev->next = aux->next;
    delete aux;
    return;
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

OpenFileListEntry*
OpenFileList::FindOpenFile(int sector)
{
    OpenFileListEntry *aux;
    for (aux = first; aux != nullptr && aux->sector != sector; aux = aux->next);
    return aux;
}

OpenFileListEntry*
OpenFileList::CreateOpenFileEntry(int sector)
{
    OpenFileListEntry* entry = new OpenFileListEntry;
	entry->sector = sector;
	entry->openInstances = 1;
	entry->toBeRemoved = false;
    entry->next = nullptr;

	return entry;
}