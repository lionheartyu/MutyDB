#ifndef __KVSTORE_H__
#define __KVSTORE_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>

#define BUFFER_LENGTH 512

#ifdef ENABLE_LOG

#define LOG(_fmt, ...) fprintf(stdout, "[%s:%d]:%s"_fmt, __FILE__, __LINE__, __VAR_ARGS__)

#else

#define LOG(_fmt, ...)

#endif

typedef int (*RCALLBACK)(int fd);

// conn, fd, buffer, callback
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
// libevent -->



#ifdef __cplusplus
extern "C" {
#endif

int kvstore_requese(struct conn_item *item);

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {
#endif

int kmuduo_enrty();

#ifdef __cplusplus
}
#endif

int epoll_entry(void);
int ntyco_enrty(void);
int io_uring_entry(void);


void *kvstore_malloc(size_t size);
void kvstore_free(void *ptr);

#define NETWORK_EPOLL 0
#define NETWORK_NTYCO 1
#define NETWORK_IOURING 2
#define NETWORK_KMUDUO 3

#define ENABLE_ARRAY_KVENGINE 1
#define ENABLE_RBTREE_KVENGINE 1
#define ENABLE_HASH_KVENGINE 1

#define ENABLE_MEM_POOL 0
#define ENABLE_NETWORK_SELECT NETWORK_NTYCO

#if ENABLE_MEM_POOL
typedef struct mempool_s mempool_t;
extern mempool_t M;
int mp_init(mempool_t *m, int size);
void mp_dest(mempool_t *m);
void *mp_allock(mempool_t *m);
void mp_free(mempool_t *m, void *ptr);

#endif

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

#if ENABLE_RBTREE_KVENGINE

typedef struct _rbtree _rbtree_t;
extern _rbtree_t Tree;
int kvstore_rbtree_create(_rbtree_t *tree);
void kvstore_rbtree_destory(_rbtree_t *tree);
int kvs_rbtree_set(_rbtree_t *tree, char *key, char *value);
char *kvs_rbtree_get(_rbtree_t *tree, char *key);
int kvs_rbtree_delete(_rbtree_t *tree, char *key);
int kvs_rbtree_modify(_rbtree_t *tree, char *key, char *value);
int kvs_rbtree_count(_rbtree_t *tree);

#endif

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

#endif