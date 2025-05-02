#include<stdio.h>
#include<stdlib.h>

#define MEM_PAGE_SIZE 4096

typedef struct mp_node_s{
    unsigned char*free_ptr;
    unsigned char*end;

    struct mp_node_s *next;
}mp_node_t;
 
typedef struct mp_pool_s{
    struct mp_node_s *first;
    struct mp_node_s *current;
    int max;
}mp_pool_t;

//内存池的初始化
int mp_init(mp_pool_t*m,int size){
    if(m==NULL) return -1;

    void *addr = malloc(size);
    mp_node_t *node =(mp_node_t*)addr;

    node->free_ptr=(char*)addr + sizeof(mp_node_t);
    node->end=(char*)addr + size;
    node->next=NULL;

    m->first=node;
    m->current=node;
    m->max=size;

    return 0;
}

//内存池的销毁
void mp_des(mp_pool_t*m,int size){
    if(m==NULL) return;
    while(m->first!=NULL){
        void*addr = m->first;
        mp_node_t*node=(mp_node_t*)addr;
        m->first=node->next;

        free((addr));
    }
    return;
}

void* mp_allock(mp_pool_t*m,int size){
    void*addr= m->current;
    mp_node_t*node = (mp_node_t*)addr;

    do{
        if(size<=node->end-node->free_ptr){
            char*ptr =node->free_ptr;
            node->free_ptr+=size;
            return ptr;
        }
        node=node->next;

    }while(node);

    addr =malloc(m->max);
    node = (mp_node_t*)addr;

    node->free_ptr = (char*)addr+sizeof(mp_node_t);
    node->end=(char*)addr+m->max;

    node->next=m->current;
    m->current=node;

    char*ptr=node->free_ptr;
    node->free_ptr+=size;
    
    return ptr;
}

// void mp_free(mp_pool_t*m,int size){

// }

int main(){
    mp_pool_t m;

    mp_init(&m,MEM_PAGE_SIZE);

    void *p1 =mp_allock(&m,16);
	printf("1:mp_alloc:%p\n",p1);

	void *p2 =mp_allock(&m,32);
	printf("2:mp_alloc:%p\n",p2);

	void *p3 =mp_allock(&m,64);
	printf("3:mp_alloc:%p\n",p3);

	void *p4 =mp_allock(&m,128);
	printf("4:mp_alloc:%p\n",p4);

	void *p5 =mp_allock(&m,256);
	printf("5:mp_alloc:%p\n",p5);
}