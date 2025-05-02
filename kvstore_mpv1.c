#include<stdlib.h>
#include<stdio.h>
#include"kvstore.h"
#define MEM_PAGE_SIZE 4096

typedef struct mempool_s{
	int block_size;
	int free_count;

	char*free_ptr;
	char*mem;
}mempool_t;

//内存池的初始化
int mp_init(mempool_t*m,int size){
	if(m==NULL) return -1;
	if(size<16) size=16;
	
	m->block_size=size;//固定大小的内存块
	m->mem=(char*)malloc(MEM_PAGE_SIZE);
	if(m->mem==NULL) return -2;

	m->free_ptr=m->mem;
	m->free_count=MEM_PAGE_SIZE/size;

	int i =0;
	char*ptr= m->free_ptr;
	for(i;i<m->free_count;i++){
		*(char**)ptr=ptr+size;//☆
		ptr+=size;
	}
	*(char**)ptr=NULL;
}

//内存池的销毁
void mp_dest(mempool_t*m){
	if(m==NULL||m->mem==NULL) return;
	free(m->mem);
}

//为内存块分配内存
void * mp_allock(mempool_t*m){
	if(m==NULL||m->free_count==0) return NULL;
	void*ptr =m->free_ptr;//ptr指向一块空闲的内存

	m->free_ptr=*(char**)ptr;//m->free_ptr指向下一块空闲的内存 ☆
	m->free_count--;
	
	return ptr;
}

//为内存块释放内存
void mp_free(mempool_t*m,void*ptr){
	*(char**)ptr=m->free_ptr;
	m->free_ptr=(char*)ptr;
	m->free_count++;
}

mempool_t M;


#if 0
int main(){
	mempool_t m;
    mp_init(&m,32);
	

	void *p1 =mp_allock(&m);
	printf("1:mp_alloc:%p\n",p1);

	void *p2 =mp_allock(&m);
	printf("2:mp_alloc:%p\n",p2);

	void *p3 =mp_allock(&m);
	printf("3:mp_alloc:%p\n",p3);

	void *p4 =mp_allock(&m);
	printf("4:mp_alloc:%p\n",p4);

	mp_free(&m,p2);

	void *p5 =mp_allock(&m);
	printf("5:mp_alloc:%p\n",p5);

}

#endif

//对内存块的管理
//1.避免频繁分配
//2.长期运行出现的内存碎片