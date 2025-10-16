# http解析

## 头文件

### 系统调用与进程控制

#### unistd.h

UNIX标准函数库，几乎所有的系统调用都来自这里。

包括：

- `read()`、`write()` ---文件/套接字读写
- `close()` --- 关闭文件描述符
- `fork()` --- 创建子进程
- `pipe()` --- 创建管道
- `sleep()`、 `usleep()`  ---延迟(睡眠)
- `getid()` --- 获取进程ID

#### sys/types.h

定义系统调用中使用的数据类型

- `pid_t` --- 进程ID类型
- `size_t` --- 数据大小
- `ssize_t` --- 有符号大小类型
- `time_t` --- 时间类型

这些底层类型支持文件，经常被其他头文件间接包含

#### sys/stat.h

文件属性相关函数：

- `stat()`、`fstat()`、`lstat()` --- 获取文件状态信息
- `chmod()` ---修改权限
- `mkdir()` --- 创建目录
- 结构体： `struct stat ` (记录文件大小，权限类型，时间戳等)
- <img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251013163033730.png" alt="image-20251013163033730" style="zoom: 50%;" />

#### fctnl.h

用于文件控制（常用将将socket设置为非阻塞）

提供：

- `open()` --- 打开文件
- `fctnl()` --- 改变文件属性，比如：`O_NONBLAOK`(用于非阻塞)、`FD_CLOEXEC`(执行时关闭)

#### sys/wait.h

进程等待函数：

- `wait()`、 `waitpid()` ---等待子进程退出

- 宏：`WIFEXITED(status)` 判断子进程是否正常退出

  `WEXITSTATUS(status)` 获取退出码

### 信号机制

#### signal.h

信号机制相关：

- `signal()` 、`sigaction()` ---注册信号处理函数
- `kill()` ---发送信号  常见信号：`SIGINT`（Ctrl + C)、`SIGTERM`(终止进程)、 `SIGPIPE`(向已关闭socket写数据)

### 线程与并发

#### pthread.h

POSIX现成数据库

- `pthread_create()` — 创建线程
- `pthread_join()` — 等待线程结束
- `pthread_detach()` — 设置分离状态
- `pthread_mutex_t` — 互斥锁
- `pthread_cond_t` — 条件变量

### socket相关

#### sys/socket.h

 socket 基础接口：

- `socket()` — 创建套接字
- `bind()`、`listen()`、`accept()` — 服务端套接字
- `connect()` — 客户端连接
- `send()`、`recv()` — 数据收发
- 结构体：
  - `sockaddr`、`sockaddr_in`

#### netinet/in.h

Internet网络协议定义：

- `struct sockaddr_in` — IPv4 地址结构体
- 宏定义：
- - `AF_INET`、`INADDR_ANY`
  - `htons()`、`htonl()`、`ntohs()`、`ntohl()` — 主机/网络字节序转换

#### arpa/inet.h

提供 IP 地址转换函数：

- `inet_addr()`、`inet_ntoa()` — IPv4 字符串 ↔ 数值互转
- `inet_pton()`、`inet_ntop()` — IPv4/IPv6 通用转换

> 与 `<netinet/in.h>` 配合使用。

### I/O模型与事件机制

#### sys/epoll.h

 **epoll 事件驱动模型**：

- `epoll_create()`、`epoll_ctl()`、`epoll_wait()`
- 结构体：
  - `struct epoll_event`
- 宏：
  - `EPOLLIN`（可读）
  - `EPOLLOUT`（可写）
  - `EPOLLET`（边缘触发）

#### sys/uio.h

**分散/聚集 I/O**：

- `readv()` / `writev()`
  - 一次读取多个缓冲区
  - 一次写入多个缓冲区

> 在 HTTP 服务器中常用于：
>
> - 一次发送响应头 + 响应体

### 内存映射与共享内存

#### sys/mman.h

 内存映射文件函数：

- `mmap()` — 将文件映射到内存
- `munmap()` — 解除映射

用途：

- 快速文件 I/O
- 进程间共享内存
- 实现零拷贝传输（HTTP 服务器中常用）

### 通用工具类库

#### string.h

C字符串操作：

`strlen()`、 `strcpy()`、 `strcmp()` 、 `memset()`、 `memcpy()`

#### stdio.h

标准输入输出：

`printf()`、`fprintf()`、`fopen()`、`fclose()`

#### stdlib.h

 通用实用函数：

- `malloc()`、`free()` — 内存管理
- `atoi()`、`itoa()` — 转换
- `rand()`、`srand()` — 随机数
- `exit()` — 退出程序

#### stdarg.h

 可变参数函数支持：

- `va_start()`、`va_arg()`、`va_end()`

例如：

```
void my_log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
```

#### errno.h

错误码定义：

- 全局变量 `errno` 表示上一次系统调用的错误号。
- 宏：
  - `EAGAIN`、`EINTR`、`EBADF`、`ECONNRESET` 等。

#### map

`#include<map>`

 C++ STL 容器：

- `std::map<K,V>`：基于红黑树的键值映射。
- 自动排序，查找效率 O(log n)。
- <img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251013150843982.png" alt="image-20251013150843982" style="zoom:50%;" />

## enum：枚举类型

enum枚举类型，是一种用户自定义类型，用于给一组整形常量取别名。

```
enum Color {
    RED,
    GREEN,
    BLUE
};
等价于：
const int RED = 0;
const int GREEN = 1;
const int BLUE = 2;

```

<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251010101748581.png" alt="image-20251010101748581" style="zoom:50%;" />

## http解析中的主状态机和从状态机

在高性能服务器（例如 WebServer 项目）中，**主状态机** 和 **从状态机** 通常一起配合，用于 **逐步解析 HTTP 请求报文**。

因为发送来的报文请求很长，可以分为请求行、请求头、请求体，这些内容不会一次读完。所以通过主状态机判断读取到哪一步。在读取上述三个中的某一个时，根据从状态机来判断时候读完。

<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251010160942692.png" alt="image-20251010160942692" style="zoom:50%;" />

## 结构体 stat

定义在头文件 `sys/stat.h`中

系统调用stat()或者fstat()用来获取文件信息并填充到定义的结构体stat中。

例如：

```
#include <sys/stat.h>
#include <iostream>
using namespace std;

int main() {
    struct stat file_stat;
    if (stat("/var/www/html/index.html", &file_stat) == 0) {
        cout << "文件大小：" << file_stat.st_size << " 字节" << endl;
    } else {
        perror("stat error");
    }
    return 0;
}

这样文件的信息，权限等便写入到结构体 file_stat中
```

结构体成员

<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251010204049725.png" alt="image-20251010204049725" style="zoom:50%;" />

<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251010204122332.png" alt="image-20251010204122332" style="zoom:50%;" />

## struct iovec m_iv[2]

用于writev()分散写的两个缓冲区

### writev()

在普通的write()调用中，只能一次性写一个连续的缓冲区。但是HTTP的响应由两部分组成：响应头以及响应主体（不在同一个内存块）。

​	如果用write()写效率太低，所以Linux提供了更高效的系统调用writev()

```
writev(int fd, const struct iovec *iov, int iovcnt);

```

它允许一次性将**多个非连续内存块**发送出去，
 从而减少系统调用次数，提高效率。

#### 结构体iovec定义

```
#include <sys/uio.h>

struct iovec {
    void  *iov_base;  // 缓冲区起始地址
    size_t iov_len;   // 缓冲区长度（字节数）
};

```

<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251010205155838.png" alt="image-20251010205155838" style="zoom:50%;" />

通过定义 int m_iv_count 表明在调用过程中实际使用的缓冲区个数。提供给writev()；



## strpbrk()函数

```
strpbrk(const char *str1,const char *str2)
//不加const 表示可以通过指针修改
strpbrk(char *str1,char *str2)
```

在一个字符串 `str1` 中，**从左到右扫描**，查找**第一个**出现在字符集合 `str2` 中的字符

返回值类型为 char *。

- **成功**：返回指向 `str1` 中**第一个**匹配字符的**指针**。这个指针可以直接用来进行后续的字符串操作。
- **失败**：如果在 `str1` 中完全找不到任何属于 `str2` 的字符，则返回**空指针 `NULL`**。
- 返回的指针指向的是 `str1` 中的原始位置，这意味着你可以通过修改这个指针指向的内容来改变原字符串 `str1`（但如果 `str1` 本身被声明为 `const`，则不能修改）。



## strcasecmp()函数

```
int strcasecmp (const char *s1, const char *s2);
//也可以是不加const
int strcasecmp (char *s1,char *s2);
```

strcasecmp 是 C 语言中用于**不区分大小写**比较两个字符串的函数，主要用于忽略字符大小写差异的场景（如用户[输入验证](https://so.csdn.net/so/search?q=输入验证&spm=1001.2101.3001.7020)、不区分大小写的字符串匹配等）。它属于 POSIX 标准库，定义在 `<string.h>` 头文件中。

**核心逻辑**：函数会先将两个字符串中的字符统一转换为相同大小写（通常是小写），再逐个字符比较其 ASCII 码值，直到遇到不同字符或字符串结束符 `'\0'`。

返回值类型：

​	返回值为0，表示两个字符串大小相等（忽略大小写）

## strncasecmp()函数

```
#include <strings.h>   /* Linux / POSIX */
int strncasecmp(const char *s1, const char *s2, size_t n);
```

`strncasecmp` 就是“**带长度限制的忽略大小写版 strncmp**”

​	`size_t` 是 C/C++ 标准里定义的 **一种无符号整数类型**，专门用来表示“**对象大小**”或“**数组索引范围**”。

​	`	int` 是 **有符号** 类型，可能为负；`size_t` **永远 ≥ 0**。

##   strspn()函数

```
 strspn(const char *str1, const char *str2)
 //同样 const可以去掉
 
```



就是从str1的第一个元素开始往后数，查看str1中是不是连续往后的每个字符都可以在str2中找到。直到第一个不在str2中的元素为止。

返回值为str1在str2中可以查找到的连续字符个数（从第一个开始）

## strchr()函数

```
#include <string.h>

char *strchr(const char *s, int c);   /* 普通指针版本 */
```

**在字符串中找第一次出现的某个字符，返回指向该字符的指针；找不到就返回 `NULL`。**



这里的第二位参数为要查找的标志，如 ' , '  ' : '等



## CGI技术

是一种早期的动态页面生成技术。

<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251013103404875.png" alt="image-20251013103404875" style="zoom:50%;" />



<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251013103508479.png" alt="image-20251013103508479" style="zoom:50%;" />