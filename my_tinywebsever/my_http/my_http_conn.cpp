#include "my_http_conn.h"
#include <mysql/mysql.h>
#include <fstream>

// 定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file .\n";

locker lock;
map<string, string> users;

// 初始化数据库连接
void http_conn::initmysql_result(connection_pool *pool)
{
    MYSQL *mysql = NULL;
    // 从数据库连接池中取出一个数据库连接
    connectionRAII(mysql, pool);
    // 查询用户表，选出用户名以及密码
    if (mysql_query(mysql, "SELECT username, passwad FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }
    // 将查询结果全部取出，放入内存结果对象中
    MYSQL_RES *result = mysql_store_result(mysql);
    // 获取查询结果中的字段数
    int num_filelds = mysql_num_filelds(result);

    // 返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    // 逐行取出用户名以及对应密码  mysql_fetch_row返回值为一个指针数组
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}

// 将文件描述符设置为非阻塞
int setnonblocking(int fd)
{
    // F_GETFL 表述取出当前fd的属性
    int old_option = fctnl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    // F_SETEL 表示设置fd的属性
    fctnl(fd, F_SETEL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;
    if (TRIGMode == 1)
    {
        event.events = EPOLLIN | EPPOLLRDHUP | EPOLLET;
    }
    else
    {
        event.events = EPOLLIN | EPOLLRDHUP;
    }
    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 从内核事件表中删除描述符
void removefd(int epollfd, int fd)
{
    // 从 epollfd中删除
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    // 释放fd所占用的资源
    close(fd);
}

// 将事件重置为EPOLLONESHOT
// 当某个事件触发一次后，epoll 就不会再监听它，直到你重新用 modfd() 把它“激活”回来
// 所以这里叫重置 EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;
    if (TRIGMode == 1)
    {
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    }
    else
    {
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    }
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

void http_conn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        printf("close %d\n", m_sockfd);
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

// 外部建立连接时调用
void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
                     int close_log, string user, string passwd, string sqlname)
{
    m_sockfd = sockfd;
    m_address = addr;
    m_TRIGMode = TRIGMode;
    addfd(m_epollfd, m_sockfd, true, m_TRIGMode);
    m_user_count++; // 多线程中是否上锁

    doc_root = root;
    m_close_log = close_log;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());
    // 调用无参init，对其他参数进行初始化
    init();
}

// 初始化新接受的连接，重置参数
void http_conn::init()
{
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_but, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

// 从状态机，用于分析出一行的内容
// 返回值为行的读取状态，LINE_OK, LINE_BAD,LINE_OPEN
// 所以函数前面半段，是返回值类型
// 这就是用来判断读缓冲区的数据是否读完，如果读完了某一部分（请求行，请求头，请求主体），就将结尾的'\r\n'， 替换为'\0\0' ,方便处理

// 解析读缓冲区中的一行 HTTP 数据。
// 作用：判断缓冲区中是否已经读到完整的一行（以 \r\n 结尾）。
// 如果读到完整行（请求行或请求头），
// 就把行尾的 '\r\n' 替换为 '\0\0'，
// 方便后续把这一行当作 C 字符串处理。
// 返回值：
//   LINE_OK   - 解析到完整一行
//   LINE_OPEN - 数据尚未完整（还没读到 \r\n）
//   LINE_BAD  - 行格式错误
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for (; m_checked_idx < m_read_idx; m_checked_idx++)
    {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r') //  \r表示回车
        {
            if ((m_checked_idx + 1) == m_read_idx)
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                // 后自增，表明先使用，使用完之后在指向下一个，这两个行表明将'\r\n', 均替换为'\0\0'（变为C语言字符串）,方便之后处理
                m_read_buf[m_checked_idx++] = '\0';
                // 全部设置完之后，m_checked_idx又会+1 ,指向下一行开头
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            // 格式错误
            return LINE_BAD;
        }
    }
    // 未读完
    return LINE_OPEN;
}

bool http_conn::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false
    }
    int bytes_read = 0;
    // LT模式
    if (m_TRIGMode == 0)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read <= 0)
        {
            return false;
        }
        m_read_idx += bytes_read;
    }
    // ET模式
    if (m_TRIGMode == 1)
    {
        while (true)
        {
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            if (bytes_read == -1)
            {
                // 在ET模式下，当recv函数返回值小于0， 且errno为EAGAIN 或者EWOULDBLOCK，则认为连接正常，数据没有读完
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    break;
                }
                return false;
            }
            // ET模式下，返回值为0，则表示正常关闭连接
            else if (bytes_read == 0)
            {
                return false;
            }
            m_read_idx += bytes_read;
        }
        return true;
    }
}

// 解析http报文的请求行,获得请求方法，url,http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    m_url = strpbrk(text, "\t"); // 检索str1(text)，当遇到str2(\t)中包含的字符时，将该字符位置的前的str1中的字符全部去掉
    if (!m_url)
        return BAD_REQUEST;
    // 这里将指针指向的空格(\t)改为 字符串结束符号(\0),从而将text截为两部分
    *m_url++ = '\0';
    char *method = text;
    if (strcasecmp(method, "GET") == 0)
    {
        m_method = GET;
    }
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }
    else
    {
        return BAD_REQUEST;
    }
    // 经过前面的处理，m_url应该指向URL，但是为了防止URL前面存在空格，要进行这样的处理
    // 让m_url指向有效的URL字符
    m_url += strspn(m_url, '\t');
    m_version = strpbrk(m_url, '\t');
    if (!m_version)
    {
        return BAD_REQUEST;
    }
    // 将URL以及版本号之间的第一个空格变为结束符
    *m_version++ = '\0';
    // 同样，找出版本号前多余的空格，并移动指针，使其指向有效字符
    m_version += strspn(m_version, '\t');
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    // 下面两个if语句是用来剥掉协议头
    if (strncasecmp(m_url, "HTTP://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }
    // 如果上面没找到 /，或者路径不是以 / 开头，直接返回 400
    if (!m_url || m_url[0] != '/')
    {
        return BAD_REQUEST;
    }
    // 路径只剩 "/"时，默认给judge.htm这个页面
    if (strlen(m_url))
    {
        strcat(m_url, "judge.html");
    }
    // 告诉主循环“请求行已合格，但头部还没收完，继续读数据”
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

// 解析请求头
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    if (text[0] == '\0')
    {
        // 判断之后是否有请求主体
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, '\t');
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, "\t");
        // 将字符串形式的数据转换为long
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, "\t");
        m_host = text;
    }
    else
    {
        LOG_INFO("oop!unknow header: %s", text);
    }
    return NO_REQUEST;
}

// 解析请求主体
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    if (m_read_idx >= (m_checked_idx + m_content_length))
    {
        text[m_content_length] = '\0';
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST
}

// 主状态机
http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;
    while (((line_status = parse_line()) == LINE_OK) ||
           (m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK))
    {
        text = get_line();            // 指向读缓冲区的首行
        m_start_line = m_checked_idx; // 每次从状态机parse_line()函数调用时，会使m_checked_idx指向下一行首行
        LOG_INFO("%s", text);
        switch (m_check_state)
        {
        case CHECK_STATE_RESQUESTLINE:
        {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST)
                return do_request();
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
                return do_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}
// 为HTTP响应准备数据   找到请求的资源文件、映射进内存、并准备好后续发送
http_conn::HTTP_CODE http_conn::do_request()
{
    // 获取根目录，复制到m_real_file中
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);

    // 假设m_url = "/2CGIServer" 此时p指向 /
    const char *p = strrchr(m_url, '/');
    // 2表示登录，3表示注册（但这些都是post请求）
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {
        char flag = m_url[1];
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, '/');
        strcat(m_url_real, m_url + 2);
        // 限制拷贝的长度
        strncpy(m_real_file + len, m_url_real, FINENAM_LEN - len - 1);
        free(m_url_real);

        // 将用户名和密码提取出来
        // user=123&passwd=123
        char name[100], passwd[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
        {
            name[i - 5] = m_string[i];
        }
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
        {
            passwd[j] = m_string[i];
        }
        passwd[j] = '\0';
        // 3 表示注册，先判断是否存在
        // 不存在则继续注册
        if (*(p + 1) == '3')
        {
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "','");
            strcat(sql_insert, "'");
            strcat(sql_insert, passwd);
            strcat(sql_insert, "')");

            // 在map中没有找到对应的名字，说明未注册
            lock.lcok();
            if (users.find(name) == users.end())
            {

                // 把sql_insert语句发送给MYSQL，让服务器执行，返回值为0表示成功
                int res = mysql_query(mysql, sql_insert);
                // 往map中插入数据
                users.insert(pair<string, string>(name, passwd));
                lock.unlock();
                free(sql_insert);

                if (!res)
                {
                    strcpy(m_url, "/log.html");
                }
                else
                {
                    strcpy(m_url, "/registerError.html");
                }
            }
            else
            {
                strcpy(m_url, "/registerErro.html");
            }
        }
        // 登录
        else if (*(p + 1) == '2')
        {
            // 在map容器中可以找到名字，并且对应的密码也一样
            if (users.find(name) != users.end() && users[name] == passwd)
            {
                strcpy(m_url, "/welcome.html");
            }
            else
            {
                strcpy(m_url, "/logError.html");
            }
        }
    }
    if (*(p + 1) == '0')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (*(p + 1) == '6')
    {
        // 使用C++风格
        string m_url_real = "/video.html";
        // strncpy(m_real_file + len, m_url_real.c_str(), m_url_real.length());
        // 使用snprinf可以直接在字符串的结尾添加 \0 , 使用strncpy, m_url_real >= strlen(m_url_real)时，不会在末尾加 \0
        snprintf(m_real_file + len, FILENAME_LEN - len - 1, "%s", m_url_real.c_str());
    }
    else if (*(p + 1) == '7')
    {
        string m_url_real = "/fans.html";
        snprintf(m_real_file + len, FILENAME_LEN - len - 1, "%s", m_url_real.c_str());
    }
    else
    {
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    }
    // 使用stat检查文件是否存在（路径是否能找到文件）
    // 让结构体m_file_stat中写入m_real_file的信息（m_real_file为文件路径）
    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;
    // 判断文件时候可读，如果文件不可读，返回FORBIDDEN_REQUEST
    if (!(m_file_stat.st_mode & = S_IROTH))
    {
        return FORBIDDEN_REQUEST;
    }
    // 如果是目录，返回BAD_REQUEST
    if (S_ISDIR(m_file_stat.st_mode))
    {
        return BAD_REQUEST;
    }
    // 以只读方式获取文件描述符，通过mmap将该文件描述符映射到内存
    int fd = open(m_real_file, O_RDONLY);
    // 这里的m_file_address表示文件内容在内存中的映射地址，之后就可以直接操作---像操作数组一样直接访问文件内容
    // m_file_stat.st_size 就是文件长度，告诉 mmap 需要映射多少字节
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    // 请求文件存在，可以访问
    return FILE_REQUEST;
}

// 解除内存映射
void http_conn::unmap()
{
    if (m_file_address)
    {
        // 解除之前用 mmap() 建立的内存映射，把映射区域从进程地址空间中移除。
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

// 向写缓冲区写入格式化字符串，用于构建HTTP响应数据
// stat 专门用于可变参数
bool http_conn::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
    {
        return false;
    }
    va_list arg_list;
    //  va_start 初始化可变参数列表，从 format 之后的参数开始读取。
    va_start(arg_list, format);
    // m_wirte_buf + m_write_idx 写入当前位置
    int len = vsprintf(m_wirte_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        // 如果超过，释放可变参数资源
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    // 释放可变参数资源
    va_end(arg_list);
    // 可变参数就是要传入char * 或者 const char *类型
    LOG_INFO("request: %s", m_write_buf);
    return true;
}
// 往缓冲区中写入HTTP响应行
bool http_conn::add_status_line(int status, const char *title)
{
    return add_response(" %s %d %s \r\n", "HTTP/1.1", status, title);
}
// 响应头中的响应体长度
bool http_conn::add_content_length(int content_len)
{
    return add_response(" Content-Length: %d\r\n", content_len);
}
// 响应头中的响应内容类型
boot http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}
// 是否保持连接
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
// 添加空行
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}
// 通过调用上述函数，完成响应头
bool http_conn::add_headers(int content_len)
{
    return add_content_length(content_len) && add_linger() && add_blank_line();
}
// 完成响应体
bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}

// 开始调用上述函数，将响应行，响应行以及响应体中写入具体数据
bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    // 服务器内部错误 500
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        // 写缓冲区可能空间不足，有失败的情况
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
        add_status_line(404, erro_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        // 这里是成功响应，并要判断文件大小
        if (m_file_stat.st_size != 0)
        {
            // 因为 响应头和响应行在写缓冲区中，文件在映射的内存地址中。两者在不同的地方。
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;    // iov_base 缓冲区起始地址
            m_iv[0].iov_len = m_write_idx;     // iov_len 缓冲区长度
            m_iv[1].ion_base = m_file_address; // m_file_address 是通过 mmap 映射得到的文件在内存中的首地址
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2; // 缓冲区个数
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
    }
    default:
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iv_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

// 发送HTTP响应数据
bool http_conn::write()
{
    int temp = 0;

    // 没有要发送的数据
    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        init();
        return true;
    }

    while (1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);
        // 发送失败 ，temp < 0
        if (temp < 0)
        {
            // errno == EAGAIN：说明发送缓冲区满了，不能继续写
            // 重新注册写事件
            if (error == EAGAIN)
            {
                // 重新启用epoll监听（因为使用了oneshot，所以监听一次，这个fd就会停止）
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            unmap();
            return false;
        }
        bytes_have_send += temp;
        bytes_to_send -= temp;
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0; // 写缓冲区发送完毕
            // 这时，响应体文件可能也发送也一部分，所以要重新定位m_iv[1]的首地址以及长度
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m.iv[0].iov_len - bytes_to_send;
        }
        if (bytes_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
            // 判断是否保持连接
            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}

// 封装上述
void http_conn::process()
{
    // 解析客户端请求
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return true;
    }
    // 生成HTTP响应数据
    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
    }
    // 如何HTTP响应数据生成成功，修改fd为可写事件
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}