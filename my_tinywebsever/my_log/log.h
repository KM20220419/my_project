/*
author: fpy
date: 2025/0930

*/

#ifndef MY_LOG_H
#define MY_LOG_H

#include <iostream>
#include <string>
#include <stdarg.h>
// 引入线程库
#include <pthread.h>
// 用于底层文件的写入
#include <stdio.h>
// 引入互斥锁
#include "../my_lock/my_lock.h"
#include "block_queue.h"

using namespace std;

class log
{
public:
    // 创建单例模式--懒汉模式
    // 静态成员函数 使得可以不创建对象就可以调用这个成员函数 ---> log::get_instance来调用
    // 核心： 静态成员函数---> 属于类 而不属于对象
    static log *get_instance()
    {
        // 使用静态成员变量创建对象，保证在这个程序运行期间，只会有这一个实例
        static log instance;
        // 返回的是指向这个实例的指针
        return &instance;
    }

    // 初始化 参数   不写在构造函数中是方便可以灵活调节，并且
    bool init(const char *file_name, int close_log, int log_bif_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

    void write_log(int level, const char *format, ...);

    void flush(void);

    // 异步日志写入 -- 为线程的访问提供一个接口  pthread_create
    static void *flush_log_thread(void *args)
    {
        log::get_instance()->async_write_log();
    }

private:
    // 将析构函数写在私有成员中， 可以防止外部创建该类
    log();
    virtual ~log();

    void *async_write_log() // 这里的返回类型也可以为void   写成void* 估计是为了pthread_create,与上面flush_log_thread
    {
        string single_log;
        // 从阻塞队列中取出一个日志，写入文件
        // 循环将队列中的日志写入文件中
        while (m_log_queue->pop(single_log))
        {
            m_mutex.lock();
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
    }

    char dir_name[128];               // 路径名
    char log_name[128];               // log 文件名
    int m_split_lines;                // 单个日志文件的最大行数
    int m_log_buf_size;               // 日志缓冲区大小
    long long m_count;                // 日志总行数（用于判断是否需要分文件）
    int m_today;                      // 当前日期（按天分类日志）
    FILE *m_fp;                       // 打开当前日志的指针
    char *m_buf;                      // 日志缓冲区
    block_queue<string> *m_log_queue; // 阻塞队列（异步日志使用）
    bool m_is_aysnc;                  // 判断是否异步
    locker m_mutex;                   // 互斥锁
    int m_close_log;                  // 是否关闭日志
};
// ##__VA_ARGS__ 也是可变参数列表
#define LOG_DEBUG(format, ...)                                    \
    if (0 == m_close_log)                                         \
    {                                                             \
        Log::get_instance()->write_log(0, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }
#define LOG_INFO(format, ...)                                     \
    if (0 == m_close_log)                                         \
    {                                                             \
        Log::get_instance()->write_log(1, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }
#define LOG_WARN(format, ...)                                     \
    if (0 == m_close_log)                                         \
    {                                                             \
        Log::get_instance()->write_log(2, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }
#define LOG_ERROR(format, ...)                                    \
    if (0 == m_close_log)                                         \
    {                                                             \
        Log::get_instance()->write_log(3, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }

#endif