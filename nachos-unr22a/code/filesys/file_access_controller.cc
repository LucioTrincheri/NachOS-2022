#include "file_access_controller.hh"

FileAccessController::FileAccessController() {
    ReadCounterLock = new Lock("ReadCounterLock");
    NoReaders = new Condition("ReaderWriter CondVar", ReadCounterLock);
    readCounter = 0;
}

FileAccessController::~FileAccessController() {
    delete NoReaders;
    delete ReadCounterLock;
}

void
FileAccessController::AcquireRead() {
    if(not ReadCounterLock->IsHeldByCurrentThread()) {
        ReadCounterLock->Acquire();

        readCounter++;

        ReadCounterLock->Release();
    }
}

void
FileAccessController::ReleaseRead() {
    if(!ReadCounterLock->IsHeldByCurrentThread()) {
        ReadCounterLock->Acquire();

        readCounter--;

        if(readCounter == 0)
            NoReaders->Broadcast();

        ReadCounterLock->Release();
    }
}

void
FileAccessController::AcquireWrite() {
    ReadCounterLock->Acquire();

    while(readCounter > 0)
        NoReaders->Wait();

}

void
FileAccessController::ReleaseWrite() {
    NoReaders->Signal();
    ReadCounterLock->Release();
}
