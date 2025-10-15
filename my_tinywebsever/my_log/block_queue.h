#ifndef MY_BLOCK_QUEUE_H
#define MY_BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>

#include "../my_lock/my_lock.h"

// 使用模板->不确定队列中的类型

// 使用循环数组实现阻塞队列
template <class T>
class block_queue
{
public:
    block_queue(int size = 1000)
    {
        if (size <= 0)
        {
            exit(-1);
        }
        int m_max_size = size;
        m_array = new T[size];
        m_size = 0;
        m_front = -1;
        m_back = -1;
    }
    void clear()
    {
        // 清除操作也是对队列进行改动，要加锁
        m_mutex.lock();
        m_front = -1;
        m_back = -1;
        m_size = 0;
        m_mutex.unlock();
    }
    ~block_queue()
    {
        m_mutex.lock();
        if (m_array != NULL)
        {
            delete[] m_array;
        }
        m_mutex.unlock();
    }
    // 判断队列是否满了
    bool Isfull()
    {
        m_mutex.lock();
        if (m_size >= m_max_size)
        {
            m_mutex.unlock();
            return true
        }
        m_mutex.unlock();
        return false
    }
    // 判断队列是否为空
    bool Isempty()
    {
        m_mutex.lock();
        if (m_size == 0)
        {
            m_mutex.unlock();
            return true
        }
        m_mutex.unlock();
        return false
    }

    // 返回队首元素---用引用的方法
    // 使用bool类型作为返回值，可以在使用时判断队列是否为空，用void就不行
    bool front(T &value)
    {
        m_mutex.lock();
        if (m_size == 0)
        {
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_front];
        m_mutex.unlock();
        return true;
    }
    // 返回队尾元素
    bool back(T &value)
    {
        m_mutex.lock();
        if (0 == m_size)
        {
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_back];
        m_mutex.unlock();
        return true;
    }
    // 下面的两个函数size 以及 max_size 都是为了线程安全
    // 如果直接返回m_szie，max_size，可能存在有线程在增加或者删去数据
    int size()
    {
        int tmp = 0;
        m_mutex.lock();
        tmp = m_size;
        m_mutex.unlock();
        return tmp;
    }

    int max_size()
    {
        int tmp = 0;

        m_mutex.lock();
        tmp = m_max_size;

        m_mutex.unlock();
        return tmp;
    }

    // 往队列中push元素
    // push之后还要通知所有等待的线程，告诉他们有新的任务来了
    bool push(T &value)
    {
        m_mutex.lock();
        // 所以假如这次队列满了之后，他会通知wait的线程来消费。之后下一次才能push成功
        if (m_size >= m_max_size)
        {
            m_cond.signal_all(); // 队列满了的情况下，通知线程消耗一些
            m_mutex.unlock();    // 这一步释放了互斥锁之后，被唤醒的线程开始争夺互斥锁
            return false;
        }
        m_back = (m_back + 1) % m_max_size; // 采用循环数组
        m_array[m_back] = value;
        m_size++;
        // 通知所有线程，有数据来了
        m_cond.signal_all();
        m_mutex.unlock();
        return true
    }

    bool pop(T &value)
    {
        m_mutex.lock();
        // 如队列中没有元素，就循环等待
        while (m_size <= 0)
        {
            if (!m_cond.cond_wait(m_mutex.getptr())) // 在这里阻塞
            {
                m_mutex.unlock();
                return false
            }
        }
        m_front = (m_front + 1) % m_max_size;
        value = m_array[m_front];
        m_mutex.unlock();
        return true
    }

    // 超时处理
    bool pop(T &value, int ms_timeout)
    {
        struct timespec t = {0, 0};
        struct timeval now = {0, 0};
        gettimeofday(&now, NULL);
        m_mutex.lock();
        if (m_size <= 0)
        {
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            t.tv_nsec = (ms_timeout % 1000) * 1000;
            if (!m_cond.cond_timewait(m_mutex.getptr(), t))
            {
                m_mutex.unlock();
                return false;
            }
        }
        // 上面不用while循环判断是因为这里还有一次判断
        if (m_size <= 0)
        {
            m_mutex.unlock();
            return false;
        }
        m_front = (m_front + 1) % m_max_size;
        value = m_array[m_front];
        m_szie--;
        m_mutex.unlock();
        return true;
    }

private:
    locker m_mutex;
    cond m_cond;

    T *m_array;     // 数组（循环）
    int m_size;     // 元素个数
    int m_max_size; // 数组容量
    int m_front;    // 队首的下标
    int m_back;     // 队尾的下标
};

#endif