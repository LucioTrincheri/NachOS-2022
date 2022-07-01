#ifndef NACHOS_FILESYS_OPENFILELIST__HH
#define NACHOS_FILESYS_OPENFILELIST__HH

#include "threads/lock.hh"


//class Lock; // ??????????????

// Entrada de la linked_list OpenFileList
struct OpenFileListEntry{
    // Name of the file
    int sector;
    // Amount of OpenFile instances that reference the file
    int openInstances;
    // True iff Remove has been called on the file
    bool toBeRemoved;

    OpenFileListEntry *next;
};

class OpenFileList {
public:

    OpenFileList();

    ~OpenFileList();

    bool AddOpenFile(int sector);

    int CloseOpenFile(int sector);

    bool SetToBeRemoved(int sector);

    bool GetToBeRemoved(int sector);

    void Acquire();

    void Release();

    void RemoveOpenFile(int sector);
private:
    // Lock
    Lock *listLock;
    
    OpenFileListEntry *first, *last;
    // Metodos

    OpenFileListEntry* FindOpenFile(int sector);
    OpenFileListEntry* CreateOpenFileEntry(int sector);


};


#endif
