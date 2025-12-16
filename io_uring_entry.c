#include <stdio.h>
#include <liburing.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "kvstore.h"

#define EVENT_ACCEPT   	0   // 事件类型：accept
#define EVENT_READ		1   // 事件类型：read
#define EVENT_WRITE		2   // 事件类型：write

#define ENTRIES_LENGTH      1024   // io_uring 队列长度

// 用于传递事件类型和连接对象指针
struct conn_info {
    int event;                  // 事件类型
    struct conn_item *item;     // 指向连接对象
};

// 初始化服务器监听 socket，返回监听 fd
int init_server(unsigned short port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // 创建 socket
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(struct sockaddr_in)); // 清零
    serveraddr.sin_family = AF_INET;                    // IPv4
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);     // 任意地址
    serveraddr.sin_port = htons(port);                  // 端口

    if (-1 == bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr))) { // 绑定端口
        perror("bind");
        return -1;
    }

    listen(sockfd, 10); // 监听
    return sockfd;      // 返回监听 fd
}

// 注册一个异步读事件到 io_uring
int set_event_recv(struct io_uring *ring, struct conn_item *item, int flags) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring); // 获取提交队列项
    struct conn_info *info = malloc(sizeof(struct conn_info)); // 分配事件信息
    info->event = EVENT_READ;      // 设置事件类型为 read
    info->item = item;             // 关联连接对象
    io_uring_prep_recv(sqe, item->fd, item->rbuffer, BUFFER_LENGTH, flags); // 填充读操作
    sqe->user_data = (unsigned long)info; // 事件完成时可取回 info
    return 0;
}

// 注册一个异步写事件到 io_uring
int set_event_send(struct io_uring *ring, struct conn_item *item, size_t len, int flags) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring); // 获取提交队列项
    struct conn_info *info = malloc(sizeof(struct conn_info)); // 分配事件信息
    info->event = EVENT_WRITE;     // 设置事件类型为 write
    info->item = item;             // 关联连接对象
    io_uring_prep_send(sqe, item->fd, item->wbuffer, len, flags); // 填充写操作
    sqe->user_data = (unsigned long)info; // 事件完成时可取回 info
    return 0;
}

// 注册一个异步 accept 事件到 io_uring
int set_event_accept(struct io_uring *ring, int sockfd, struct sockaddr *addr, socklen_t *len, int flag) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring); // 获取提交队列项
    struct conn_info *info = malloc(sizeof(struct conn_info)); // 分配事件信息
    info->event = EVENT_ACCEPT;   // 设置事件类型为 accept
    info->item = NULL;            // accept 没有连接对象
    io_uring_prep_accept(sqe, sockfd, addr, len, flag); // 填充 accept 操作
    sqe->user_data = (unsigned long)info; // 事件完成时可取回 info
    return 0;
}

// io_uring 异步事件主循环
int io_uring_entry() {
    unsigned short port = 1024;   // 监听端口
    int sockfd = init_server(port); // 初始化监听 socket

    struct io_uring_params params;
    memset(&params, 0, sizeof(params)); // 清零参数
    struct io_uring ring;
    io_uring_queue_init_params(ENTRIES_LENGTH, &ring, &params); // 初始化 io_uring

    struct sockaddr_in clinaddr;      // 客户端地址
    socklen_t clinlen = sizeof(clinaddr); // 地址长度
    set_event_accept(&ring, sockfd, (struct sockaddr*)&clinaddr, &clinlen, 0); // 注册第一个 accept 事件

    while (1) { // 主循环
        io_uring_submit(&ring); // 提交所有准备好的事件到内核

        struct io_uring_cqe *cqe;
        io_uring_wait_cqe(&ring, &cqe); // 等待至少一个事件完成

        struct io_uring_cqe *cqes[128]; // 批量处理完成事件
        int nready = io_uring_peek_batch_cqe(&ring, cqes, sizeof(cqes)/sizeof(cqes[0]));

        for (int i = 0; i < nready; i++) { // 遍历所有完成事件
            struct io_uring_cqe *entries = cqes[i]; // 当前完成事件
            struct conn_info *info = (struct conn_info *)entries->user_data; // 取回事件信息

            if (info->event == EVENT_ACCEPT) { // 新连接到来
                set_event_accept(&ring, sockfd, (struct sockaddr*)&clinaddr, &clinlen, 0); // 继续注册下一个 accept

                int connfd = entries->res; // 新客户端 fd
                if (connfd >= 0) {
                    struct conn_item *item = malloc(sizeof(struct conn_item)); // 分配连接对象
                    memset(item, 0, sizeof(struct conn_item)); // 清零
                    item->fd = connfd; // 记录 fd
                    set_event_recv(&ring, item, 0); // 注册读事件
                }
                free(info); // 释放事件信息
            } else if (info->event == EVENT_READ) { // 有客户端数据到来
                struct conn_item *item = info->item; // 取回连接对象
                int ret = entries->res; // 读到的数据长度
                if (ret <= 0) { // 客户端关闭或出错
                    close(item->fd); // 关闭 fd
                    free(item);      // 释放连接对象
                } else {
                    item->rbuffer[ret] = '\0'; // 补零，方便字符串处理
                    kvstore_requese(item);     // 业务处理，写入 wbuffer
                    set_event_send(&ring, item, strlen(item->wbuffer), 0); // 注册写事件
                }
                free(info); // 释放事件信息
            } else if (info->event == EVENT_WRITE) { // 写响应完成
                struct conn_item *item = info->item; // 取回连接对象
                set_event_recv(&ring, item, 0); // 继续注册读事件
                free(info); // 释放事件信息
            }
        }
        io_uring_cq_advance(&ring, nready); // 标记这些事件已处理
    }
}