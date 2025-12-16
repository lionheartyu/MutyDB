#include <stdio.h>
#include <liburing.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "kvstore.h"

#define EVENT_ACCEPT   	0
#define EVENT_READ		1
#define EVENT_WRITE		2

#define ENTRIES_LENGTH      1024



struct conn_info {
    int event;
    struct conn_item *item;
};

int init_server(unsigned short port) {
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

int set_event_recv(struct io_uring *ring, struct conn_item *item, int flags) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    struct conn_info *info = malloc(sizeof(struct conn_info));
    info->event = EVENT_READ;
    info->item = item;
    io_uring_prep_recv(sqe, item->fd, item->rbuffer, BUFFER_LENGTH, flags);
    sqe->user_data = (unsigned long)info;
    return 0;
}

int set_event_send(struct io_uring *ring, struct conn_item *item, size_t len, int flags) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    struct conn_info *info = malloc(sizeof(struct conn_info));
    info->event = EVENT_WRITE;
    info->item = item;
    io_uring_prep_send(sqe, item->fd, item->wbuffer, len, flags);
    sqe->user_data = (unsigned long)info;
    return 0;
}

int set_event_accept(struct io_uring *ring, int sockfd, struct sockaddr *addr, socklen_t *len, int flag) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    struct conn_info *info = malloc(sizeof(struct conn_info));
    info->event = EVENT_ACCEPT;
    info->item = NULL;
    io_uring_prep_accept(sqe, sockfd, addr, len, flag);
    sqe->user_data = (unsigned long)info;
    return 0;
}

int io_uring_entry() {
    unsigned short port = 1024;
    int sockfd = init_server(port);

    struct io_uring_params params;
    memset(&params, 0, sizeof(params));
    struct io_uring ring;
    io_uring_queue_init_params(ENTRIES_LENGTH, &ring, &params);

    struct sockaddr_in clinaddr;
    socklen_t clinlen = sizeof(clinaddr);
    set_event_accept(&ring, sockfd, (struct sockaddr*)&clinaddr, &clinlen, 0);

    while (1) {
        io_uring_submit(&ring);

        struct io_uring_cqe *cqe;
        io_uring_wait_cqe(&ring, &cqe);

        struct io_uring_cqe *cqes[128];
        int nready = io_uring_peek_batch_cqe(&ring, cqes, sizeof(cqes)/sizeof(cqes[0]));

        for (int i = 0; i < nready; i++) {
            struct io_uring_cqe *entries = cqes[i];
            struct conn_info *info = (struct conn_info *)entries->user_data;

            if (info->event == EVENT_ACCEPT) {
                set_event_accept(&ring, sockfd, (struct sockaddr*)&clinaddr, &clinlen, 0);

                int connfd = entries->res;
                if (connfd >= 0) {
                    struct conn_item *item = malloc(sizeof(struct conn_item));
                    memset(item, 0, sizeof(struct conn_item));
                    item->fd = connfd;
                    set_event_recv(&ring, item, 0);
                }
                free(info);
            } else if (info->event == EVENT_READ) {
                struct conn_item *item = info->item;
                int ret = entries->res;
                if (ret <= 0) {
                    close(item->fd);
                    free(item);
                } else {
                    item->rbuffer[ret] = '\0';
                    kvstore_requese(item);
                    set_event_send(&ring, item, strlen(item->wbuffer), 0);
                }
                free(info);
            } else if (info->event == EVENT_WRITE) {
                struct conn_item *item = info->item;
                set_event_recv(&ring, item, 0);
                free(info);
            }
        }
        io_uring_cq_advance(&ring, nready);
    }
}