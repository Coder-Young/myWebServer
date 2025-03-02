#include <exception>

#include "../include/locker.h"

locker::locker()
{
    if (pthread_mutex_init(&mut, nullptr) != 0)
    {
        throw std::exception();
    }
}

locker::~locker()
{
    pthread_mutex_destroy(&mut);
}

bool locker::lock()
{
    return pthread_mutex_lock(&mut) == 0;
}

bool locker::unlock()
{
    return pthread_mutex_unlock(&mut) == 0;
}



sem::sem()
{
    if (sem_init(&m_sem, 0, 0) != 0)
    {
        throw std::exception();
    }
}

sem::~sem()
{
    sem_destroy(&m_sem);
}

bool sem::post()
{
    return sem_post(&m_sem) == 0;
}

bool sem::wait()
{
    return sem_wait(&m_sem) == 0;
}