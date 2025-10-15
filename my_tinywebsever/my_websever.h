#ifndef MY_WEBSEVER_H
#define MY_WEBSEVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <error.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "./my_threadpool/my_threadpool.h"
#include "./my_http/my_http_conn.h"
#include "./my_log/log.h"

const int MAX_FD = 65536;           // 最大文件描述符
const int MAX_EVENT_NUMBER = 10000; // 最大事件数
const int TIMESLOT = 5;             // 最小超时单位

class WebSever
{
public:
    WebSever();
    ~WebSever();
    void init(int port, string user, string passWord, strign databaseName,
              int log_write, int opt_linger, int trigmode, int sql_num,
              int thread_num, int close_log, int actor_model);
    void thread_pool();                                        // 创建线程池
    void sql_pool();                                           // 初始化数据库连接池
    void log_write();                                          // 初始化日志系统
    void trig_mode();                                          // 设置监听/连接触发模式（LT/ET）
    void eventListen();                                        // 创建监听套接字并注册到 epoll
    void eventLoop();                                          // 服务器主循环，不断监听 epoll 事件并分发处理（读/写/超时/信号）。
    void timer(int connfd, struct sockaddr_in client_address); // 给新连接分配一个定时器
    void adjust_timer(util_timer *timer);                      // 当连接有活动时，更新时间。
    void deal_timer(util_timer *timer, int sockfd);            // 处理超时事件（关闭连接、释放资源）
    bool dealclientdata();                                     // 处理新客户连接
    bool dealwithsignal(bool &timerout, int sockfd);           // 处理信号（SIGALRM等）
    void dealwithread(int sockfd);                             // 处理读事件
    void dealwithwrite(int sockfd);                            // 处理写事件
public:
    // 基础
    int m_port;
    char *m_root;
    int m_log_write;
    int m_close_log;
    int m_actormodel;

    int m_pipefd[2];
    int m_epollfd;
    http_conn *users;

    // 数据库相关
    connection_pool *m_connpool;
    std::string m_user;
    std::string m_passWord;
    std::string m_databaseName;
    int m_sql_num;

    // 线程池
    threadpool<http_conn> *m_pool;
    int m_thread_num;

    // epoll_event相关
    epoll_event events[MAX_EVENT_NUMBER];
    int m_listenfd;
    int m_POT_LINGER;
    int m_TRIGMode;       // 总的模式
    int m_LISTENTrigmode; // 监听套接字的模式 LT  ET
    int m_CONNTrigmode;   // 连接套接字的模式 LT ET

    // 定时器相关
    client_data *users_timer;
    Utils utils;
}

#endif