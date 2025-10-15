#include "my_lst_timer.h"
#include "../http/http_conn.h"

sort_timer_lst::sort_timer_lst()
{
    head = NULL;
    tail = NULL;
}

sort_timer_lst::~sort_timer_lst()
{
    util_timer *tmp = NULL;
    while (head != NULL)
    {
        tmp = head->next; // 指向当前定时器的下一个
        delete head;
        head = tmp;
    }
}

void sort_timer_lst::add_timer(util_timer *timer)
{
    // timer为空指针
    if (timer == NULL)
    {
        return
    }
    // 链表为空
    if (head == NULL)
    {
        head = tail = timer;
        return
    }
    // 如果传入的定时器的过期时间最短，将其放入链表的队头
    if (timer->expire < head->expire)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return
    }
    // 否则，调用函数重载，放入链表的中部或者尾部
    add_timer(timer, head);
}

// 所需参数为，想传入的定时器以及链表
void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head)
{
    util_timer *h = lst_head;
    until_timer *tmp = h->next;
    while (tmp)
    {
        if (timer->expire < tmp->expire)
        {
            h->next = timer;
            tmp->prev = timer;
            timer->prev = h;
            timer->next = tmp;
            return
        }
        h = tmp;
        tmp = h->next;
    }
    // 此时，h指向队尾
    h->next = timer;
    timer->prev = h;
    tail = timer;
    return
}

void sort_timer_lst::adjust_timer(util_timer *timer)
{
    if (!timer)
    {
        return
    }
    util_timer *tmp = timer->next;
    // 修改了过期时间，但是还是比后面的小---不变
    if (!tmp || timer->expire < tmp->expire)
    {
        return
    }

    if (timer == head) // 修改的是头部的timer
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    else
    {
        timer->next->prev = timer->prev;
        timer->prev->next = timer->next;
        timer->prev = NULL;
        timer->next = NULL;
        add_timer(timer, timer->next);
        return
    }
}

void sort_timer_lst::del_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    if ((timer == head) && (timer == tail))
    {
        // 这两步的顺序不能错
        // 要先释放内存，然后置空
        // 可以理解为：如果先将head置为空，就无法找到timer这个结构体所在的地址
        delete timer;
        head = tail = NULL;
        return
    }
    if (timer == head)
    {
        head = timer->next;
        head->prev = NULL;
        delete timer;
        timer->next = NULL;
        return
    }
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
    return
}

void sort_timer_lst::tick()
{
    if (!head)
    {
        return
    }
    time_t cur = time(NULL);
    util_timer *tmp = head;
    while (tmp)
    {
        if (cur < tmp->expire)
        {
            break;
        }
        tmp->cb_func(tmp->user_data); // 关闭客户端连接
        // 删除节点
        head = tmp->next;
        if (head)
        {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}

// 设置“定时器触发间隔”的参数。
// 比如设置 timeslot = 5，那么服务器每隔 5 秒触发一次 SIGALRM 信号，用来检查超时连接
void Utils::init(int timeslot)
{
    my_TIMESLOT = timeslot;
}

// 将文件描述符设置为非阻塞
int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SFTFL, new_option);
    return old_option;
}

// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
// 把客户端连接（fd）添加到 epoll 监听列表中，并配置其触发行为
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    eppll_event event;

    event.data.fd = fd;
    // 设置边缘触发模式
    if (1 == TRIGMode)
    {
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    else
    {
        event.events = EPOLLIN | EPOLLRDHUP
    }
    // 一次性触发
    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    // ET模式下，要保证socket为非阻塞模式
    setnonblocking(fd);
}

// 信号处理函数 用于将捕获到的信号写入管道，以通知主线程
// 用管道将“信号事件”转化为“可读事件”，让 epoll 主循环安全地处理信号。
void Utils::sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

// 设置信号函数
// handler写在函数的参数列表中，不加*，也等同于函数指针，即addsig(int sig, void(*handler)(int), bool restart)
// 但是写在其他地方就不能等同
void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    // sig 为要绑定的信号（例如 SIGALRM, SIGTERM, SIGINT 等）
    struct sigcation sa;
    memset(&sa, "\0", sizeof(sa));
    sa.sa_handler = handler; // 为信号绑定信号处理函数 如sig_handler信号处理函数
    if (restart)
        sa.sa_flags |= SA_RESTART; // SA_RESTART 表示系统调用被信号打断后自动重启；
    sigfillset(&sa.sa_mask);       // 表示在处理这个信号期间，屏蔽所有其他信号
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    my_timer_lst.tick();
    alarm(my_TIMESLOT); // 定时，多久触发一次
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

// 初始化静态成员变量
int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils; // 前向声明
// 回调函数-->关闭超时的客户端
void cb_func(client_data *user_data)
{
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, use_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}