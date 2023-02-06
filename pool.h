#ifndef POOL_H
#define POOL_H

#include <pthread.h>
#include <list>
#include <cstdio>
#include "locker.h"

template <typename T>
class pool
{
public:
    pool(int thread_number = 8, int max_requests = 10000);
    ~pool();
    bool append(T *request);

private:
    static void *worker(void *arg);
    void run();

private:
    //线程数量
    int m_thread_number;

    //线程池数组
    pthread_t *m_threads;

    //请求队列中最多允许的等待处理的数量
    int m_max_requests;

    //请求队列
    std::list<T *> m_workqueue;

    //互斥锁
    locker m_queuelocker;

    //信号量用来判断是否有任务需要处理
    sem m_queuestat;

    //是否结束线程
    bool m_stop;
};

template <typename T>
threadpool<T>::pool(int thread_number, int max_requests) : m_thread_number(thread_number), m_max_requests(max_requests),
                                                           m_stop(false), m_threads(NULL)
{
    //判断要创建的线程、任务的数量
    if ((thread_number <= 0) || (max_requests <= 0))
    {
        throw std::exception();
    }

    //创建线程数组
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
    {
        throw std::exception();
    }

    //创建线程，并设置为线程分离
    for (int i = 0; i < thread_number; i++)
    {
        printf("create the %dth thread\n", i);
        //线程创建
        if (pthread_create(m_threads + i, NULL, worker, NULL) != 0)
        { /*线程、线程属性、回调函数、传递参数*/
            delete[] m_threads;
            throw std::exception();
        }

        //线程分离
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
pool<T>::~pool()
{
    delete[] m_threads;
    m_stop = true;
}

template <typename T>
void *threadpool<T>::worker(void *arg)
{   
    threadpool *pool = (threadpool *)arg;
    pool->run(); 
    return pool;
}

//线程加入队列
template <typename T>
bool pool<T>::append(T *request)
{
    m_queuelocker.lock();
    //判断请求队列是否可添加
    if (m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    //将任务加入队列
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    //信号量加一
    m_queuestat.post();

    return true;
}

#endif