#include "open_file_list.hh"

#include <stdio.h>
#include <string.h>


OpenFileList::OpenFileList(FileSystem * myFileSystem_)
{
    Lock *listLock = new Lock("OpenFileListLock");
    first = nullptr;
    myFileSystem = myFileSystem_;
}

FileSystem::~FileSystem() {
    OpenFileListEntry* aux;

    while(first != nullptr){
  		aux = first -> next;
  		delete [] first -> name;
  		delete first;
  		first = aux;
	}

    delete listLock;
}

bool
OpenFileList::AddOpenFile(int sector){
    listLock->Acquire();
    OpenFileListEntry *entry = FindOpenFile(sector);
    if (entry != nullptr){
        entry->openInstances++;
        listLock->Release();
        return true;
    } else {
        // ! Falta hacer que AddOpenFile cree ua nueva instancia de OpenFileEntry si no encontro ninguna, ya que existe en el directorio. Checkear first y reasignarlo.
    }
    listLock->Release();
    return false;
}

int
OpenFileList::CloseOpenFile(int sector) {
    OpenFileListEntry *entry = FindOpenFile(sector);
    if (entry != nullptr){
        entry->openInstances--;
        return entry->openInstances;
    }
    return -1;
}

bool
OpenFileList::SetToBeRemoved(int sector) {
    OpenFileListEntry *entry = FindOpenFile(sector);
    if (entry != nullptr) {
        if(entry->openInstances == 0) {
            return true;
        }
    }
    return false;
}

// Si esta funcion se llama y openInstances es distinto de 0, hay algun error grave en la logica.
void
OpenFileList::RemoveOpenFile(int sector) {
    OpenFileListEntry *aux, *prev = nullptr;
    for (aux = first; aux != nullptr && aux->sector != sector; prev = aux, aux = aux->next);

    if (aux == nullptr) {
        return;
    }
    if (aux == first){
        first = first->next;
        // TODO borrar aux
        return;
    }
    prev->next = aux->next;
    // TODO borrar aux
}


void
OpenFileList::Acquire(){
    listLock->Acquire();
}

void
OpenFileList::Release(){
    listLock->Release();
}

OpenFileListEntry*
OpenFileList::FindOpenFile(int sector){
    OpenFileListEntry *aux;
    for (aux = first; aux != nullptr && aux->sector != sector; aux = aux->next);
    return aux;
}

OpenFileListEntry*
OpenFileList::CreateOpenFileEntry(int sector){
}
