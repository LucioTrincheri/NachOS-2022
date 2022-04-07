#ifndef NACHOS_THREADS_CHANNEL__HH
#define NACHOS_THREADS_CHANNEL__HH

#include "lock.hh"
#include "condition.hh"

class Channel {
public:
    Channel(const char *debugname);

    ~Channel();

    void Send(int message);
    void Receive(int *message);
    const char *getName();

private:
    const char *cname;
   
    List<int*>* messages;
    Lock* bufferLock;
    Condition* sender;
    Condition* receiver;

};

#endif