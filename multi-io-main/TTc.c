#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/time.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_LENGTH 128

typedef int (*RCALLBACK)(int fd);
// listenfd 触发 EPOLLIN 事件的时候，执行 accept_cb
int accept_cb(int fd);
// clientfd 触发 EPOLLIN 事件的时候，执行 recv_cb
int recv_cb(int fd);
// clientfd 触发 EPOLLOUT 事件的时候，执行 send_cb
int send_cb(int fd);

struct conn_item
{
    int fd;
    char rbuffer[BUFFER_LENGTH];
    int rlen;
    char wbuffer[BUFFER_LENGTH];
    int wlen;

    union
    {
        RCALLBACK accept_callback;
        RCALLBACK recv_callback;
    } recv_t;

    RCALLBACK send_callback;
};

int epfd = 0;
struct conn_item conn_list[1048576] = {0};
struct timeval start_time;
#define TIME_SUB_MS(tv1, tv2) ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)

int set_event(int fd, int event, int flag)
{
    if (flag)
    { // 1: add
        struct epoll_event ev;
        ev.events = event;
        ev.data.fd = fd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    }
    else
    { // 0: mod
        // 设置事件
        struct epoll_event ev;
        ev.events = event;
        ev.data.fd = fd;
        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
    }
}

int accept_cb(int fd)
{
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    int clientfd = accept(fd, (struct sockaddr *)&client_addr, &len);
    if (clientfd < 0)
    {
        return -1;
    }

    set_event(clientfd, EPOLLIN, 1);

    conn_list[clientfd].fd = clientfd;
    memset(conn_list[clientfd].rbuffer, 0, BUFFER_LENGTH);
    conn_list[clientfd].rlen = 0;
    memset(conn_list[clientfd].wbuffer, 0, BUFFER_LENGTH);
    conn_list[clientfd].wlen = 0;
    conn_list[clientfd].recv_t.recv_callback = recv_cb;
    conn_list[clientfd].send_callback = send_cb;

    if ((clientfd % 1000) == 999) {
        struct timeval cur_time;
        gettimeofday(&cur_time, NULL);
        int time_used = TIME_SUB_MS(cur_time, start_time);
        memcpy(&start_time, &cur_time, sizeof(struct timeval));

        printf("clientfd: %d, time_used: %d\n", clientfd, time_used);
    }
    return clientfd;
}

int recv_cb(int fd)
{
    char *buffer = conn_list[fd].rbuffer;
    int idx = conn_list[fd].rlen;

    int count = recv(fd, buffer + idx, BUFFER_LENGTH - idx, 0);
    if (count == 0)
    {
        printf("disconnect \n");

        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);

        close(fd);
        return -1;
    }
    conn_list[fd].rlen += count;

    memcpy(conn_list[fd].wbuffer, conn_list[fd].rbuffer, conn_list[fd].rlen);
    conn_list[fd].wlen = conn_list[fd].rlen;
    conn_list[fd].rlen -= conn_list[fd].rlen;

    set_event(fd, EPOLLOUT, 0);

    return count;
}

int send_cb(int fd)
{
    char *buffer = conn_list[fd].wbuffer;
    int idx = conn_list[fd].wlen;

    int count = send(fd, buffer, idx, 0);

    set_event(fd, EPOLLIN, 0);

    return count;
}

int init_server(unsigned short port)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (-1 == bind(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)))
    {
        perror("bind");
        return -1;
    }

    listen(sockfd, 10);

    return sockfd;
}

int main()
{
    int port_count = 10;
    unsigned short port = 2048;
    int i = 0;
    epfd = epoll_create(1);
    for (i = 0; i < port_count; i++)
    {
        int sockfd = init_server(port + i); // 2048、2049、2050...2057
        conn_list[sockfd].fd = sockfd;
        conn_list[sockfd].recv_t.accept_callback = accept_cb;
        set_event(sockfd, EPOLLIN, 1);
    }

    gettimeofday(&start_time, NULL);

    struct epoll_event events[1024] = {0};

    while (1)
    {
        int nready = epoll_wait(epfd, events, 1024, -1);
        int i = 0;
        for (i = 0; i < nready; i++)
        {
            int connfd = events[i].data.fd;
            if (events[i].events & EPOLLIN)
            {
                int count = conn_list[connfd].recv_t.recv_callback(connfd);
            }
            else if (events[i].events & EPOLLOUT)
            {
                int count = conn_list[connfd].send_callback(connfd);
            }
        }
    }

    getchar();

    return 0;
}
