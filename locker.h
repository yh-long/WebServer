#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <exception>
#include <semaphore.h>
//线程同步机制封装类

//互斥锁类,共享数据同步
class locker
{
public:
    locker()
    {
        if (pthread_mutex_init(&m_mutex, NULL) != 0)
        {
            throw std::exception();
        }
    }

    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    //加锁
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    //解锁
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    //获取锁
    pthread_mutex_t *get()
    {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};

//条件变量类，获得锁后满足条件执行
class cond
{
public:
    cond()
    {
        if (pthread_cond_init(&m_cond, NULL) != 0)
        {
            throw std::exception();
        }
    }

    ~cond()
    {
        pthread_cond_destroy(&m_cond);
    }
    //条件、锁
    bool wait(pthread_mutex_t *mutex)
    {
        return pthread_cond_wait(&m_cond, mutex) == 0;
    }
    //定时条件、锁
    bool timedwait(pthread_mutex_t *mutex, struct timespec t)
    {
        return pthread_cond_timedwait(&m_cond, mutex, &t) == 0;
    }
    //信号
    bool signal()
    {
        return pthread_cond_signal(&m_cond) == 0;
    }

    //唤醒线程
    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    pthread_cond_t m_cond;
};

//信号量类，是否可以执行任务
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
    sem(int num)
    {
        if (sem_init(&m_sem, 0, num) != 0)
        {
            throw std::exception();
        }
    }

    //销毁信号量
    ~sem()
    {
        sem_destroy(&m_sem);
    }

    //等待信号量-1
    bool wait()
    {
        return sem_wait(&m_sem) == 0;
    }

    //增加信号量+1
    bool post()
    {
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;
};
#endif