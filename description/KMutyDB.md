# 1.网络库部分

## 1.1Reactor_epoll/IO多路复用(epoll_entry.c)

**epoll 的核心特点**

- **高效**：只关心“活跃”的 fd，避免了 select/poll 每次都遍历全部 fd 的低效。
- **支持大并发**：可以监听成千上万个 fd，适合高并发场景。
- **事件驱动**：只在有事件发生时通知应用，减少无效唤醒和 CPU 占用。

* **前提概要**

```bash
你启动服务器，监听 2048 端口，得到监听 fd（如 sockfd=3）。
有客户端连接，epoll_wait 检测到 sockfd=3 可读（EPOLLIN），调用 accept_cb。
accept_cb 里 accept() 得到 clientfd=5，代表这个新客户端。
以后 clientfd=5 有数据到来，epoll_wait 检测到 clientfd=5 可读（EPOLLIN），调用 recv_cb。
```

* **架构流程图：**

```bash
epoll_entry(epoll_create())
    |
    v
init_Server (#多端口监听)
    |
    v
set_event(sockfd, EPOLLIN, 1)(epoll_ctl)
    |
    v
------------------- #事件循环 -------------------
    |
    v
epoll_wait
    |
    v
+-----------------------------+
| #事件分发 (for each fd)      |
+-----------------------------+
    |
    +--[EPOLLIN]--> connlist[fd].recv_t.recv_callback(fd)
    |                   |
    |                   +-- accept_cb(fd)  (#监听fd)
    |                   |      |
    |                   |      +-- accept() #新连接
    |                   |      +-- set_event(clientfd, EPOLLIN, 1)
    |                   |
    |                   +-- recv_cb(fd)    (#客户端fd)
    |                          |
    |                          +-- recv() #读数据
    |                          +-- kvstore_requese(&connlist[fd])
    |                          +-- set_event(fd, EPOLLOUT, 0)
    |
    +--[EPOLLOUT]--> connlist[fd].send_callback(fd)
                        |
                        +-- send_cb(fd)
                        |     |
                        |     +-- send() #写数据
                        |     +-- set_event(fd, EPOLLIN, 0)
```

- `accept_cb`：处理新连接
- `recv_cb`：读数据、业务处理、准备写
- `send_cb`：写数据、准备读
- `epoll_entry`：主循环和事件分发

* **定义全局变量**

```c++
// 连接对象数组
struct conn_item connlist[1048576] = {0}; // 支持大量连接
struct timeval zvoice_king; // 用于统计时间
int epfd = 0; // 全局 epoll fd
```

* **设置epoll事件**

```c++
int set_event(int fd, int event, int flag) {
    struct epoll_event ev;
    ev.events = event;
    ev.data.fd = fd;
    if (flag) { // 1: add, 0: mod
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    } else {
        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
    }
}
```

* **监听socket上有新连接时的回调**

```c++
int accept_cb(int fd) {
    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);
    int clientfd = accept(fd, (struct sockaddr*)&clientaddr, &len);
    if (clientfd < 0) {
        return -1;
    }
    set_event(clientfd, EPOLLIN, 1); // 新连接注册 EPOLLIN

    // 初始化连接对象
    connlist[clientfd].fd = clientfd;
    memset(connlist[clientfd].rbuffer, 0, BUFFER_LENGTH);
    connlist[clientfd].rlen = 0;
    memset(connlist[clientfd].wbuffer, 0, BUFFER_LENGTH);
    connlist[clientfd].wlen = 0;
    connlist[clientfd].recv_t.recv_callback = recv_cb;
    connlist[clientfd].send_callback = send_cb;
    
    return clientfd;
}
```

* **客户端 fd 可读时的回调**

```c++
int recv_cb(int fd) {
    char *buffer = connlist[fd].rbuffer;
    int idx = connlist[fd].rlen;
    int count = recv(fd, buffer, BUFFER_LENGTH, 0);
    if (count == 0) {
        printf("disconnect\n");
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        close(fd);
        return -1;
    }
    connlist[fd].rlen = count;

    // 业务处理：调用 kvstore_requese 处理请求
    kvstore_requese(&connlist[fd]);
    connlist[fd].wlen = strlen(connlist[fd].wbuffer);

    set_event(fd, EPOLLOUT, 0); // 注册 EPOLLOUT，准备写响应
    return count;
}
```

* **客户端 fd 可写时的回调**

```c++
int send_cb(int fd) {
    char *buffer = connlist[fd].wbuffer;
    int idx = connlist[fd].wlen;
    int count = send(fd, buffer, idx, 0);
    set_event(fd, EPOLLIN, 0); // 写完后重新注册 EPOLLIN，继续读
    return count;
}
```

* **初始化服务器监听 socket**

```c++
int init_Server(unsigned short port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(struct sockaddr_in));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);

    if (-1 == bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr))) {
        perror("bind");
        return -1;
    }
    listen(sockfd, 10);
    return sockfd;
}
```

* **epoll 主循环入口**

```c++
int epoll_entry(void) {
    int port_count = 20;
    unsigned short port = 2048;
    int i = 0;

    epfd = epoll_create(1);

    // 启动多个监听端口
    for (i = 0; i < port_count; i++) {
        int sockfd = init_Server(port + i);
        connlist[sockfd].fd = sockfd;
        connlist[sockfd].recv_t.accept_callback = accept_cb;
        set_event(sockfd, EPOLLIN, 1);
    }

    gettimeofday(&zvoice_king, NULL);

    struct epoll_event events[1024] = {0};

    // 事件主循环
    while (1) {
        int nready = epoll_wait(epfd, events, 1024, -1);
        for (i = 0; i < nready; i++) {
            int connfd = events[i].data.fd;
            if (events[i].events & EPOLLIN) {
                // 可读事件：监听fd调用accept_cb，客户端fd调用recv_cb
                connlist[connfd].recv_t.recv_callback(connfd);
            } else if (events[i].events & EPOLLOUT) {
                // 可写事件：调用send_cb
                connlist[connfd].send_callback(connfd);
            }
        }
    }
    getchar();
}
```

## 1.2io_uring(io_uring_entry.c)

- io_uring 是 Linux 5.1+ 提供的高性能异步 I/O 框架。
- 它通过“提交队列（SQ）”和“完成队列（CQ）”实现用户态和内核态的高效通信，极大减少了系统调用次数和内核切换。
- 适合高并发网络服务器、数据库等场景。

```c++
io_uring_entry
    |
    v
init_server         // 初始化监听socket
    |
    v
set_event_accept    // 注册第一个accept事件
    |
    v
------------------- 主循环 -------------------
    |
    v
io_uring_submit
    |
    v
io_uring_wait_cqe
    |
    v
io_uring_peek_batch_cqe
    |
    v
for (每个完成事件)
    |
    +--> if (EVENT_ACCEPT)
    |        |
    |        +--> set_event_accept      // 注册下一个accept
    |        |
    |        +--> malloc conn_item
    |        +--> set_event_recv        // 注册读事件
    |
    +--> else if (EVENT_READ)
    |        |
    |        +--> if (ret <= 0)
    |        |        +--> close/free
    |        |
    |        +--> else
    |                 +--> kvstore_requese
    |                 +--> set_event_send // 注册写事件
    |
    +--> else if (EVENT_WRITE)
             |
             +--> set_event_recv         // 注册下一个读事件

------------------- 关键函数区域 -------------------
- set_event_accept: 注册accept事件
- set_event_recv:   注册读事件
- set_event_send:   注册写事件
- kvstore_requese:  业务处理
---------------------------------------------------
```

```bash
启动 -> 初始化监听socket和io_uring
    -> 注册accept事件
    -> 主循环:
        -> 提交事件
        -> 等待事件完成
        -> 对每个完成事件:
            -> accept: 新连接，注册下一个accept，分配conn_item，注册read
            -> read:   有数据，业务处理，注册write；无数据则关闭
            -> write:  写完，注册下一个read
```



## 1.3NtyCo协程库(nty_entry.c)

```bash
ntyco_enrty
    |
    v
for (端口)
    |
    v
nty_coroutine_create(&co, server, port)
    |
    v
------------------ 协程调度 ------------------
    |
    v
server
    |
    v
while (1)
    |
    v
accept(fd, ...)
    |
    v
nty_coroutine_create(&read_co, server_reader, cli_fd)
    |
    v
------------------ 协程调度 ------------------
    |
    v
server_reader
    |
    v
while (1)
    |
    v
recv(fd, ...)
    |
    v
kvstore_requese(&item)
    |
    v
send(fd, ...)
```

- `ntyco_enrty`：入口，创建 server 协程
- `server`：监听端口，accept 新连接，为每个连接创建 server_reader 协程
- `server_reader`：循环收发数据，调用业务处理（kvstore_requese）

## 1.4Muduo网络库(见专栏)(kmuduo_entry.cc)

# 2.数据存储部分

## 2.1数组(kvstore_array.c)

```c++
#if ENABLE_ARRAY_KVENGINE
struct kvs_array_item_s
{
	char *key;
	char *value;
};

typedef struct array_s array_t;
extern array_t Array;
int kvstore_array_create(array_t *arr);
void kvstore_array_destory(array_t *arr);
int kvs_array_set(array_t *arr, char *key, char *value); // 设置
char *kvs_array_get(array_t *arr, char *key);			 // 获取
int kvs_array_del(array_t *arr, char *key);				 // 删除
int kvs_array_mod(array_t *arr, char *key, char *value); // 修改
int kvs_array_count(array_t *arr);						 // 获取数量
#define KVS_ARRAY_SIZE 1024

#endif
```

**访问速度快**

- 数组元素在内存中是连续存放的，可以通过下标 O(1) 时间直接访问，非常高效。

**实现简单**

- 代码结构简单，易于实现和维护，适合小规模或定长数据场景。

**遍历高效**

- 顺序遍历数组时，CPU 缓存命中率高，性能好。

**内存空间紧凑**

- 没有额外的指针或结构体开销，节省内存。

**适合静态或定长数据**

- 如果数据量已知且不会频繁增删，数组是最优选择。

## 2.2红黑树(kvstore_rbtree.c)

```c++
#if ENABLE_RBTREE_KVENGINE

typedef struct _rbtree _rbtree_t;
extern _rbtree_t Tree;
int kvstore_rbtree_create(_rbtree_t *tree); //创建一颗红黑树
void kvstore_rbtree_destory(_rbtree_t *tree);//销毁一颗红黑树
int kvs_rbtree_set(_rbtree_t *tree, char *key, char *value); //插入一组key value到新节点上
char *kvs_rbtree_get(_rbtree_t *tree, char *key);//获取key对应的value
int kvs_rbtree_delete(_rbtree_t *tree, char *key);//删除key对应的value
int kvs_rbtree_modify(_rbtree_t *tree, char *key, char *value);//.....
int kvs_rbtree_count(_rbtree_t *tree);//.......

#endif
```

红黑树的存储优势主要有：

**查找、插入、删除效率高且稳定**

- 红黑树是一种自平衡二叉搜索树，能保证**最坏情况下**查找、插入、删除操作的时间复杂度都是 O(log n)。

**自动保持有序**

- 所有节点始终按 key 有序排列，支持范围查询、最小/最大值查找等操作，非常方便。

**适合动态数据集**

- 数据频繁插入和删除时，红黑树能自动调整结构，始终保持高效访问。

**空间利用率高**

- 不需要像哈希表那样预分配大量空间，也不会像数组那样有大量空位。

**无哈希冲突问题**

- 不依赖哈希函数，避免了哈希冲突和哈希退化问题。

**支持有序遍历**

- 可以高效地按 key 顺序遍历所有元素，适合需要排序输出或区间操作的场景。

**适用场景**：

- 需要频繁插入、删除、查找且数据有序性要求的场合，如数据库索引、内存有序表等

## 2.3哈希表(kvstore_hash.c)

```c++
#if ENABLE_HASH_KVENGINE
typedef struct hashtable_s hashtable_t;
extern hashtable_t Hash;

#endif
int kvstore_hash_create(hashtable_t *hash);
void kvstore_hash_destory(hashtable_t *hash);
int kvs_hash_set(hashtable_t *hash, char *key, char *value);
char *kvs_hash_get(hashtable_t *hash, char *key);
int kvs_hash_delete(hashtable_t *hash, char *key);
int kvs_hash_modify(hashtable_t *hash, char *key, char *value);
int kvs_hash_count(hashtable_t *hash);
```

**查找、插入、删除速度极快**

- 理论上平均时间复杂度为 O(1)，适合高频访问和大数据量场景。

**键值对映射直观**

- 直接通过 key 定位 value，无需遍历或排序，使用简单。

**扩展性强**

- 支持动态扩容，能高效应对数据量增长。

**适合无序数据管理**

- 不要求 key 有序，适合快速检索和更新。

**冲突处理灵活**

- 通过链表、开放寻址等方式解决哈希冲突，保证数据完整性。

**适用场景**：需要高效查找、插入、删除的场合，如缓存、索引、唯一性校验等

# 3.核心入口函数(kvstore.h/kvstore.c)

## 3.1对于命令的读取流程

```bash
客户端发送命令
    |
    v
网络层收到数据（如 epoll/ntyco/io_uring）
    |
    v
conn_item.rbuffer ← 读入命令字符串
    |
    v
kvstore_requese(conn_item)
    |
    v
// 去除结尾换行符
|
v
kvstore_split_token(msg, tokens)
    |
    v
// 按空格分割命令和参数
|
v
kvstore_parser_protocol(conn_item, tokens, count)
    |
    v
// 识别命令类型（如 SET/GET/DEL/...）
|
v
switch(cmd) {
    case KV_CMD_SET:      // ...等
        kvstore_array_set(key, value);
        wal_append("SET", key, value);
        // ...写入响应到 conn_item.wbuffer
        break;
    // 其它命令类似
}
    |
    v
响应内容写入 conn_item.wbuffer
    |
    v
网络层将 wbuffer 发送回客户端
```

- 网络层负责接收命令字符串，写入 `conn_item.rbuffer`。
- `kvstore_requese` 负责解析和处理命令，写响应到 `conn_item.wbuffer`。
- 响应通过网络层发回客户端。

## 3.2持久化的流程

```bash
写操作（如 SET/DEL/MOD/HSET...）
    |
    v
调用 wal_append(cmd, key, value)
    |
    v
将操作以文本形式追加写入 wal.log
    |
    v
[如果 wal.log 超过 WAL_MAX_SIZE]
    |
    v
rotate_wal()
    |
    v
归档轮转（wal.log -> wal.log.1, wal.log.1 -> wal.log.2, ...）

--------------------------------------------

程序启动
    |
    v
调用 wal_replay()
    |
    v
依次读取 wal.log.N ... wal.log.1, wal.log
    |
    v
逐条解析日志内容
    |
    v
在内存数据库中执行对应操作（SET/DEL/...）
    |
    v
恢复出最新的数据状态
```

- 所有写操作都会先写入 wal.log，保证数据不丢失。
- 日志文件超限时自动归档，防止单文件过大。
- 启动时重放所有日志，实现持久化恢复。