#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include "locker.h"

#include <pthread.h>
#include <list>
#include <exception>

template<typename T>
class threadpool
{
    static const int MAX_THREAD_NUM = 2000;
    static const int MAX_REQUEST_NUM = 2000;

public:
    threadpool(int thread_num);
    ~threadpool();

public:
    bool append(T *job);

    static void* worker(void *);

    void run();

private:
    int m_thread_num;
    bool m_stop;
    pthread_t *threads;
    std::list<T*> m_workqueue;
    locker m_locker;
    sem m_sem;
};

template<typename T>
threadpool<T>::threadpool(int thread_num) : m_thread_num(thread_num), m_stop(false)
{
    if (m_thread_num >= MAX_THREAD_NUM || m_thread_num <= 0)
    {
        throw std::exception();
    }

    threads = new pthread_t[thread_num];
    for (int i = 0; i < thread_num; i++)
    {
        if (pthread_create(threads + i, NULL, worker, this) != 0)
        {
            throw std::exception();
        }
        
        if (pthread_detach(threads[i]) != 0)
        {
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool()
{
    m_stop = true;
    delete [] threads;
}

template<typename T>
void* threadpool<T>::worker(void *arg)
{
    threadpool<T> *p = static_cast<threadpool<T>*>(arg);
    if (p == nullptr)
    {
        return NULL;
    }

    p->run();
    return p;
}

template<typename T>
void threadpool<T>::run()
{
    while (!m_stop)
    {
        m_sem.wait();
        m_locker.lock();
        if (m_workqueue.empty())
        {
            m_locker.unlock();
            continue;
        }

        T *job = m_workqueue.front();
        m_workqueue.pop_front();
        m_locker.unlock();
        if (job == NULL)
        {
            continue;
        }

        job->process();
    }
}

template<typename T>
bool threadpool<T>::append(T *job)
{
    if (job == NULL)
    {
        return false;
    }

    m_locker.lock();
    if (m_workqueue.size() >= MAX_REQUEST_NUM)
    {
        m_locker.unlock();
        return false;
    }

    m_workqueue.push_back(job);
    m_locker.unlock();
    m_sem.post();
    
    return true;
}

#endif // THREADPOOL_H_