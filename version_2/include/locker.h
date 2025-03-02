#ifndef LOCKER_H_
#define LOCKER_H_

#include <pthread.h>
#include <semaphore.h>

class locker
{
public:
    locker();
    ~locker();

public:
    bool lock();
    bool unlock();

private:
    pthread_mutex_t mut;
};

class sem
{
public:
    sem();
    ~sem();

public:
    bool wait();
    bool post();

private:
    sem_t m_sem;
};

#endif