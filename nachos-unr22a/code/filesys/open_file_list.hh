#ifndef NACHOS_FILESYS_OPENFILELIST__HH
#define NACHOS_FILESYS_OPENFILELIST__HH

class Lock;

// Entrada de la linked_list OpenFileList
struct OpenFileListEntry{
    // Name of the file
    int sector;
    // Amount of OpenFile instances that reference the file
    int openInstances;
    // True iff Remove has been called on the file
    bool toBeRemoved;

    Lock *writeLock;

    OpenFileListEntry *next;
};

class OpenFileList {
public:

    OpenFileList();

    ~OpenFileList();

    Lock* AddOpenFile(int sector);

    int CloseOpenFile(int sector);

    bool SetToBeRemoved(int sector);

    bool GetToBeRemoved(int sector);

    void Acquire();

    void Release();

    void RemoveOpenFile(int sector);

    void PrintList();

    void Print(OpenFileListEntry * sector);

    OpenFileListEntry* FindOpenFile(int sector);
private:
    // Lock
    Lock *listLock;
    
    OpenFileListEntry *first, *last;
    // Metodos

    OpenFileListEntry* CreateOpenFileEntry(int sector);

};


#endif