# 线程池

## Reactor与Proactor两种模型的区别

              Reactor 模型                          Proactor 模型
    ───────────────────────────────       ───────────────────────────────
    epoll_wait 检测到可读事件              epoll_wait 检测到I/O完成事件
           ↓                                       ↓
    主线程通知线程池执行                     主线程直接 read_once()
           ↓                                       ↓
    线程执行 recv()/send()                     主线程完成 I/O
           ↓                                       ↓
    线程处理请求业务                         线程池仅处理业务逻辑
    




## worker静态成员函数

注意其中的函数返回值类型以及形参

<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251009193154079.png" alt="image-20251009193154079" style="zoom:50%;" />

<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251009193203460.png" alt="image-20251009193203460" style="zoom:50%;" />

<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251009193228645.png" alt="image-20251009193228645" style="zoom:50%;" />

### pthread_create函数

```
int pthread_create(
    pthread_t *thread,               // 输出参数：线程ID
    const pthread_attr_t *attr,      // 线程属性，一般传 NULL 使用默认
    void *(*start_routine)(void *),  // 线程入口函数
    void *arg                        // 传给线程入口函数的参数
);

```

可以看出**定义的线程入口函数只能传入一个参数**，就是第四位的void * arg，是一个任意类型的指针。但是类中的非静态成员函数都包含一个隐藏的this指针参数。只有 **静态成员函数** 才不带隐含的 `this` 指针参数，能当作普通函数传给 pthread_create。

例如这里：

```
void* worker(threadpool* this, void* arg); // 隐含一个 this 参数
```