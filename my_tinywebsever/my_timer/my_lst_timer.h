#ifndef MY_LST_TIMER_H
#define MY_LST_TIMER_H

#include <iostream>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/type.h>
#include <sys/epoll.h>
#include <fcntl.h> //用于设置非阻塞
#include <sys/socket.h>
#include <netinet.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
#include "../log/log.h"

class util_timer;

struct client_data // 绑定客户端和定时器的结构体
{
    sockaddr_in address;
    int sockfd;
    util_timer *timer;
}

class util_timer
{
public:
    // 初始化，将定时器结构的前后指针先置为空
    util_timer() : prev(NULL), next(NULL) {};

public:
    time_t expire; // 定义超时时间

    void (*cb_func)(client_data *);
    client_data *uer_data;
    util_timer *prev; // 前向指针
    util_timer *next; // 后向指针
}

class sort_timer_lst // 管理多个util_timer类，使用升序双向链表
{
public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(util_timer *timer);
    // 当定时器的超时时间 (expire) 发生变化时，调整它在升序链表中的位置，以保持链表的有序性。
    void adjust_timer(util_timer *timer);
    void del_timer(util_timer *timer);
    void tick();

private:
    // 函数重载
    void add_timer(uutil_timer *timer, util_timer *lst_head);
    util_timer *head;
    util_timer *tail;
}

class Utils // 服务器的辅助功能类，用于定时器、信号以及epoll的集成
{
public:
    Utils() {};
    ~Utils() {};
    void init(int timeslot);

    // 将fd设置为非阻塞
    int setnonblocking(int fd);

    // 将内核事件表注册读事件，ET模式， 选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    // 信号处理函数
    static void sig_handler(int sig);

    // 设置信号函数
    void adsig(int sig, void(handler)(int), bool restart = true);

    // 定时处理任务， 重新定时以不断触发SIGALRM
    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    sort_timer_lst my_timer_lst;
    static int u_epollfd;
    int my_TIMESLOT;
};

void cb_func(client_data *user_data);

#endif