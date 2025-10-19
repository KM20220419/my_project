#ifndef MY_SQL_CONNECTION_POOL_H
#define MY_SQL_CONNECTION_POOL_H

#include <iostream>
#include <stdio.h>
#include <mysql/mysql.h>
#include <list>
#include <error.h>
#include <string.h>
#include <string>

#include "../my_lock/my_locker.h"
#include "../my_log/log.h"

class connection_pool
{
public:
    static connection_pool *GetInstance(); // 创建单例对象，数据库连接池

    MYSQL *GetConnection();              // 获取数据库连接，返回的是指向这个连接的指针
    bool ReleaseConnection(MYSQL *conn); // 释放当前连接
    int GetFreeConn();                   // 获取空闲的连接数
    void DestoryConn();                  // 销毁连接

    void init(std::string url, std::string User, std::string PassWord, std::string DataBaseName, int Portm, int MaxConn, int close_log);

public:
    std::string m_url; // 主机地址
    std::string m_port;
    std::string m_User;
    std::string m_PassWord;
    std::string m_Databasename;
    int m_close_log; // 日志开关

private:
    // 写在私有成员中防止外部创建
    connection_pool();
    ~connection_pool();

    int m_MaxConn;               // 最大连接数
    int m_CurConn;               // 当前已使用的连接数
    int FreeConn;                // 当前空闲的连接数
    locker m_lock;               // 互斥锁
    std::list<MYSQL *> connList; // 连接池
    sem resever;
};

class connectionRAII
{
public:
    connectionRAII(MYSQL **con, connection_pool *connpool); // 这里的MYSQL **con 使用了地址传递，也可以写为MYSQL * &con
    ~connectionRAII();

private:
    MYSQL *conRAII;            // 用于数据库连接
    connection_pool *poolRAII; // 指向数据池
};

#endif
