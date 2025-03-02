#ifndef _LOCKER_H_
#define _LOCKER_H_

#include <pthread.h>
#include <semaphore.h>
#include <exception>

class sem
{
public:
    sem()
    {
        if (sem_init(&m_sem, 0, 0) != 0)
        {
            throw std::exception();
        }
    }

    ~sem()
    {
        sem_destroy(&m_sem);
    }

    bool wait()
    {
        return sem_wait(&m_sem) == 0;
    }

    bool post()
    {
        return sem_post(&m_sem) == 0;
    }
private:
    sem_t m_sem;
};

class locker
{
public:
    locker()
    {
        if (pthread_mutex_init(&mut, nullptr) != 0)
        {
            throw std::exception();
        }
    }

    ~locker()
    {
        pthread_mutex_destroy(&mut);
    }

    bool lock()
    {
        return pthread_mutex_lock(&mut) == 0;
    }

    bool unlock()
    {
        return pthread_mutex_unlock(&mut) == 0;
    }

private:
    pthread_mutex_t mut;
};

class cond
{
public:
    cond()
    {
        if (pthread_mutex_init(&m_mut, nullptr) != 0)
        {
            throw std::exception();
        }

        if (pthread_cond_init(&m_cond, nullptr) != 0)
        {
            pthread_mutex_destroy(&m_mut);
            throw std::exception();
        }
    }

    ~cond()
    {
        pthread_mutex_destroy(&m_mut);
        pthread_cond_destroy(&m_cond);
    }

    bool wait()
    {
        pthread_mutex_lock(&m_mut);
        int ret = pthread_cond_wait(&m_cond, &m_mut);
        pthread_mutex_unlock(&m_mut);
        return ret == 0;
    }

    bool signal()
    {
        return pthread_cond_signal(&m_cond) == 0;
    }
private:
    pthread_mutex_t m_mut;
    pthread_cond_t m_cond;
};

#endif