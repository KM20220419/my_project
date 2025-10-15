#ifndef MY_THREAD_POOL_H
#define NY_THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread>
#include "../my_lock/my_lock.h"
#include "../my_CGImysql/my_sql_connection_pool.h"

template <class T>
class threadpool
{
public:
    threadpool(int actor_model, connection_pool *m_connpool, int m_thread_numbers = 8, int m_max_requests = 10000);
    ~threadpool();
    bool append(T *request, int state); // 添加任务到队列中，reactor模型
    bool append_p(T *request);          // Proactor模型

public:
    void run();                     // 工作现成的运行函数
    static void *worker(void *arg); // 工作线程的入口函数。
    // 注意这里的形参为 void* ,主要是因为pthread_create的第三个参数要求 返回值为void *类型的函数。
    // 第四个参数为 void* 类型 ，

private:
    int m_thread_numbers; // 线程池中的线程数
    int m_max_requests;   // 线程池中允许的最大请求数
    pthread_t *pthread;   // 存放线程的数组
    locker m_lock;
    sem m_requeuestat; // 信号量，确定有无任务
    connection_pool *m_connpool;
    int m_actor_model;         // 用于模型切换
    std::list<T *> m_workqueue // 用于存放任务的请求队列
};
template <class T>
threadpool<T>::threadpool(int actor_model, connection_pool *m_connpool, int m_thread_numbers = 8, int m_max_requests = 10000) : m_actor_model(actor_model), m_connpool(m_connpool), m_thread_numbers(m_thread_numbers), m_max_requests(m_max_requests), pthreads(NULL)
{
    if (m_thread_numbers == 0 || m_max_requests == 0)
    {
        throw std::exception();
    }
    // 创建线程数组
    // new 一个pthread_t类型的数组，并返回对应类型的指针，所以使用pthread_t *的指针来接收
    pthread = new pthread_t[m_thread_numbers];
    if (!pthread)
    {
        throw std::exception();
    }
    for (int i = 0; i < m_thread_numbers; i++)
    {
        // pthread_create 中第一个参数要求写入 pthread_t* 类型的指针（也就是地址），用于存放线程
        if (pthread_create(pthread + i, NULL, worker, this) != 0) // pthread_create返回值为0，表示创建成功
        {
            delete[] pthread;
            throw std::exception();
        }
        if (pthread_detach(pthread[i]) != 0)
        {
            delete[] pthread;
            throw std::exception();
        }
    }
}

template <class T>
threadpool<T>::~threadpool()
{
    delete[] pthread;
}

template <class T>
// 用于reactor模式，其中state表示读或者写
bool threadpool<T>::append(T *request, int state)
{
    m_lock.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_lock.unlock();
        return false;
    }
    request->m_state = state;
    m_workqueue.push_back(request);
    m_lock.unlock();
    m_requeuestat.post();
    return true
}
// proactor
template <class T>
bool threadpool<T>::append_p(T *request)
{
    m_lock.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_lock.unlock();
        return false
    }
    m_workqueue.push_back(request);
    m_lock.unlock();
    m_requeuestat.post();
    return true
}

template <class T>
static void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template <class T>
void threadpool<T>::run()
{
    while (true)
    {
        m_requeuestat.wait();
        m_locker.lock();
        if (m_workqueue.empty())
        {
            m_locker.unlock();
            continue
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_locker.unlock();
        if (!request)
        { // 防止取出的为空指针
            continue
        };
        if (m_actor_model == 1) // m_actor_model为1表示Reactor
        {
            if (request->m_state == 0) // 表示读事件
            {
                if (request->read_once())
                {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connpool);
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else
            {
                if (request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else
        {
            // proactor模式中工作线程只负责业务逻辑处理
            connectionRAII mysqlconn(&request->mysql, m_connpool);
            request->process();
        }
    }
#endif