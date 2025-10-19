#include "my_websever.h"

WebSever::WebSever()
{
    // 为每个连接创建http_conn对象
    users = new http_conn[MAX_FD];

    // root文件夹路径
    char server_path[200];
    // 获取当前工作目录路径
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    // 为每个连接构建定时器
    users_timer = new client_data[MAX_FD];
}
WebSever::~WebSever()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

// 初始化
void WebSever::init(int port, string user, string passWord, string databaseName, int log_write,
                    int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_model)
{
    m_port = port;                 // 端口号
    m_user = user;                 // 数据库用户名
    m_passWord = passWord;         // 数据库密码
    m_databaseName = databaseName; // 数据库名
    m_sql_num = sql_num;           // 数据库连接池的数量
    m_thread_num = thread_num;     // 线程池的数量
    m_log_write = log_write;       // 日志的写入方式
    m_POT_LINGER = opt_linger;     // 用于控制关闭连接时的延迟发送
    m_TRIGMode = trigmode;         // 触发模式 LT ET
    m_close_log = close_log;       // 控制日志关闭
    m_actormodel = actor_model;    // 并发模式选择： 0 Poractor  1 Reactor
}

void WebSever::trig_mode()
{
    // LT LT
    if (m_TRIGMode == 0)
    {
        LOG_INFO("m_TRIGMode == 0");
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
    }
    // LT + ET
    else if (m_TRIGMode == 1)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
    // ET + LT
    else if (m_TRIGMode == 1)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }
    // ET + ET
    else if (m_TRIGMode == 2)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
}

// 初始化日志
void WebSever::log_write()
{
    if (m_close_log == 0)
    {
        if (m_log_write == 1) // 选择异步日志模式
        {
            // 初始化日志系统
            log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
        }
        else
        {
            log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
        }
    }
}

//
void WebSever::sql_pool()
{
    // 初始化数据库连接池
    m_connpool = connection_pool::GetInstance();
    m_connpool->init("localhost", m_user, m_passWord, m_databaseName, 3306, m_sql_num, m_close_log);
    // 初始化数据库读取表
    users->initmysql_result(m_connpool);
}

void WebSever::thread_pool()
{
    // 模板类线程池，任务类型为http_conn
    m_pool = new threadpool<http_conn>(m_actormodel, m_connpool, m_thread_num);
}

void WebSever::eventListen()
{
    // 网络编程基础步骤
    // LOG_INFO("eventListen");
    m_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    // 优雅关闭连接
    if (m_POT_LINGER == 0)
    {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if (m_POT_LINGER == 1)
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    // 允许端口复用
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    // 初始化定时器管理
    utils.init(TIMESLOT);

    // epoll创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5); // 参数5没有用
    assert(m_epollfd != -1);

    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    // 保证所有的http_conn对象调用同一个epoll
    http_conn::m_epollfd = m_epollfd;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    utils.setnonblocking(m_pipefd[1]);
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    // 添加信号处理
    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    // 在指定的秒数后向进程发送 SIGALRM 信号
    alarm(TIMESLOT);
    // 静态成员变量赋值
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

// 接收客户端连接后，进行一系列的定时器操作
void WebSever::timer(int connfd, struct sockaddr_in client_address)
{
    LOG_INFO("dealclientdata->timer");
    // 初始化http连接
    users[connfd].init(connfd, client_address, m_root, m_CONNTrigmode, m_close_log, m_user, m_passWord, m_databaseName);

    // 初始化client_data
    // 创建定时器，设置回调函数和超时时间，绑定用户，将定时器添加到链表中
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    // 将定时器绑定到客户端中
    users_timer[connfd].timer = timer;
    // 添加到链表中
    utils.my_timer_lst.add_timer(timer);
    LOG_INFO("timer-->end");
}

// 若有数据传输，则将定时器往后延时3个单位
// 并将新的定时器在链表上的位置调整
void WebSever::adjust_timer(util_timer *timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.my_timer_lst.adjust_timer(timer);
    LOG_INFO("%s", "adjust timer once");
}

void WebSever::deal_timer(util_timer *timer, int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if (timer)
    {
        utils.my_timer_lst.del_timer(timer);
    }
    LOG_INFO("deal_timer close fd %d", users_timer[sockfd].sockfd);
}

// 处理客户端连接请求
bool WebSever::dealclientdata()
{
    // LOG_INFO("dealclientdata");
    struct sockaddr_in client_address; // 用于获取客户端的IP地址，端口号
    // 获取结构体长度
    socklen_t client_addrlength = sizeof(client_address);
    // 根据m_LISTENTrigmode选择监听套接字的方法：LT(0)  ET(1)
    if (0 == m_LISTENTrigmode)
    {
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if (connfd < 0)
        {
            LOG_ERROR("%s: errno is:%d", "accept error", errno);
            return false;
        }
        if (http_conn::m_user_count >= MAX_FD)
        {
            utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        // LOG_INFO("0 == m_LISTENTrigmode");
        timer(connfd, client_address);
    }
    else
    {
        LOG_INFO("1 == m_LISTENTrigmode");
        // ET模式 要一直循环
        while (1)
        {
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if (connfd < 0)
            {
                LOG_ERROR("%s:error is: %d", "accept, error", errno);
                break;
            }
            if (http_conn::m_user_count >= MAX_FD)
            {
                utils.show_error(connfd, "Internel server busy");
                LOG_ERROR("%s", "Inter server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

// 处理信号事件
// 当信号触发时（比如定时器超时），信号处理函数会向管道的写端 m_pipefd[1] 写入信号值；
// 主线程的 epoll 正在监听管道的读端 m_pipefd[0]；
// 当 epoll 检测到可读事件时，就会调用本函数来读取这些信号并进行处理。
bool WebSever::dealwithsignal(bool &timeout, bool &stop_sever)
{
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1)
    {
        return false;
    }
    else if (ret == 0)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < ret; ++i) // i++ -->++i
        {
            switch (signals[i])
            {
            case SIGALRM: // 超时信号
            {
                timeout = true;
                break;
            }
            case SIGTERM: // 终止进程信号
            {
                stop_sever = true;
                break;
            }
            }
        }
    }
    return true;
}

void WebSever::dealwithread(int sockfd)
{
    // 对应客户端的定时器
    util_timer *timer = users_timer[sockfd].timer;
    // reactor
    if (m_actormodel == 1)
    {
        if (timer)
        {
            adjust_timer(timer);
        }
        m_pool->append(users + sockfd, 0); // users + sockfd是对应sockfd的http_conn地址，0表示读
        while (true)
        {
            if (1 == users[sockfd].improv) // improv 标记线程池是否已处理完成该任务
            {
                // timer_flag标记连接是否超时
                if (1 == users[sockfd].timer_flag)
                {
                    LOG_INFO("dealwithread---reactor---deal_timer");
                    deal_timer(timer, sockfd);

                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        // proactor
        if (users[sockfd].read_once())
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            // 检测到读事件，将该事件放入请求队列
            m_pool->append_p(&users[sockfd]);
            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            // LOG_INFO("1018---dealwithread---precator---deal_timer");
            deal_timer(timer, sockfd);
        }
    }
}

void WebSever::dealwithwrite(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;
    // reactor
    if (m_actormodel == 1)
    {
        if (timer)
        {
            adjust_timer(timer);
        }
        m_pool->append(users + sockfd, 1);

        while (true)
        {
            if (users[sockfd].improv == 1)
            {
                if (users[sockfd].timer_flag == 1)
                {
                    LOG_INFO("dealwithwrite---reactor---deal_timer");
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        if (users[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            // LOG_INFO("dealwithwrite---precator---deal_timer");
            deal_timer(timer, sockfd);
        }
    }
}

// 服务器的主循环
void WebSever::eventLoop()
{
    // LOG_INFO("eventLoop");
    bool timeout = false;    // 是否有定时器超时信号
    bool stop_sever = false; // 是否关闭服务器

    while (!stop_sever)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }
        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;

            // 处理新到的客户连接
            if (sockfd == m_listenfd)
            {
                bool flag = dealclientdata();
                if (flag == false)
                {
                    continue;
                }
            }
            // 处理连接异常事件
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                // 服务器端关闭连接，移除对应的定时器
                util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            // 处理信号
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                LOG_INFO("sockfd == m_pipefd[0]");
                // 调用信号处理函数，是超时还是关闭服务器
                bool flag = dealwithsignal(timeout, stop_sever);
                if (flag == false)
                {
                    LOG_ERROR("%s", "dealclientdata failure");
                }
            }
            // 处理客户连接上接受的数据
            else if (events[i].events & EPOLLIN) // 只要这个文件描述符可读
            {
                dealwithread(sockfd);
            }
            else if (events[i].events & EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }
        }
        // 超时信号
        if (timeout)
        {
            utils.timer_handler();
            LOG_INFO("%s", "eventLoop---time tick");
            timeout = false;
        }
    }
}