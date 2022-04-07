#include "channel.hh"
#include <stdlib.h>

Channel::Channel(const char *debugname)
{
    cname = debugname;
    messages = new List<int*>;
    bufferLock = new Lock(cname);
    sender = new Condition(cname, bufferLock);
    receiver = new Condition(cname, bufferLock);
}

Channel::~Channel()
{
    delete messages;
    delete bufferLock;
    delete sender;
    delete receiver;
}

const char * Channel::getName()
{
    return cname;
}

void Channel::Send(int message)
{
    bufferLock->Acquire();
    int* msg = (int*)malloc(sizeof(int));
    *msg = message;
    messages->Append(msg);
    receiver->Signal();
    int* m = messages->Head();
    while (!messages->IsEmpty() && m == messages->Head()) 
    {
        sender->Wait();
    }
    bufferLock->Release();
}

void Channel::Receive(int *message)
{
    bufferLock->Acquire();
    while (messages->IsEmpty()) {
        receiver->Wait();
    }
    int* msg = messages->Pop();
    *message = *msg;
    free(msg);
    sender->Signal();
    bufferLock->Release();
}