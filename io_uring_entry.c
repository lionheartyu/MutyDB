#include <stdio.h>
#include <liburing.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include"kvstore.h"


#define EVENT_ACCEPT   	0
#define EVENT_READ		1
#define EVENT_WRITE		2

struct conn_item item={0};

struct conn_info{
	int fd;
	int event;
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

#define ENTRIES_LENGTH      1024
#define BUFFER_LENGTH		1024

int set_event_recv(struct io_uring *ring,int sockfd,void*buffer,size_t len,int flags){
	struct io_uring_sqe *sqe =  io_uring_get_sqe(ring);//从ring中获取一个提交队列
	
	struct conn_info accept_info={
		.fd = sockfd,
		.event=EVENT_READ,
	};

	// io_uring_prep_recv(sqe,sockfd,buffer,len,flags);
	io_uring_prep_recv(sqe,sockfd,item.rbuffer,len,flags);
	kvstore_requese(&item);
	memcpy(&sqe->user_data,&accept_info,sizeof(struct conn_info));
	return 0;
}

int set_event_send(struct io_uring *ring,int sockfd,void*buffer,size_t len,int flags){
	struct io_uring_sqe *sqe =  io_uring_get_sqe(ring);//从ring中获取一个提交队列
	
	struct conn_info accept_info={
		.fd = sockfd,
		.event=EVENT_WRITE,
	};

	// io_uring_prep_send(sqe,sockfd,buffer,len,flags);
	io_uring_prep_send(sqe,sockfd,item.wbuffer,len,flags);
	memcpy(&sqe->user_data,&accept_info,sizeof(struct conn_info));
	return 0;
}

int set_event_accept(struct io_uring*ring,int sockfd,struct sockaddr *addr,socklen_t* len,int flag){
	struct io_uring_sqe *sqe =  io_uring_get_sqe(ring);//从ring中获取一个提交队列
	
	struct conn_info accept_info={
		.fd = sockfd,
		.event=EVENT_ACCEPT,
	};

	io_uring_prep_accept(sqe,sockfd,(struct sockaddr*)addr,len,flag);
	memcpy(&sqe->user_data,&accept_info,sizeof(struct conn_info));
	return 0;
}



int io_uring_entry(){
     
	unsigned short port = 8080;
	int sockfd = init_server(port);

	struct io_uring_params params;
	memset(&params, 0, sizeof(params));

	struct io_uring ring;
	io_uring_queue_init_params(ENTRIES_LENGTH,&ring,&params);//初始化
    //ring这个结构体包含了提交队列和完成队列

    struct sockaddr_in clinaddr;
    socklen_t clinlen = sizeof(clinaddr);
    set_event_accept(&ring,sockfd,(struct sockaddr*)&clinaddr,&clinlen,0);

	char buffer[BUFFER_LENGTH]={0};

	while(1){
		io_uring_submit(&ring);
		// io_uring 提交队列中的 I/O 操作请求提交给内核进行处理


		struct io_uring_cqe *cqe;
		io_uring_wait_cqe(&ring,&cqe);//
		//该函数会阻塞当前进程，直到 io_uring 完成队列中有新的 I/O 操作完成事件。
		//存储到 cqe 指针中。

		struct io_uring_cqe *cqes[128];
		int nready = io_uring_peek_batch_cqe(&ring,cqes,sizeof(cqes));

		int i;
		for(i=0;i<nready;i++){
			struct io_uring_cqe *entries=cqes[i];
			struct conn_info reslut;
			memcpy(&reslut,&entries->user_data,sizeof(struct conn_info));

			if(reslut.event==EVENT_ACCEPT){
				set_event_accept(&ring,sockfd,(struct sockaddr*)&clinaddr,&clinlen,0);
				// printf("set_event_accept\n"); //
				//当第二个客户端连接时，服务器在处理第一个客户端连接完成事件时
				//重新调用 set_event_accept 把新的 accept 操作添加到 SQ 队列，再通过 io_uring_submit 提交给内核以处理新连接

				int condfd= entries->res;
				set_event_recv(&ring, condfd, buffer, BUFFER_LENGTH, 0);
			}else if(reslut.event==EVENT_READ){
				int ret =entries->res;
				// printf("set_event_recv ret: %d, %s\n", ret, buffer); 
				if(ret==0){
					close(reslut.fd);
				}else{
					set_event_send(&ring, reslut.fd, buffer, BUFFER_LENGTH, 0);
				}
			}else if(reslut.event==EVENT_WRITE){

				int ret =entries->res;
				// printf("set_event_send ret: %d, %s\n", ret, buffer);
				set_event_recv(&ring, reslut.fd, buffer, BUFFER_LENGTH, 0);
			}

		}
		io_uring_cq_advance(&ring,nready);
		
	}
    
}