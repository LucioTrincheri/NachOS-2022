/// Data structures for managing a disk file header.
///
/// A file header describes where on disk to find the data in a file, along
/// with other information about the file (for instance, its length, owner,
/// etc.)
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_FILESYS_FILEACCESSCONTROLLER__HH
#define NACHOS_FILESYS_FILEACCESSCONTROLLER__HH

#include "threads/lock.hh"
#include "threads/condition.hh"
class FileAccessController {
public:

    FileAccessController();

    ~FileAccessController();

    void AcquireRead();

    void ReleaseRead();

    void AcquireWrite();

    void ReleaseWrite();

private:
    Lock *ReadCounterLock;
    Condition *NoReaders;
    int readCounter;
};
#endif