#ifndef MY_LOCK_H
#define MY_LOCK_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

class sem
{
public:
    // 默认的构造函数
    sem()
    {
        // 下面的的三个类别初始化成功返回值都为0, 如果初始化失败就 返回错误
        if (sem_init(&m_set, 0, 0) != 0)
        {
            throw std::exception();
        }
    };
    // 有参构造，可以勇于指定初始时有一定的资源数（在生产者-消费者模型中）
    sem(int num)
    {
        if (sem_init(&m_set, 0, num) != 0)
        {
            throw std::exception();
        }
    }
    // 析构函数
    ~sem()
    {
        // 销毁信号量
        sem_destroy(&m_set);
    }
    // sem_wait函数在调用时，会将信号量进行-1的操作，如果剩余信号量>0 就返回 0
    // 反之信号量不够的话，就会阻塞
    // 这里用bool 作为返回值，是将操作成功后返回值从0 转变为true
    bool wait()
    {
        return sem_wait(&m_set) == 0;
    }

    // 这里与sem_wait相反，是进行信号量+1的操作
    bool post()
    {
        return sem_post(&m_set) == 0;
    }

private:
    sem_t m_set;
};

// 使用线程库pthread 实现互斥锁
class locker
{
public:
    locker()
    {
        if (pthread_mutex_init(&mtx, NULL) != 0)
        {
            throw std::exception();
        }
    }
    ~locker()
    {
        pthread_mutex_destroy(&mtx);
    }

    // 加锁操作
    bool lock()
    {
        return pthread_mutex_lock(&mtx);
    }
    bool unlock()
    {
        return pthread_mutex_unlock(&mtx);
    }
    // 对外提供裸指针  --- 为下面的条件变量 pthread_cond_wait使用
    pthread_mutex_t *getptr()
    {
        return &mtx;
    }

private:
    pthread_mutex_t mtx;
};

class cond
{
public:
    cond()
    {
        if (pthread_cond_init(&cv, NULL) != 0)
        {
            throw std::exception();
        }
    }
    ~cond()
    {
        pthread_cond_destroy(&cv);
    }

    // 并且pthread_cond条件变量与C++11的condition_variable使用方法不同

    // 注意使用pthread_cond_wait,要使用while循环来一直判断是否可以唤醒，不能用if
    // 因为，在if中pthread_cond_wait被阻塞后，假设被唤醒，是不会再次进行条件判断的，会接着执行
    // 这里就有可能存在虚假唤醒以及多线程竞争
    bool cond_wait(pthread_mutex_t *m_mutex)
    {
        int ret = 0;
        ret = pthread_cond_wait(&cv, m_mutex);
        return ret == 0;
    }

    bool cond_timewait(pthread_mutex_t *m_mutex, struct timespec time)
    {
        int ret = 0;
        // timedwait 表示阻塞一定的时间，超时报错
        ret = pthread_cond_timedwait(&cv, m_mutex, &time);
        return ret == 0;
    }

    bool signal_one()
    {
        // pthread_codn_signal表示唤醒一个阻塞的线程
        return pthread_cond_signal(&cv) == 0;
    }
    bool signal_all()
    {
        // pthread_cond_broadcast 表示唤醒所有阻塞的线程
        return pthread_cond_broadcast(&cv) == 0;
    }

private:
    pthread_cond_t cv;
};

#endif