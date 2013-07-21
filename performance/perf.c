#include "perf.h"
#include "common.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <pthread.h>
#include "logfile.h"
#include "utils.h"
#include "memlink_client.h"

#define MEMLINK_HOST        "127.0.0.1"
#define MEMLINK_PORT_READ   11011
#define MEMLINK_PORT_WRITE  11012
#define MEMLINK_TIMEOUT     30

int test_ping(TestArgs *args)
{
    MemLink *m;
    int ret, i;
    
    DINFO("====== ping ======\n");
    if (args->longconn) {
        m = memlink_create(args->host, args->rport, args->wport, args->timeout);
        if (NULL == m) {
            DERROR("memlink_create error!\n");
            exit(-1);
            return -1;
        }
        for (i = 0; i < args->testcount; i++) {
            ret = memlink_cmd_ping(m);        
            if (ret != MEMLINK_OK) {
                DERROR("ping error: %d\n", ret);
                return -2;
            }
        }
        memlink_destroy(m);
    }else{
        for (i = 0; i < args->testcount; i++) {
            m = memlink_create(args->host, args->rport, args->wport, args->timeout);
            if (NULL == m) {
                DERROR("memlink_create error!\n");
                exit(-1);
                return -1;
            }
            ret = memlink_cmd_ping(m);        
            if (ret != MEMLINK_OK) {
                DERROR("ping error: %d\n", ret);
                return -2;
            }
            memlink_destroy(m);
        }
    }

    return 0;
}


int test_create_node(TestArgs *args)
{
    MemLink *m;
    int ret, i;
    char key[512] = {0};
    //char name[512] = {0}; 
    DINFO("====== create ======\n");

    if (args->longconn) {
        m = memlink_create(args->host, args->rport, args->wport, args->timeout);
        if (NULL == m) {
            DERROR("memlink_create error!\n");
            exit(-1);
            return -1;
        }
        /*char *sp = strchr(args->key, '.');
        if (NULL == sp) {
            DERROR("must set table name in key. eg: table_name.key_name\n"); 
            exit(-1);
            return -1;
        }
        char *start = args->key;
        i = 0;
        while (start != sp) {
            name[i] = *start;
            i++;
            start++;
        }
        name[i] = 0;
        */
        ret = memlink_cmd_create_table_list(m, args->table, args->valuesize, args->attrstr);
        if (ret != MEMLINK_OK) {
            DERROR("create table list error! ret:%d\n", ret);
            exit(-1);
            return -2;
        }

        for (i = 0; i < args->testcount; i++) {
            sprintf(key, "%s%d", args->key, args->tid * args->testcount + i);
            ret = memlink_cmd_create_node(m, args->table, key);
            if (ret != MEMLINK_OK) {
                DERROR("create list error! ret:%d\n", ret);
                return -2;
            }
        }
        memlink_destroy(m); 
    }else{
        for (i = 0; i < args->testcount; i++) {
            m = memlink_create(args->host, args->rport, args->wport, args->timeout);
            if (NULL == m) {
                DERROR("memlink_create error!\n");
                exit(-1);
                return -1;
            }
            sprintf(key, "%s%d", args->key, args->tid * args->testcount + i);
            ret = memlink_cmd_create_table_list(m, key, args->valuesize, args->attrstr);
            if (ret != MEMLINK_OK) {
                DERROR("create list error! ret:%d\n", ret);
                return -2;
            }
            memlink_destroy(m); 
        }
    }

    return 0;
}

int test_insert(TestArgs *args)
{
    MemLink *m;
    int ret, i;
    int pos;
    char value[512] = {0};
    char format[64] = {0};
    char attrstr[64] = {0};
    
    DINFO("====== insert ======\n");
    if (args->table[0] == 0 || args->key[0] == 0 || args->valuesize == 0) {
        DERROR("table, key and valuesize must not null!\n");
        return -1;
    }
    pos = args->pos;

    sprintf(format, "%%0%dd", args->valuesize);
    if (args->longconn) {
        m = memlink_create(args->host, args->rport, args->wport, args->timeout);
        if (NULL == m) {
            DERROR("memlink_create error!\n");
            exit(-1);
            return -1;
        }
        for (i = 0; i < args->testcount; i++) {
            sprintf(value, format, i);
            if (pos < -1) {
                pos = random();
            }
            //sprintf(attrstr, "%d:%d", args->tid * args->testcount + i, 1);
            ret = memlink_cmd_insert(m, args->table, args->key, value, args->valuesize, args->attrstr, pos);
            if (ret != MEMLINK_OK) {
                DERROR("insert error! ret:%d, table:%s key:%s,value:%s,attr:%s,pos:%d\n", 
                        ret, args->table, args->key, value, args->attrstr, args->pos);
                return -2;
            }
        }
        memlink_destroy(m); 
    }else{
        for (i = 0; i < args->testcount; i++) {
            m = memlink_create(args->host, args->rport, args->wport, args->timeout);
            if (NULL == m) {
                DERROR("memlink_create error!\n");
                exit(-1);
                return -1;
            }
            sprintf(value, format, i);
            //sprintf(attrstr, "%d:%d", args->tid * args->testcount + i, 1);
            ret = memlink_cmd_insert(m, args->table, args->key, value, args->valuesize, args->attrstr, args->pos);
            //ret = memlink_cmd_insert(m, args->key, value, args->valuesize, attrstr, args->pos);
            if (ret != MEMLINK_OK) {
                DERROR("insert error! ret:%d, table:%s key:%s,value:%s,attr:%s,pos:%d\n", 
                        ret, args->table, args->key, value, args->attrstr, args->pos);
                return -2;
            }
            memlink_destroy(m); 
        }
    }

    return 0;
}

int test_range(TestArgs *args)
{
    MemLink *m;
    int ret, i;

    DINFO("====== range ======\n");
    if (args->longconn) {
        m = memlink_create(args->host, args->rport, args->wport, args->timeout);
        if (NULL == m) {
            DERROR("memlink_create error!\n");
            exit(-1);
            return -1;
        }
        for (i = 0; i < args->testcount; i++) {
            MemLinkResult   result;
            ret = memlink_cmd_range(m, args->table, args->key, args->kind, args->attrstr, args->from, args->len, &result);
            if (ret != MEMLINK_OK) {
                DERROR("range error! ret:%d, kind:%d,attr:%s,from:%d,len:%d\n", 
                        ret, args->kind, args->attrstr, args->from, args->len);
                return -2;
            }
            if (result.count != args->len) {
                DERROR("range result count error: %d, len:%d\n", result.count, args->len);
            }
            memlink_result_free(&result);
        }
        memlink_destroy(m); 
    }else{
        for (i = 0; i < args->testcount; i++) {
            m = memlink_create(args->host, args->rport, args->wport, args->timeout);
            if (NULL == m) {
                DERROR("memlink_create error!\n");
                exit(-1);
                return -1;
            }
            MemLinkResult result;
            ret = memlink_cmd_range(m, args->table, args->key, args->kind, args->attrstr, args->from, args->len, &result);
            if (ret != MEMLINK_OK) {
                DERROR("range error! ret:%d, kind:%d,attr:%s,from:%d,len:%d\n", 
                        ret, args->kind, args->attrstr, args->from, args->len);
                return -2;
            }
            if (result.count != args->len) {
                DERROR("range result count error: %d, len:%d\n", result.count, args->len);
            }
            memlink_result_free(&result);
            memlink_destroy(m); 
        }
    }

    return 0;
}

int test_move(TestArgs *args)
{
    MemLink *m;
    int ret, i;
    char value[512] = {0};
    int valint = atoi(args->value);
    char format[64];

    DINFO("====== move ======\n");
    sprintf(format, "%%0%dd", args->valuesize);

    if (args->longconn) {
        m = memlink_create(args->host, args->rport, args->wport, args->timeout);
        if (NULL == m) {
            DERROR("memlink_create error!\n");
            exit(-1);
            return -1;
        }
        for (i = 0; i < args->testcount; i++) {
            sprintf(value, format, valint);
            ret = memlink_cmd_move(m, args->table, args->key, value, args->valuesize, args->pos);
            if (ret != MEMLINK_OK) {
                DERROR("move error! ret:%d, table:%s key:%s,value:%s,pos:%d\n", 
                        ret, args->table, args->key, value, args->pos);
                return -2;
            }
            valint++;
        }
        memlink_destroy(m); 
    }else{
        for (i = 0; i < args->testcount; i++) {
            m = memlink_create(args->host, args->rport, args->wport, args->timeout);
            if (NULL == m) {
                DERROR("memlink_create error!\n");
                exit(-1);
                return -1;
            }
            sprintf(value, format, valint);
            ret = memlink_cmd_move(m, args->table, args->key, value, args->valuesize, args->pos);
            if (ret != MEMLINK_OK) {
                DERROR("move error! ret:%d, table:%s key:%s,value:%s,pos:%d\n", 
                    ret, args->table, args->key, value, args->pos);
                return -2;
            }
            valint++;
            memlink_destroy(m); 
        }
    }

    return 0;
}
int test_del(TestArgs *args)
{
    MemLink *m;
    int ret, i;
    
    DINFO("====== del ======\n");
    if (args->longconn) {
        m = memlink_create(args->host, args->rport, args->wport, args->timeout);
        if (NULL == m) {
            DERROR("memlink_create error!\n");
            exit(-1);
            return -1;
        }
        for (i = 0; i < args->testcount; i++) {
            ret = memlink_cmd_del(m, args->table, args->key, args->value, args->valuelen);
            if (ret != MEMLINK_OK) {
                DERROR("del error! ret:%d, table:%s key:%s, value:%s\n", 
                        ret, args->table, args->key, args->value);
                return -2;
            }
        }
        memlink_destroy(m); 
    }else{
        for (i = 0; i < args->testcount; i++) {
            m = memlink_create(args->host, args->rport, args->wport, args->timeout);
            if (NULL == m) {
                DERROR("memlink_create error!\n");
                exit(-1);
                return -1;
            }
            ret = memlink_cmd_del(m, args->table, args->key, args->value, args->valuelen);
            if (ret != MEMLINK_OK) {
                DERROR("del error! ret:%d, table:%s key:%s, value:%s\n", 
                    ret, args->table, args->key, args->value);
                return -2;
            }
            memlink_destroy(m); 
        }
    }
    return 0;
}

int test_attr(TestArgs *args)
{
    MemLink *m;
    int ret, i;
    
    DINFO("====== attr ======\n");
    if (args->longconn) {
        m = memlink_create(args->host, args->rport, args->wport, args->timeout);
        if (NULL == m) {
            DERROR("memlink_create error!\n");
            exit(-1);
            return -1;
        }
        for (i = 0; i < args->testcount; i++) {
            ret = memlink_cmd_attr(m, args->table, args->key, args->value, args->valuelen, args->attrstr);
            if (ret != MEMLINK_OK) {
                DERROR("attr error! ret:%d, table:%s key:%s, value:%s, attr:%s\n", ret, args->table, args->key, args->value, args->attrstr);
                return -2;
            }
        }
        memlink_destroy(m); 
    }else{
        for (i = 0; i < args->testcount; i++) {
            m = memlink_create(args->host, args->rport, args->wport, args->timeout);
            if (NULL == m) {
                DERROR("memlink_create error!\n");
                exit(-1);
                return -1;
            }
            ret = memlink_cmd_attr(m, args->table, args->key, args->value, args->valuelen, args->attrstr);
            if (ret != MEMLINK_OK) {
                DERROR("attr error! ret:%d, table:%s key:%s, value:%s, attr:%s\n", ret, args->table, args->key, args->value, args->attrstr);
                return -2;
            }
            memlink_destroy(m); 
        }
    }
    return 0;
}

int test_tag(TestArgs *args)
{
    MemLink *m;
    int ret, i;
    
    DINFO("====== tag ======\n");
    if (args->longconn) {
        m = memlink_create(args->host, args->rport, args->wport, args->timeout);
        if (NULL == m) {
            DERROR("memlink_create error!\n");
            exit(-1);
            return -1;
        }
        for (i = 0; i < args->testcount; i++) {
            ret = memlink_cmd_tag(m, args->table, args->key, args->value, args->valuelen, args->tag);
            if (ret != MEMLINK_OK) {
                DERROR("tag error! ret:%d, table:%s key:%s, value:%s, tag:%d\n", ret, args->table, args->key, args->value, args->tag);
                return -2;
            }
        }
        memlink_destroy(m); 
    }else{
        for (i = 0; i < args->testcount; i++) {
            m = memlink_create(args->host, args->rport, args->wport, args->timeout);
            if (NULL == m) {
                DERROR("memlink_create error!\n");
                exit(-1);
                return -1;
            }
            ret = memlink_cmd_tag(m, args->table, args->key, args->value, args->valuelen, args->tag);
            if (ret != MEMLINK_OK) {
                DERROR("tag error! ret:%d, table:%s key:%s, value:%s, tag:%d\n", ret, args->table, args->key, args->value, args->tag);
                return -2;
            }
            memlink_destroy(m); 
        }
    }
    return 0;
}

int test_count(TestArgs *args)
{
    MemLink *m;
    int ret, i;
    
    DINFO("====== count ======\n");
    if (args->longconn) {
        m = memlink_create(args->host, args->rport, args->wport, args->timeout);
        if (NULL == m) {
            DERROR("memlink_create error!\n");
            exit(-1);
            return -1;
        }
        for (i = 0; i < args->testcount; i++) {
            MemLinkCount result;
            ret = memlink_cmd_count(m, args->table, args->key, args->attrstr, &result);
            if (ret != MEMLINK_OK) {
                DERROR("count error! ret:%d, table:%s key:%s, attr:%s\n", 
                    ret, args->table, args->key, args->attrstr);
                return -2;
            }
        }
        memlink_destroy(m); 
    }else{
        for (i = 0; i < args->testcount; i++) {
            m = memlink_create(args->host, args->rport, args->wport, args->timeout);
            if (NULL == m) {
                DERROR("memlink_create error!\n");
                exit(-1);
                return -1;
            }
            MemLinkCount result;
            ret = memlink_cmd_count(m, args->table, args->key, args->attrstr, &result);
            if (ret != MEMLINK_OK) {
                DERROR("count error! ret:%d, table:%s key:%s, attr:%s\n", ret, args->table, args->key, args->attrstr);
                return -2;
            }
            memlink_destroy(m); 
        }
    }
    return 0;
}

int test_lpush(TestArgs *args)
{
    MemLink *m;
    int ret, i;
    int pos;
    char value[512] = {0};
    char format[64] = {0};
    char attrstr[64] = {0};
    
    DINFO("====== lpush ======\n");
    if (args->table[0] == 0 || args->key[0] == 0 || args->valuesize == 0) {
        DERROR("table, key and valuesize must not null!\n");
        return -1;
    }
    pos = args->pos;

    sprintf(format, "%%0%dd", args->valuesize);
    if (args->longconn) {
        m = memlink_create(args->host, args->rport, args->wport, args->timeout);
        if (NULL == m) {
            DERROR("memlink_create error!\n");
            exit(-1);
            return -1;
        }
        for (i = 0; i < args->testcount; i++) {
            sprintf(value, format, i);
            if (pos < -1) {
                pos = random();
            }
            //sprintf(attrstr, "%d:%d", args->tid * args->testcount + i, 1);
            ret = memlink_cmd_lpush(m, args->table, args->key, value, args->valuesize, args->attrstr);
            if (ret != MEMLINK_OK) {
                DERROR("lpush error! ret:%d, table:%s key:%s,value:%s,attr:%s,pos:%d\n", 
                        ret, args->table, args->key, value, args->attrstr, args->pos);
                return -2;
            }
        }
        memlink_destroy(m); 
    }else{
        for (i = 0; i < args->testcount; i++) {
            m = memlink_create(args->host, args->rport, args->wport, args->timeout);
            if (NULL == m) {
                DERROR("memlink_create error!\n");
                exit(-1);
                return -1;
            }
            sprintf(value, format, i);
            //sprintf(attrstr, "%d:%d", args->tid * args->testcount + i, 1);
            ret = memlink_cmd_lpush(m, args->table, args->key, value, args->valuesize, args->attrstr);
            //ret = memlink_cmd_insert(m, args->key, value, args->valuesize, attrstr, args->pos);
            if (ret != MEMLINK_OK) {
                DERROR("lpush error! ret:%d, table:%s key:%s,value:%s,attr:%s,pos:%d\n", 
                        ret, args->table, args->key, value, args->attrstr, args->pos);
                return -2;
            }
            memlink_destroy(m); 
        }
    }

    return 0;
}

int test_rpush(TestArgs *args)
{
    MemLink *m;
    int ret, i;
    int pos;
    char value[512] = {0};
    char format[64] = {0};
    char attrstr[64] = {0};
    
    DINFO("====== rpush ======\n");
    if (args->table[0] == 0 || args->key[0] == 0 || args->valuesize == 0) {
        DERROR("key and valuesize must not null!\n");
        return -1;
    }
    pos = args->pos;

    sprintf(format, "%%0%dd", args->valuesize);
    if (args->longconn) {
        m = memlink_create(args->host, args->rport, args->wport, args->timeout);
        if (NULL == m) {
            DERROR("memlink_create error!\n");
            exit(-1);
            return -1;
        }
        for (i = 0; i < args->testcount; i++) {
            sprintf(value, format, i);
            if (pos < -1) {
                pos = random();
            }
            //sprintf(attrstr, "%d:%d", args->tid * args->testcount + i, 1);
            ret = memlink_cmd_rpush(m, args->table, args->key, value, args->valuesize, args->attrstr);
            if (ret != MEMLINK_OK) {
                DERROR("rpush error! ret:%d, table:%s key:%s,value:%s,attr:%s,pos:%d\n", 
                        ret, args->table, args->key, value, args->attrstr, args->pos);
                return -2;
            }
        }
        memlink_destroy(m); 
    }else{
        for (i = 0; i < args->testcount; i++) {
            m = memlink_create(args->host, args->rport, args->wport, args->timeout);
            if (NULL == m) {
                DERROR("memlink_create error!\n");
                exit(-1);
                return -1;
            }
            sprintf(value, format, i);
            //sprintf(attrstr, "%d:%d", args->tid * args->testcount + i, 1);
            ret = memlink_cmd_rpush(m, args->table, args->key, value, args->valuesize, args->attrstr);
            //ret = memlink_cmd_insert(m, args->key, value, args->valuesize, attrstr, args->pos);
            if (ret != MEMLINK_OK) {
                DERROR("rpush error! ret:%d, table:%s key:%s,value:%s,attr:%s,pos:%d\n", 
                        ret, args->table, args->key, value, args->attrstr, args->pos);
                return -2;
            }
            memlink_destroy(m); 
        }
    }

    return 0;
}

int test_lpop(TestArgs *args)
{
    MemLink *m;
    int ret, i;

    DINFO("====== lpop ======\n");
    if (args->longconn) {
        m = memlink_create(args->host, args->rport, args->wport, args->timeout);
        if (NULL == m) {
            DERROR("memlink_create error!\n");
            exit(-1);
            return -1;
        }
        for (i = 0; i < args->testcount; i++) {
            MemLinkResult   result;
            ret = memlink_cmd_lpop(m, args->table, args->key, args->len, &result);
            if (ret != MEMLINK_OK) {
                DERROR("lpop error! ret:%d\n", ret);
                return -2;
            }
            if (result.count != args->len) {
                DERROR("lpop result count error: %d, len:%d\n", result.count, args->len);
            }
            memlink_result_free(&result);
        }
        memlink_destroy(m); 
    }else{
        for (i = 0; i < args->testcount; i++) {
            m = memlink_create(args->host, args->rport, args->wport, args->timeout);
            if (NULL == m) {
                DERROR("memlink_create error!\n");
                exit(-1);
                return -1;
            }
            MemLinkResult result;
            ret = memlink_cmd_lpop(m, args->table, args->key, args->len, &result);
            if (ret != MEMLINK_OK) {
                DERROR("lpop error! ret:%d\n", ret);
                return -2;
            }
            if (result.count != args->len) {
                DERROR("lpop result count error: %d, len:%d\n", result.count, args->len);
            }
            memlink_result_free(&result);
            memlink_destroy(m); 
        }
    }

    return 0;
}

int test_rpop(TestArgs *args)
{
    MemLink *m;
    int ret, i;

    DINFO("====== rpop ======\n");
    if (args->longconn) {
        m = memlink_create(args->host, args->rport, args->wport, args->timeout);
        if (NULL == m) {
            DERROR("memlink_create error!\n");
            exit(-1);
            return -1;
        }
        for (i = 0; i < args->testcount; i++) {
            MemLinkResult   result;
            ret = memlink_cmd_rpop(m, args->table, args->key, args->len, &result);
            if (ret != MEMLINK_OK) {
                DERROR("range error! ret:%d\n", ret);
                return -2;
            }
            if (result.count != args->len) {
                DERROR("rpop result count error: %d, len:%d\n", result.count, args->len);
            }
            memlink_result_free(&result);
        }
        memlink_destroy(m); 
    }else{
        for (i = 0; i < args->testcount; i++) {
            m = memlink_create(args->host, args->rport, args->wport, args->timeout);
            if (NULL == m) {
                DERROR("memlink_create error!\n");
                exit(-1);
                return -1;
            }
            MemLinkResult result;
            ret = memlink_cmd_rpop(m, args->table, args->key, args->len, &result);
            if (ret != MEMLINK_OK) {
                DERROR("rpop error! ret:%d\n", ret);
                return -2;
            }
            if (result.count != args->len) {
                DERROR("rpop result count error: %d, len:%d\n", result.count, args->len);
            }
            memlink_result_free(&result);
            memlink_destroy(m); 
        }
    }

    return 0;
}

int test_sl_insert(TestArgs *args)
{
    MemLink *m;
    int ret, i;

    DINFO("====== sl_insert ======\n");
    if (args->longconn) {
        m = memlink_create(args->host, args->rport, args->wport, args->timeout);
        if (NULL == m) {
            DERROR("memlink_create error!\n");
            exit(-1);
            return -1;
        }
        for (i = 0; i < args->testcount; i++) {
            MemLinkResult   result;
            ret = memlink_cmd_rpop(m, args->table, args->key, args->len, &result);
            if (ret != MEMLINK_OK) {
                DERROR("range error! ret:%d\n", ret);
                return -2;
            }
            if (result.count != args->len) {
                DERROR("rpop result count error: %d, len:%d\n", result.count, args->len);
            }
            memlink_result_free(&result);
        }
        memlink_destroy(m); 

    }else{
    }
 
    return 0;
}
int test_sl_range(TestArgs *args)
{
    MemLink *m;
    int ret, i;

    DINFO("====== sl_range ======\n");
    if (args->longconn) {
        m = memlink_create(args->host, args->rport, args->wport, args->timeout);
        if (NULL == m) {
            DERROR("memlink_create error!\n");
            exit(-1);
            return -1;
        }
        for (i = 0; i < args->testcount; i++) {
            MemLinkResult   result;
            ret = memlink_cmd_rpop(m, args->table, args->key, args->len, &result);
            if (ret != MEMLINK_OK) {
                DERROR("range error! ret:%d\n", ret);
                return -2;
            }
            if (result.count != args->len) {
                DERROR("rpop result count error: %d, len:%d\n", result.count, args->len);
            }
            memlink_result_free(&result);
        }
        memlink_destroy(m); 


    }else{
    }
 
    return 0;
}
int test_sl_count(TestArgs *args)
{
    MemLink *m;
    int ret, i;

    DINFO("====== sl_count ======\n");
    if (args->longconn) {
    }else{
    }
 
    return 0;
}

int test_sl_del(TestArgs *args)
{
    MemLink *m;
    int ret, i;

    DINFO("====== sl_del ======\n");
    if (args->longconn) {
    }else{
    }
 
    return 0;
}



volatile int    thread_create_count = 0;
volatile int    thread_run = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cond = PTHREAD_COND_INITIALIZER;

void* thread_start(void *args)
{
    ThreadArgs  *ta = (ThreadArgs*)args;
    struct timeval start, end;  

    pthread_mutex_lock(&lock);
    //while (thread_create_count < ta->threads) {
    while (thread_run == 0) {
        if (thread_create_count == ta->threads) {
            thread_run = 1;

            int ret = pthread_cond_broadcast(&cond);
            if (ret != 0) {
                char errbuf[1024];
                strerror_r(errno, errbuf, 1024);
                DERROR("pthread_cond_broadcase error: %s\n",  errbuf);
            }
        }else{
            pthread_cond_wait(&cond, &lock);
        }
        DINFO("cond wait return ...\n");
    }   
    pthread_mutex_unlock(&lock);

    gettimeofday(&start, NULL);
    int ret = ta->func(ta->args);
    gettimeofday(&end, NULL);

    unsigned int tmd = timediff(&start, &end);
    double speed = ((double)ta->args->testcount / tmd) * 1000000;
    double onetime = 1000 / speed;
    DINFO("thread test use time:%u, speed:%.2f, time:%.2f\n", tmd, speed, onetime);
    long mytime = (int)ceil(onetime);
    if (ret < 0) {
        mytime = mytime  * -1;
    }
    return (void*)mytime;
}

int single_start(ThreadArgs *ta)
{
    struct timeval start, end;  

    gettimeofday(&start, NULL);
    int ret = ta->func(ta->args);
    gettimeofday(&end, NULL);

    unsigned int tmd = timediff(&start, &end);
    double speed = ((double)ta->args->testcount / tmd) * 1000000;
    DINFO("thread test use time:%u, speed:%.2f\n", tmd, speed);
    
    free(ta->args);
    free(ta);

    return ret;
}

int test_start(TestConfig *cf)
{
    pthread_t       threads[THREAD_MAX_NUM] = {0};
    struct timeval  start, end;
    int i, ret;

    DINFO("====== test start with thread:%d ======\n", cf->threads);
    for (i = 0; i < cf->threads; i++) {
        ThreadArgs  *ta = (ThreadArgs*)malloc(sizeof(ThreadArgs));
        TestArgs    *ts = (TestArgs*)malloc(sizeof(TestArgs));
        memcpy(ts, &cf->args, sizeof(TestArgs));
        ts->tid  = i;
        ta->func = cf->func;
        ta->args = ts;
        ta->threads = cf->threads;
        
        thread_create_count += 1;
        ret = pthread_create(&threads[i], NULL, thread_start, ta); 
        if (ret != 0) {
            char errbuf[1024];
            strerror_r(errno, errbuf, 1024);
            DERROR("pthread_create error! %s\n",  errbuf);
            return -1;
        }
    }

    gettimeofday(&start, NULL);
    /*ret = pthread_cond_broadcast(&cond);
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("pthread_cond_broadcase error: %s\n",  errbuf);
    }*/

    long reqtime[1000] = {0};

    for (i = 0; i < cf->threads; i++) {
        pthread_join(threads[i], (void**)&reqtime[i]);
    }
    gettimeofday(&end, NULL);

    ret = 0;
    int sum = 0;
    for (i = 0; i < cf->threads; i++) {
        if (reqtime[i] > 0) {
            sum += reqtime[i];
        }else{
            sum += reqtime[i] * -1;
            ret = -1;
        }
    }

    unsigned int tmd = timediff(&start, &end);
    double speed = (((double)cf->args.testcount * cf->threads)/ tmd) * 1000000;
    DINFO("thread all test use time:%u, speed:%.2f, onetime:%.2f\n", tmd, speed, (float)sum/cf->threads);

    return ret;
}

TestFuncLink gfuncs[] = {{"ping", test_ping},
                         {"create", test_create_node}, 
                         {"insert", test_insert},
                         {"range", test_range},
                         {"move", test_move},
                         {"del", test_del},
                         {"attr", test_attr},
                         {"tag", test_tag},
                         {"count", test_count},
                         {"lpush", test_lpush},
                         {"rpush", test_rpush},
                         {"lpop", test_lpop},
                         {"rpop", test_rpop},
                         {"slinsert", test_sl_insert},
                         {"slrange", test_sl_range},
                         {"slcount", test_sl_count},
                         {"sldel", test_sl_del},
                         {NULL, NULL}};

TestFunc funclink_find(char *name)
{
    int i = 0;
    TestFuncLink    *flink;

    while (1) {
        flink = &gfuncs[i];
        if (flink->name == NULL)
            break;
        if (strcmp(flink->name, name) == 0) {
            return flink->func;
        }
        i++;
    }
    return NULL;
}


int show_help()
{
    printf("usage:\n\tperf [options]\n");
    printf("options:\n");
    printf("\t--host,-h\tmemlink server ip\n");
    printf("\t--readport,-r\tserver read port. default 11001\n");
    printf("\t--writeport,-w\tserver write port. default 11002\n");
    printf("\t--thread,-t\tthread count for test\n");
    printf("\t--count,-n\tinsert number\n");
    printf("\t--from,-f\trange from position\n");
    printf("\t--len,-l\trange length\n");
    printf("\t--do,-d\t\tping/create/insert/range/move/del/attr/tag/count/lpush/rpush/lpop/rpop\n");
    printf("\t--key,-k\tkey\n");
    printf("\t--value,-v\tvalue\n");
    printf("\t--valuesize,-s\tvaluesize for create\n");
    printf("\t--pos,-p\tposition for insert\n");
    printf("\t--popnum,-o\tpop number\n");
    printf("\t--kind,-i\tkind for range. all/visible/tagdel\n");
    printf("\t--attr,-m\tattr str\n");
    printf("\t--longconn,-c\tuse long connection for test. default 1\n");
    printf("\n\n");

    exit(0);
}

int main(int argc, char *argv[])
{
    logfile_create("stdout", 4);

    int     optret;
    int     optidx = 0;
    char    *optstr = "h:t:n:f:l:k:v:s:p:o:i:a:m:d:c:r:w:";
    struct option myoptions[] = {{"host", 0, NULL, 'h'},
                                 {"readport", 0, NULL, 'r'},
                                 {"writeport", 0, NULL, 'w'},
                                 {"thread", 0, NULL, 't'},
                                 {"count", 0, NULL, 'n'},
                                 {"frompos", 0, NULL, 'f'},
                                 {"len", 0, NULL, 'l'},
                                 {"table", 0, NULL, 'b'},
                                 {"key", 0, NULL, 'k'},
                                 {"value", 0, NULL, 'v'},
                                 {"valuesize", 0, NULL, 's'},
                                 {"pos", 0, NULL, 'p'},
                                 {"popnum", 0, NULL, 'o'},
                                 {"kind", 0, NULL, 'i'},
                                 {"tag", 0, NULL, 'a'},
                                 {"attr", 0, NULL, 'm'},
                                 {"do", 0, NULL, 'd'},
                                 {"longconn", 0, NULL, 'c'},
                                 {NULL, 0, NULL, 0}};


    if (argc < 2) {
        show_help();
        return -1;
    }
    
    TestConfig tcf;
    memset(&tcf, 0, sizeof(TestConfig));

    sprintf(tcf.args.host, "%s", MEMLINK_HOST);
    tcf.args.rport = MEMLINK_PORT_READ;
    tcf.args.wport = MEMLINK_PORT_WRITE;
    tcf.args.timeout = MEMLINK_TIMEOUT;

    while(optret = getopt_long(argc, argv, optstr, myoptions, &optidx)) {
        if (0 > optret) {
            break;
        }

        switch (optret) {
        case 'f':
            tcf.args.from = atoi(optarg);    
            break;
        case 't':
            tcf.threads = atoi(optarg);
            break;
        case 'n':
            tcf.args.testcount   = atoi(optarg);
            break;
        case 'c':
            tcf.args.longconn = atoi(optarg);
            break;
        case 'l':
            tcf.args.len = atoi(optarg);
            break;        
        case 'd':
            snprintf(tcf.action, 64, "%s", optarg);
            tcf.func = funclink_find(tcf.action);
            if (tcf.func == NULL) {
                printf("--do/-d error: %s\n", optarg);
                exit(0);
            }
            break;
        case 'b':
            snprintf(tcf.args.table, 255, "%s", optarg);
            break;

        case 'k':
            snprintf(tcf.args.key, 255, "%s", optarg);
            break;
        case 'v':
            snprintf(tcf.args.value, 255, "%s", optarg);
            tcf.args.valuelen = strlen(optarg);
            break;
        case 's':
            tcf.args.valuesize = atoi(optarg);
            break;
        case 'p':
            tcf.args.pos = atoi(optarg);
            break;
        case 'o':
            tcf.args.popnum = atoi(optarg);
            break;
        case 'i':
            if (strcmp(optarg, "all") == 0) {
                tcf.args.kind = MEMLINK_VALUE_ALL;
            }else if (strcmp(optarg, "visible") == 0) {
                tcf.args.kind = MEMLINK_VALUE_VISIBLE;
            }else if (strcmp(optarg, "tagdel") == 0) {
                tcf.args.kind = MEMLINK_VALUE_TAGDEL;
            }else{
                printf("--kind/-i set error! %s\n", optarg);
                exit(0);
            }
            break;
        case 'a':
            if (strcmp(optarg, "del") == 0) {
                tcf.args.tag = MEMLINK_TAG_DEL;
            }else if (strcmp(optarg, "restore") == 0) {
                tcf.args.tag = MEMLINK_TAG_RESTORE;
            }else{
                printf("--tag/-a set error! %s\n", optarg);
                exit(0);
            }
            break;
        case 'm':
            snprintf(tcf.args.attrstr, 255, "%s", optarg);
            break;
        case 'h':
            sprintf(tcf.args.host, "%s", optarg);
            break;
        case 'r':
            tcf.args.rport = atoi(optarg);
            break;
        case 'w':
            tcf.args.wport = atoi(optarg);
            break;
        default:
            printf("error option: %c\n", optret);
            exit(-1);
            break;
        }
    }

    DINFO("memlink %s r:%d w:%d\n", tcf.args.host, tcf.args.rport, tcf.args.wport);    

    if (tcf.threads > 0) {
        return test_start(&tcf);
    }else{
        ThreadArgs  ta;// = (ThreadArgs*)malloc(sizeof(ThreadArgs));
        ta.func = tcf.func;
        ta.args = &tcf.args;
        ta.threads = tcf.threads;
        
        return single_start(&ta);
    }

    return 0;
}
