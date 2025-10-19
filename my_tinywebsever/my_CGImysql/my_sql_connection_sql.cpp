#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "my_sql_connection_pool.h"
#include "../my_log/log.h" //写在.h文件与写在cpp文件中有什么区别

using namespace std;

connection_pool::connection_pool()
{
    m_CurConn = 0;
    FreeConn = 0;
}

connection_pool::~connection_pool()
{
    DestoryConn();
}

// 函数声明时加static，在定义的时候就不用加static了
connection_pool *connection_pool::GetInstance()
{

    static connection_pool pool;
    return &pool;
}

void connection_pool::init(string url, string User, string Password, string DBName, int port, int MaxConn, int close_log)
{
    m_url = url;
    m_port = port;
    m_User = User;
    m_PassWord = Password;
    m_Databasename = DBName;
    m_close_log = close_log;

    for (int i = 0; i < MaxConn; i++)
    {
        MYSQL *conn = NULL;
        // 使用mysql_init以及 mysql_real_connecet要有接收对象
        conn = mysql_init(conn);
        if (conn == NULL)
        {
            LOG_ERROR("mysql ERROR");
            exit(1);
        }
        conn = mysql_real_connect(conn, url.c_str(), User.c_str(), Password.c_str(), DBName.c_str(), port, NULL, 0);
        if (conn == NULL)
        {
            LOG_ERROR("MYSQL ERROR");
            exit(1);
        }
        connList.push_back(conn);
        ++FreeConn;
    }
    resever = sem(FreeConn);
    m_MaxConn = FreeConn;
}
MYSQL *connection_pool::GetConnection()
{

    MYSQL *conn = NULL;
    if (connList.size() == 0)
        return NULL;
    resever.wait(); // 相当于消费者，如果现在没有，就阻塞

    m_lock.lock();
    conn = connList.front(); // 这是取出元素
    connList.pop_front();    // list容器中的pop_front只是删除元素，没有取出的效果（没有返回值）
    m_CurConn++;
    FreeConn--;
    m_lock.unlock();
    return conn;
}

bool connection_pool::ReleaseConnection(MYSQL *conn)
{
    if (conn == NULL)
    {
        return false;
    }
    m_lock.lock();
    connList.push_back(conn);
    m_CurConn--;
    FreeConn++;
    m_lock.unlock();
    resever.post();
    return true;
}

void connection_pool::DestoryConn()
{
    m_lock.lock();
    if (connList.size() > 0)
    {
        list<MYSQL *>::iterator it; // 迭代器，相当于指针
        for (it = connList.begin(); it != connList.end(); it++)
        {
            MYSQL *con = *it; // 解引用，取出容器中的元素
            mysql_close(con); // 关闭连接
        }
        m_CurConn = 0;
        FreeConn = 0;
        connList.clear(); // 清除容器中的所有元素
    }
    m_lock.unlock();
}

// 当前空闲的连接数
int connection_pool::GetFreeConn()
{
    return this->FreeConn;
}

connectionRAII::connectionRAII(MYSQL **conn, connection_pool *pool)
{
    // 这一步是将获取数据库连接指针的操作也放在函数中，避免在外面手动写pool->GetConnection()。符合RAII思想
    *conn = pool->GetConnection();
    conRAII = *conn;
    poolRAII = pool;
}
connectionRAII::~connectionRAII()
{
    // 作用域结束后，自动释放
    poolRAII->ReleaseConnection(conRAII);
}
