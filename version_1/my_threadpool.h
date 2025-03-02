#ifndef _MY_THREADPOOL_H_
#define _MY_THREADPOOL_H_

#include <list>
#include "locker.h"

template<typename T>
class threadpool
{
public:
    threadpool(int thread_num = 8, int requests = 1000);
    ~threadpool();

    bool append(T* job);

private:
    static void* worker(void* arg);
    void run();

private:
    const int m_max_request_num = 2000;
    int m_thread_num;
    int m_requests;
    pthread_t *tids;
    bool m_stop;
    std::list<T*> m_workqueue;
    sem m_queuestate;
    locker m_queuelock;
};

template<typename T>
threadpool<T>::threadpool(int thread_num, int requests) 
    : m_thread_num(thread_num), m_requests(requests), m_stop(false)
{
    if (thread_num <= 0 || requests <= 0)
    {
        throw std::exception();
    }

    tids = new pthread_t[thread_num];
    if (!tids)
    {
        throw std::exception();
    }

    for (int i = 0; i < thread_num; i++)
    {
        if (pthread_create(tids + i, NULL, worker, this) != 0)
        {
            throw std::exception();
        }

        if (pthread_detach(tids[i]) != 0)
        {
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool()
{
    m_stop = true;
    delete [] tids;
}

template<typename T>
void* threadpool<T>::worker(void *arg)
{
    threadpool<T> *p = static_cast<threadpool<T>*>(arg);
    if (!p)
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
        m_queuestate.wait();
        m_queuelock.lock();
        if (m_workqueue.empty())
        {
            m_queuelock.unlock();
            continue;
        }
        T* job = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelock.unlock();
        if (!job)
        {
            continue;
        }
        job->process();
    }
}

template<typename T>
bool threadpool<T>::append(T* job)
{
    m_queuelock.lock();
    if (m_workqueue.size() >= m_max_request_num)
    {
        m_queuelock.unlock();
        return false;
    }

    m_workqueue.push_back(job);
    m_queuelock.unlock();
    m_queuestate.post();
    return true;
}

#endif