#ifndef MY_HTTP_CONN_H
#define MY_HTTP_CONN_H
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
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
#include <map>

#include "../my_lock/my_lock.h"
#include "../my_CGImysql/my_sql_connection_pool.h"
#include "../my_log/log.h"
#include "../my_timer/my_lst_timer.h"

class http_conn
{
public:
    // 文件名长度
    static const int FILENAME_LEN = 200;
    // 读缓冲区大小
    static const int READ_BUFFER_SIZE = 2048;
    // 写缓冲区大小
    static const int WRITE_BUFFER_SIZE = 1024;

    // http方法，常用的为GET,POST
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    // 主状态机，分别为请求行，请求头，消息体内容
    enum CHECK_STATE
    {
        // 解析从请求行开始，所以默认为0
        CHEAK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    // http解析结果
    enum HTTP_CODE
    {
        NO_REQUEST,        // 请求不完整
        GET_REQUEST,       // 完整的GET请求
        BAD_REQUEST,       // 请求有语法错误
        NO_RESOURCE,       // 文件不存在
        FORBIDDEN_REQUEST, // 权限不足
        FILE_REQUEST,      // 请求文件成功
        INTERNAL_ERROR,    // 服务器内部错误
        CLOSED_CONNECTION  // 连接已关闭
    };
    // 行解析状态（从状态机）
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    htttp_conn();
    ~http_conn();

public:
    // 初始化socket，并且函数内部调用私有方法init
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);
    // 关闭连接
    void close_conn(bool real_close = true);
    void process();
    bool read_once();
    bool write();
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    void initmysql_result(connection_pool *connpool);
    int timer_flag; // 定时器状态与改进标志，用于超时处理
    int improv;     // 标记线程池是否已处理完成该任务

private:
    // 这里的初始化函数init，是用于上面的init函数内部调用
    void init();
    // 主状态机解析整个请求
    HTTP_CODE process_read();
    // 根据解析结果生成响应
    bool process_write(HTTP_CONDE ret);
    // 解析请求行
    HTTP_CODE parse_request_line(char *text);
    // 解析请求头
    HTTP_CODE parse_headers(char *text);
    // 解析请求主体
    HTTP_CODE parse_content(chaar *text);
    HTTP_CODE do_request();
    // 返回当前正在解析的http当前行的地址
    char *get_line() { return m_read_buf + m_start_line; };
    // 从状态机函数
    LINE_STATUS parse_line();
    // 释放内存映射文件
    // 如果服务器用 mmap() 将磁盘上的 HTML 文件映射到内存（用于快速发送静态文件内容），
    // 发送完毕后，就要调用 unmap() 来解除映射，防止内存泄漏。
    void unmap();
    // 向写缓冲区 m_write_buf 添加格式化内容(万能写入器)
    bool add_response(const char *format, ...);
    // 把实际要返回的 HTML 内容（或错误提示）写入响应体部分
    bool add_content(const char *content);
    // 生成响应行
    bool add_status_line(int status, const char *title);
    // 添加所有相应头部字段
    bool add_headers(int content_length);
    // 添加contnet_type字段，告诉浏览器响应体内容的类型 例如 HTML、JSON、图片等
    bool add_content_type();
    // 添加 content_length字段，告诉响应体的字段长度
    bool add_content_length(int content_length);
    // 添加 Connection 头部字段，说明是否保持连接（长连接）
    bool add_linger();
    // 在头部和正文之间插入一个空行
    // HTTP 协议规定：
    // 响应头和响应体之间必须以空行分隔（\r\n）
    bool add_blank_line();

public:
    static int m_epollfd;    // 共享的epoll描述符
    static int m_user_count; // 统计当前服务器连接的客户端数量
    MYSQL *mysql;
    int m_state; // Reactor中的读写模式，0表示读，1表示写
private:
    int m_sockfd;                      // 客户端socket
    sockaddr_in m_address;             // 客户端地址
    char m_read_buf[READ_BUFFER_SIZE]; // 读缓冲区
    long m_read_idx;                   // 当前读到的下标
    long m_checked_idx;                // 当前解析到的位置
    int m_start_line;                  // 当前解析开始的位置
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;
    CHECK_STATE m_check_state;      // 主状态机状态
    METHOD m_method;                // 请求方法
    char m_real_file[FILENAME_LEN]; // 请求文件完整路径
    char *m_url;                    // url
    char *m_version;                // 版本号
    char *m_host;                   // 主机号
    long m_content_length;          // 请求体长度
    bool m_linger;                  // 是否保持连接
    char *m_file_address;           // 文件映射到内存的地址
    struct stat m_file_stat;        // 文件状态信息
    struct iovec m_in[2];           // 用于 writev 分散写的两个缓冲区（头+文件）
    int m_iv_count;
    int cgi;             // 是否启用POST
    char *m_string;      // 存储请求头数据
    int bytes_to_send;   // 要发送的字节数
    int bytes_have_send; // 已发送的字节数
    char *doc_root;      // 网站根目录路径

    map<string, string> m_users; // 用于红黑的键值映射 --->  用户名密码映射表（登录验证）
    int m_TRIGMode;              // 触发模式（LT 或 ET）
    int m_close_log;             // 是否关闭日志

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
}

#endif