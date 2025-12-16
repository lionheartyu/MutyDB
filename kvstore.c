#include "kvstore.h"

#define KVSTORE_MAX_TOKEN 128

const char *commands[] = {
    "SET",
    "GET",
    "DEL",
    "MOD",
    "COUNT",
    "RSET",
    "RGET",
    "RDEL",
    "RMOD",
    "RCOUNT",
    "HSET",
    "HGET",
    "HDEL",
    "HMOD",
    "HCOUNT",

};

enum
{
    KV_CMD_START = 0,
    KV_CMD_SET = KV_CMD_START,
    KV_CMD_GET,
    KV_CMD_DEL,
    KV_CMD_MOD,
    KV_CMD_COUNT,

    KV_CMD_RSET,
    KV_CMD_RGET,
    KV_CMD_RDEL,
    KV_CMD_RMOD,
    KV_CMD_RCOUNT,

    KV_CMD_HSET,
    KV_CMD_HGET,
    KV_CMD_HDEL,
    KV_CMD_HMOD,
    KV_CMD_HCOUNT,
    KV_CMD_SIZE,
};

void *kvstore_malloc(size_t size)
{
#if ENABLE_MEM_POOL
    char *ptr = mp_allock(&M);
#else
    return malloc(size);
#endif
}

void kvstore_free(void *ptr)
{
#if ENABLE_MEM_POOL

#else
    return free(ptr);
#endif
}

#if ENABLE_RBTREE_KVENGINE
int kvstore_rbtree_set(char *key, char *value)
{
    return kvs_rbtree_set(&Tree, key, value);
}
char *kvstore_rbtree_get(char *key)
{
    return kvs_rbtree_get(&Tree, key);
}
int kvstore_rbtree_delete(char *key)
{
    return kvs_rbtree_delete(&Tree, key);
}
int kvstore_rbtree_modify(char *key, char *value)
{
    return kvs_rbtree_modify(&Tree, key, value);
}

int kvstore_rbtree_count()
{
    return kvs_rbtree_count(&Tree);
}
#endif

#if ENABLE_ARRAY_KVENGINE
int kvstore_array_set(char *key, char *value)
{
    return kvs_array_set(&Array, key, value);
}

char *kvstore_array_get(char *key)
{
    return kvs_array_get(&Array, key);
}

int kvstore_array_del(char *key)
{
    return kvs_array_del(&Array, key);
}

int kvstore_array_mod(char *key, char *value)
{
    return kvs_array_mod(&Array, key, value);
}

int kvstore_array_count()
{
    return kvs_array_count(&Array);
}

#endif

#if ENABLE_HASH_KVENGINE
int kvstore_hash_set(char *key, char *value)
{
    return kvs_hash_set(&Hash, key, value);
}

char *kvstore_hash_get(char *key)
{
    return kvs_hash_get(&Hash, key);
}

int kvstore_hash_del(char *key)
{
    return kvs_hash_delete(&Hash, key);
}

int kvstore_hash_mod(char *key, char *value)
{
    return kvs_hash_modify(&Hash, key, value);
}

int kvstore_hash_count()
{
    return kvs_hash_count(&Hash);
}

#endif

int kvstore_split_token(char *msg, char **tokens)
{
    if (msg == NULL || tokens == NULL)
        return -1;

    int idx = 0;
    char *token = strtok(msg, " ");
    while (token != NULL)
    {
        tokens[idx++] = token;
        token = strtok(NULL, " ");
    }

    return idx;
}

int kvstore_parser_protocol(struct conn_item *item, char **tokens, int count)
{
    if (item == NULL || tokens[0] == NULL || count == 0)
        return -1;
    int cmd = KV_CMD_START;
    for (cmd = KV_CMD_START; cmd < KV_CMD_SIZE; cmd++)
    {
        if (strcmp(commands[cmd], tokens[0]) == 0)
            break;
    }

    char *msg = item->wbuffer;
    char *key = tokens[1];
    char *value = tokens[2];

    memset(msg, 0, BUFFER_LENGTH);

    switch (cmd)
    {
    case KV_CMD_SET:
    {
        int res = kvstore_array_set(key, value);
        if (res == 0)
        {
            snprintf(msg, BUFFER_LENGTH, "SUCCESS");
        }
        else
        {
            snprintf(msg, BUFFER_LENGTH, "FAILED");
        }
        break;
    }

    case KV_CMD_GET:
    {
        char *values = kvstore_array_get(key);
        if (values != NULL)
        {
            snprintf(msg, BUFFER_LENGTH, "%s", values);
        }
        else
        {
            snprintf(msg, BUFFER_LENGTH, "NO EXIST");
        }

        // printf("get:%s\n",values);
        break;
    }

    case KV_CMD_DEL:
    {
        // printf("del\n");
        int res = kvstore_array_del(key);
        if (res < 0)
        {
            snprintf(msg, BUFFER_LENGTH, "%s", "ERROR");
        }
        else if (res == 0)
        {
            snprintf(msg, BUFFER_LENGTH, "%s", "SUCCESS");
        }
        else
        {
            snprintf(msg, BUFFER_LENGTH, "NO EXIST");
        }
        break;
    }

    case KV_CMD_MOD:
    {
        // printf("mod\n");
        int res = kvstore_array_mod(key, value);
        if (res < 0)
        {
            snprintf(msg, BUFFER_LENGTH, "%s", "ERROR");
        }
        else if (res == 0)
        {
            snprintf(msg, BUFFER_LENGTH, "%s", "SUCCESS");
        }
        else
        {
            snprintf(msg, BUFFER_LENGTH, "NO EXIST");
        }
        break;
    }
    case KV_CMD_COUNT:
    {
        int count = kvs_array_count(&Array);
        if (count < 0)
        {
            snprintf(msg, BUFFER_LENGTH, "%s", "ERROR");
        }
        else
        {
            snprintf(msg, BUFFER_LENGTH, "%d", count);
        }
        break;
    }

    case KV_CMD_RSET:
    {
        int res = kvstore_rbtree_set(key, value);
        if (res == 0)
        {
            snprintf(msg, BUFFER_LENGTH, "SUCCESS");
        }
        else
        {
            snprintf(msg, BUFFER_LENGTH, "FAILED");
        }
        break;
    }

    case KV_CMD_RGET:
    {
        char *values = kvstore_rbtree_get(key);
        if (values != NULL)
        {
            snprintf(msg, BUFFER_LENGTH, "%s", values);
        }
        else
        {
            snprintf(msg, BUFFER_LENGTH, "NO EXIST");
        }
        break;
    }

    case KV_CMD_RDEL:
    {
        int res = kvstore_rbtree_delete(key);
        if (res < 0)
        {
            snprintf(msg, BUFFER_LENGTH, "%s", "ERROR");
        }
        else if (res == 0)
        {
            snprintf(msg, BUFFER_LENGTH, "%s", "SUCCESS");
        }
        else
        {
            snprintf(msg, BUFFER_LENGTH, "NO EXIST");
        }
        break;
    }

    case KV_CMD_RMOD:
    {
        int res = kvstore_rbtree_modify(key, value);
        if (res < 0)
        {
            snprintf(msg, BUFFER_LENGTH, "%s", "ERROR");
        }
        else if (res == 0)
        {
            snprintf(msg, BUFFER_LENGTH, "%s", "SUCCESS");
        }
        else
        {
            snprintf(msg, BUFFER_LENGTH, "NO EXIST");
        }
        break;
    }

    case KV_CMD_RCOUNT:
    {
        int count = kvstore_rbtree_count();
        if (count < 0)
        {
            snprintf(msg, BUFFER_LENGTH, "%s", "ERROR");
        }
        else
        {
            snprintf(msg, BUFFER_LENGTH, "%d", count);
        }
        break;
    }

    case KV_CMD_HSET:
    {
        int res = kvstore_hash_set(key, value);
        if (res == 0)
        {
            snprintf(msg, BUFFER_LENGTH, "SUCCESS");
        }
        else
        {
            snprintf(msg, BUFFER_LENGTH, "FAILED");
        }
        break;
    }

    case KV_CMD_HGET:
    {
        char *values = kvstore_hash_get(key);
        if (values != NULL)
        {
            snprintf(msg, BUFFER_LENGTH, "%s", values);
        }
        else
        {
            snprintf(msg, BUFFER_LENGTH, "NO EXIST");
        }
        break;
    }

    case KV_CMD_HDEL:
    {
        int res = kvstore_hash_del(key);
        if (res < 0)
        {
            snprintf(msg, BUFFER_LENGTH, "%s", "ERROR");
        }
        else if (res == 0)
        {
            snprintf(msg, BUFFER_LENGTH, "%s", "SUCCESS");
        }
        else
        {
            snprintf(msg, BUFFER_LENGTH, "NO EXIST");
        }
        break;
    }

    case KV_CMD_HMOD:
    {
        int res = kvstore_hash_mod(key, value);
        if (res < 0)
        {
            snprintf(msg, BUFFER_LENGTH, "%s", "ERROR");
        }
        else if (res == 0)
        {
            snprintf(msg, BUFFER_LENGTH, "%s", "SUCCESS");
        }
        else
        {
            snprintf(msg, BUFFER_LENGTH, "NO EXIST");
        }
        break;
    }

    case KV_CMD_HCOUNT:
    {
        int count = kvstore_hash_count();
        if (count < 0)
        {
            snprintf(msg, BUFFER_LENGTH, "%s", "ERROR");
        }
        else
        {
            snprintf(msg, BUFFER_LENGTH, "%d", count);
        }

        break;
    }

    default:
    {
        // printf("cmd%s\n",commands[cmd]);
        assert(0);
    }
    }
}

int kvstore_requese(struct conn_item *item)
{
    // printf("recv: %s\n",item->rbuffer);

    char *msg = item->rbuffer;
    size_t len = strlen(msg);
    while (len > 0 && (msg[len-1] == '\n' || msg[len-1] == '\r')) {
        msg[--len] = '\0';
    }

    char *tockens[KVSTORE_MAX_TOKEN];

    int count = kvstore_split_token(msg, tockens);

    int idx;
    for (idx = 0; idx < count; idx++)
    {
        // printf("idx:%s\n",tockens[idx]);
    }

    kvstore_parser_protocol(item, tockens, count);

    return 0;
}

int init_kvengine(void)
{
#if ENABLE_ARRAY_KVENGINE
    kvstore_array_create(&Array);
#endif

#if ENABLE_RBTREE_KVENGINE
    kvstore_rbtree_create(&Tree);
#endif

#if ENABLE_HASH_KVENGINE
    kvstore_hash_create(&Hash);
#endif
}

int exit_kvengine(void)
{
#if ENABLE_ARRAY_KVENGINE
    kvstore_array_destory(&Array);
#endif

#if ENABLE_RBTREE_KVENGINE
    kvstore_rbtree_destory(&Tree);
#endif

#if ENABLE_HASH_KVENGINE
    kvstore_hash_destory(&Hash);
#endif
}

int init_pool(void)
{
#if ENABLE_MEM_POOL
    mp_init(&M, 4096);
#endif
}

int destory_pool(void)
{
#if ENABLE_MEM_POOL
    mp_dest(&M);
#endif
}

int main()
{

    init_kvengine();
    init_pool();

#if (ENABLE_NETWORK_SELECT == NETWORK_EPOLL)
    epoll_entry();
#elif (ENABLE_NETWORK_SELECT == NETWORK_NTYCO)
    ntyco_enrty();
#elif (ENABLE_NETWORK_SELECT == NETWORK_IOURING)
    io_uring_entry();
#elif (ENABLE_NETWORK_SELECT == NETWORK_KMUDUO)
    kmuduo_enrty();

#endif

    exit_kvengine();
    destory_pool();
}
