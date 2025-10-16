# POSIX:可移植操作系统接口

## 头文件

项目中使用：

```
#ifndef  文件名大写_H
#define  文件名大写_H
...
...
#endif

```

​	**这样可以防止同文件被重复包含**。因为可能会与多个文件同时包含这个文件，在展开同文件时，这个文件可能会被展开多次，导致重定义错误。所以使用上述方法可以避免。

​	使用#pragma once  也可以。

使用#ifndef  #define #endif  这个方式的移植性更高

## 1.信号量sem

定义 ：sem_t

初始化： sem_inint()

销毁：sem_destroy()

## 2.锁 pthread_mutex

定义：pthread_mutex_t

初始化：pthread_mutex_init()

pthread_mutex_lock()

pthread_mutex_unlock()

pthread_mutex_destroy()

## 3.条件变量pthread_cond

pthread_cond_t  变量名

pthread_cond_init()   初始化变量（必须要进行，不然后续使用变量会出错）

pthread_cond_wait()    ： 将传入的互斥锁解开，并让线程进入阻塞状态。

pthread_cond_wait_timedwait()

pthread_cond_signal() :表示唤醒一个阻塞的线程

pthread_cond_broadcast() ：表示唤醒所有阻塞的线程

pthread_cond_destroy()



my_lock这个文件中使用上述底层操作封装了锁以及条件变量，但是这些在C++11中已经被封装了，可以直接使用。





