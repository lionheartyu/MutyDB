#include "kvstore.h"
#include <stdio.h>
#include <string.h>

void wal_replay();
void wal_append(const char *cmd, const char *key, const char *value); // 添加这一行

#define KVSTORE_MAX_TOKEN 128
#define WAL_MAX_ARCHIVE 4096 //最大wal.log文件数为4096个
#define WAL_MAX_SIZE (10*1024*1024) // 单个wal.log文件最大为10MB

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
        wal_append("SET", key, value);
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
        wal_append("DEL", key, NULL);
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
        wal_append("MOD", key, value);
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
        wal_append("RSET", key, value);
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
        wal_append("RGET", key, value);
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
        wal_append("RDEL", key, value);
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
        wal_append("RMOD", key, value);
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
        wal_append("HSET", key, value);
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
        wal_append("HET", key, value);
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
        wal_append("HDEL", key, value);
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
        wal_append("HMOD", key, value);
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

    wal_replay(); // 放到所有引擎初始化之后
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

/**
 * wal_replay - 启动时重放 WAL 日志，实现数据恢复。
 *
 * 依次读取 wal.log 文件中的每一条操作，并在内存中执行（如 SET、DEL），
 * 从而恢复上次运行时的数据状态，实现持久化恢复。
 */
void wal_replay() {
    char fname[32];
    // 先重放最老的归档，再重放最新的 wal.log
    for (int i = WAL_MAX_ARCHIVE; i >= 1; --i) {
        snprintf(fname, sizeof(fname), "wal.log.%d", i);
        FILE *fp = fopen(fname, "r");
        if (fp) {
            char cmd[16], key[256], value[256];
            while (fscanf(fp, "%15s %255s %255[^\n]", cmd, key, value) >= 2) {
                // 数组引擎
                if (strcmp(cmd, "SET") == 0 || strcmp(cmd, "MOD") == 0) {
                    kvstore_array_set(key, value);
                } else if (strcmp(cmd, "DEL") == 0) {
                    kvstore_array_del(key);
                }
                // 红黑树引擎
                else if (strcmp(cmd, "RSET") == 0 || strcmp(cmd, "RMOD") == 0) {
                    kvstore_rbtree_set(key, value);
                } else if (strcmp(cmd, "RDEL") == 0) {
                    kvstore_rbtree_delete(key);
                }
                // 哈希表引擎
                else if (strcmp(cmd, "HSET") == 0 || strcmp(cmd, "HMOD") == 0) {
                    kvstore_hash_set(key, value);
                } else if (strcmp(cmd, "HDEL") == 0) {
                    kvstore_hash_del(key);
                }
            }
            fclose(fp);
        }
    }
    // 最后重放当前 wal.log
    FILE *fp = fopen("wal.log", "r");
    if (fp) {
        char cmd[16], key[256], value[256];
        while (fscanf(fp, "%15s %255s %255[^\n]", cmd, key, value) >= 2) {
            if (strcmp(cmd, "SET") == 0 || strcmp(cmd, "MOD") == 0) {
                kvstore_array_set(key, value);
            } else if (strcmp(cmd, "DEL") == 0) {
                kvstore_array_del(key);
            }
            else if (strcmp(cmd, "RSET") == 0 || strcmp(cmd, "RMOD") == 0) {
                kvstore_rbtree_set(key, value);
            } else if (strcmp(cmd, "RDEL") == 0) {
                kvstore_rbtree_delete(key);
            }
            else if (strcmp(cmd, "HSET") == 0 || strcmp(cmd, "HMOD") == 0) {
                kvstore_hash_set(key, value);
            } else if (strcmp(cmd, "HDEL") == 0) {
                kvstore_hash_del(key);
            }
        }
        fclose(fp);
    }
}

// 归档轮转函数
void rotate_wal() {
    for (int i = WAL_MAX_ARCHIVE - 1; i >= 1; --i) {
        char oldname[32], newname[32];
        snprintf(oldname, sizeof(oldname), "wal.log.%d", i);
        snprintf(newname, sizeof(newname), "wal.log.%d", i + 1);
        rename(oldname, newname);
    }
    rename("wal.log", "wal.log.1");
    FILE *fp = fopen("wal.log", "w");
    if (fp) fclose(fp);
}

// 修改 wal_append，自动归档
FILE *wal_fp = NULL;
void wal_append(const char *cmd, const char *key, const char *value) {
    if (!wal_fp) {
        wal_fp = fopen("wal.log", "a");
    }
    if (wal_fp) {
        if (value)
            fprintf(wal_fp, "%s %s %s\n", cmd, key, value);
        else
            fprintf(wal_fp, "%s %s\n", cmd, key);
        fflush(wal_fp);
        // 检查文件大小，超限则归档
        long pos = ftell(wal_fp);
        if (pos > WAL_MAX_SIZE) {
            fclose(wal_fp);
            wal_fp = NULL;
            rotate_wal();
        }
    }
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
