/*
  2025/10/03/00:41
*/

#include "log.h"
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <sys/time.h>
#include <iostream>

using namespace std;

log::log()
{
    m_count = 0;
    m_is_aysnc = false;
}
log::~log()
{
    // 确保文件要关闭
    if (m_fp != NULL)
    {
        fclose(m_fp);
    }
}
// 初始化
bool log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size)
{
    if (max_queue_size > 0)
    {
        // 表示使用异步方式写入日志
        m_is_aysnc = true;
        m_log_queue = new block_queue<string>(max_queue_size);
        pthread_t tid;
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }
    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    // 申请一块缓冲区
    m_buf = new char[m_log_buf_size];
    // 初始化缓冲区
    memset(m_buf, '\0', m_log_buf_size);
    m_split_lines = split_lines;

    // 获取当前时间点
    time_t t = time(NULL);

    // 传给localtime 返回的是一个指向结构体 tm 的指针
    struct tm *sys_tm = localtime(&t);
    // 由于 localtime返回的结构体 为 全局静态结构体， 所以之后的值会覆盖之前的
    // 因此，在定义一个结构体，将之前的拷贝到这里
    struct tm my_tm = *sys_tm;

    // 从字符串最后出现字符 '/' 的位置
    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {};

    // 这下面的if是用来根据路径，为log_full_name创建文件名
    if (p == NULL) // 表示文件只有文件名，没有路径
    {
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else
    {
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }

    m_today = my_tm.tm_mday;

    m_fp = fopen(log_full_name, "a");
    if (m_fp == NULL)
    {
        return false;
    }
    return true;
}

void log::write_log(int level, const char *format, ...)
{
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    // 为下面的level 准备字符串
    char s[16] = {0};
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]: ");
        break;
    case 1:
        strcpy(s, "[info]");
        break;
    case 2:
        strcpy(s, "[warn]");
        break;
    case 3:
        strcpy(s, "[erro]");
        break;
    default:
        strcpy(s, "[info]");
        break;
    }

    m_mutex.lock();
    m_count++; // 这里先加1，因为确保再写入一条日志之后，假如刚好达到分页的标准，就可以直接分页
    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0)
    {
        char new_log[256] = {0}; // 申请一个新的缓冲区（临时空间），用于存放文件名
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0}; // 用于存放当前最新的日期

        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        if (m_today != my_tm.tm_mday)
        {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else
        {
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        }
        m_fp = fopen(new_log, "a");
    }
    m_mutex.unlock();

    va_list valst; // 处理函数中的可变参数： ...
    va_start(valst, format);

    // 定义一个字符串对象 ---> 用于接收临时缓冲区的数据（字符串类型）
    string log_str;
    m_mutex.lock();
    // 写入的具体时间内容格式
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    int m = vsnprintf(m_buf + n, m_log_buf_size - n - 1, format, valst); // 这是可变参数的写入方式
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;

    m_mutex.unlock();

    if (m_is_aysnc && m_log_queue->Isfull())
    // 如果是异步写入，就将其放入阻塞队列中
    {
        m_log_queue->push(log_str); // 相当于生产者
    }
    else
    {
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }
    va_end(valst);
}

void log::flush(void)
{
    m_mutex.lock();
    // 强行将缓冲区内的数据写入磁盘中（刷新缓冲区）
    fflush(m_fp);
    m_mutex.unlock();
}