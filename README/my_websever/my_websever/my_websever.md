# Websever

## 头文件

```
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

```

这些是 **Linux 网络编程和系统调用** 的标准头文件：

- `sys/socket.h`, `netinet/in.h`, `arpa/inet.h`：用于套接字通信。
- `unistd.h`：close、read、write等系统调用。
- `errno.h`：错误码。
- `fcntl.h`：文件描述符属性控制，如非阻塞。
- `stdlib.h`, `stdio.h`, `cassert`：基础工具。
- `sys/epoll.h`：epoll 高并发 I/O 复用。

## struct linger结构体

<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251014204045587.png" alt="image-20251014204045587" style="zoom:50%;" />

## setsockopt函数

用于设置套接字选项

<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251014205021208.png" alt="image-20251014205021208" style="zoom:50%;" />

<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251014205054470.png" alt="image-20251014205054470" style="zoom:50%;" />

设置套接字选项的作用就是：

1. **控制连接关闭行为**（阻塞/非阻塞/立即中止）
2. **控制端口复用**，提高服务器灵活性
3. **调整发送/接收缓冲和超时**，提升性能和可靠性
4. **维护长连接健康**（心跳检测）
5. **实现高级网络功能**（组播、紧急数据等）



```
utils.addsig(SIGPIPE, SIG_IGN);
utils.addsig(SIGALRM, utils.sig_handler, false);
utils.addsig(SIGTERM, utils.sig_handler, false);

```

<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251015102630697.png" alt="image-20251015102630697" style="zoom:50%;" />

<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251015102644807.png" alt="image-20251015102644807" style="zoom:50%;" />

<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251015102659396.png" alt="image-20251015102659396" style="zoom:50%;" />

## accept()

<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251015140600799.png" alt="image-20251015140600799" style="zoom:50%;" />

```
struct sockaddr_in client_address;
socklen_t client_addrlength = sizeof(client_address);

int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);

```

执行时：

1. 你提供了一个空的 `client_address` 结构体；
2. 内核会自动把客户端的 IP 和端口信息**写入到这个结构体中**；
3. `accept()` 返回一个新的连接文件描述符 `connfd`；
4. 此时 `client_address` 里就已经有了这个客户端的地址数据。





## 两种模式下的定时器位置不同

<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251015152630257.png" alt="image-20251015152630257" style="zoom:50%;" />



## 信号超时：

        (1) alarm(TIMESLOT)
              ↓
         系统触发 SIGALRM
              ↓
        sig_handler() 写入 pipefd[1]
              ↓
     epoll_wait() 检测到 pipefd[0] 可读
              ↓
      dealwithsignal(timeout=true)
              ↓
    eventLoop 检测 timeout==true
              ↓
       timer_handler() 清理超时连接







## `listenfd`（监听套接字）

LT 模式下：

- 如果有多个客户端同时发起连接请求，
- 即使你 accept() 了一次，
- epoll_wait() 还会继续通知你，直到连接都 accept 完。

ET 模式下：

- **只通知一次！**
- 如果你没一次性把所有等待连接都 accept 完，就会“丢失”后面的连接请求。

​		



## `connfd`（通信套接字）

LT 模式下：

- 每次只要 socket 缓冲区里有数据没读完，epoll 就会继续通知。

ET 模式下：

- 只在“从无到有”那一瞬间通知你。
- 如果你没一次性把数据读干净，下次不会再提醒你了 😨。



## getopt()

`getopt()` 是一个命令行选项解析器，它帮你从命令行中**自动提取参数和选项**，
 例如：

```
-p 8080   → 选项 'p' 的参数是 "8080"
-l 1      → 选项 'l' 的参数是 "1"

```





## atoi()

`atoi()` 的作用是：将 C 风格字符串转换为整数。