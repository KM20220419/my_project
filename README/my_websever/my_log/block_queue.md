# Block_queue

## 头文件

​	<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251002113059587.png" alt="image-20251002113059587" style="zoom:50%;" />

<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251002113120560.png" alt="image-20251002113120560" style="zoom:50%;" />

<img src="C:\Users\王者荣耀\AppData\Roaming\Typora\typora-user-images\image-20251002113419737.png" alt="image-20251002113419737" style="zoom:50%;" />

## 1

用于异步写入日志时使用

这里是创建一个任务队列，里面存放要写入日志的数据。

## 2

​	其中使用的不是STL中的queue作为底层容器，而是通过用一个循环数组作为底层容器。

​	使用循环数组来实现push以及pop函数。

​	这里需要注意的是使用的是pthread_cond_wait()来判断是否阻塞线程，所以要用while循环来查询**（被唤醒后，还要判断条件是否满足）**。避免用if时，存在虚假唤醒（线程被唤醒，但是此时条件不满足，用if又不会继续判断）

## 3

​	在对队列进行操作时，要注意加锁。

