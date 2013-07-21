#include <stdio.h>
#include <stdlib.h>
#include "memlink_client.h"
#include "logfile.h"
#include "test.h"

int main(int argc, char **argv)
{
    MemLink *m;
#ifdef DEBUG
    logfile_create("stdout", 3);
#endif
    m = memlink_create("127.0.0.1", MEMLINK_READ_PORT, MEMLINK_WRITE_PORT, 30);
    if (NULL == m) {
        DERROR("memlink_create error\n");
        return -1;
    }
    
    int ret;
    char key[32];
    char *name = "test";
    int i;

    ret = memlink_cmd_create_table_list(m, name, 6, "4:3:1");
    if (ret != MEMLINK_OK) {
        DERROR("create table %s error: %d\n", name, ret);
        return -2;
    }
    /*
    for (i = 0; i < 3; i++) {
        sprintf(key, "test%d", i);
        ret = memlink_cmd_create_node(m, name, key);
        if (ret != MEMLINK_OK) {
            DERROR("create node %s.%s error: %d\n", name, key, ret);
            return -2;
        }
    }*/

    char val[64];
    char *maskstr = "8:3:1";
    int  insertnum = 200;
    
    for (i = 0; i < 3; i++) {
        int j = 0;
        sprintf(key, "test%d", i);
        for (j = 0; j < insertnum; j++) {
            sprintf(val, "%06d", j);
            ret = memlink_cmd_insert(m, name, key, val, strlen(val), maskstr, j); 
            if (ret != MEMLINK_OK) {
                DERROR("insert error, key:%s, val:%s, mask:%s, i:%d, ret:%d\n", key, val, maskstr, j, ret);
                return -3;
            }
        }
    }
    
    for (i = 0; i < 3; i++) {
        sprintf(key, "test%d", i);
        int j;
        for (j = 0; j < insertnum / 2; j++) {
            sprintf(val, "%06d", j);
            ret = memlink_cmd_del(m, name, key, val, strlen(val));
            if (ret != MEMLINK_OK) {
                DERROR("del error, key:%s, val:%s,i%d,ret:%d\n", key, val, i, ret);
                return -3;
            }
        }
    }
    
    MemLinkStat stat[3];
    for (i = 0; i  < 3; i++) {
        sprintf(key, "test%d", i);
        ret = memlink_cmd_stat(m, name, key, &stat[i]);
        if (ret != MEMLINK_OK) {
            DERROR("stat error, key:%s, ret:%d\n", key, ret);
            return -4;
        }
    }

    ret = memlink_cmd_clean_all(m);
    if (ret != MEMLINK_OK) {
        DERROR("clean error, key: %s, ret: %d\n", key, ret);
        return -5;
    }

    MemLinkStat stat1[3];
    for (i = 0; i < 3; i++) {
        sprintf(key, "test%d", i);
        ret = memlink_cmd_stat(m, name, key, &stat1[i]);
        if (ret != MEMLINK_OK) {
            DERROR("stat error, key: %s, ret: %d\n", key, ret);
            return -6;
        }
    }

    for (i = 0; i < 3; i++) {
        if (stat[i].data_used != stat1[i].data_used) {
            return -7;
        }
    }
    return 0;
}
