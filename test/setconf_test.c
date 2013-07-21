#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include "memlink_client.h"
#include "test.h"
#include "logfile.h"

int test()
{
    MemLink *m;
    int err  = 0;
    MyConfig config, newconfig;

#ifdef DEBUG
    logfile_create("stdout", 4);
#endif
    m = memlink_create("127.0.0.1", MEMLINK_READ_PORT, MEMLINK_WRITE_PORT, 30);
    if (NULL == m) {
        DERROR("memlink_create error!\n");
        return -1;
    }
    
    int ret;
    char value[25];
    ret = memlink_cmd_get_config_info(m, &config);
    if (ret != MEMLINK_OK) {
        DERROR("memlink_cmd_get_config_info error\n");
        return ++err;
    }
    DINFO("config.master_sync_host: %s\n", config.master_sync_host);
    sprintf(value, "%f", config.block_data_reduce + 0.1);
    ret = memlink_cmd_set_config_info(m, "block_data_reduce", value);
    if (ret != MEMLINK_OK) {
        DERROR("memlink_cmd_set_config_info: %d\n", ret);
        return ++err;
    }
    sprintf(value, "%f", config.block_clean_cond + 0.1);
    ret = memlink_cmd_set_config_info(m, "block_clean_cond", value); 
    if (ret != MEMLINK_OK) {
        DERROR("memlink_cmd_set_config_info: %d\n", ret);
        return ++err;
    }
    sprintf(value, "%d", config.block_clean_start + 1);
    ret = memlink_cmd_set_config_info(m, "block_clean_start", value);
    if (ret != MEMLINK_OK) {
        DERROR("memlink_cmd_set_config_info: %d\n", ret);
        return ++err;
    }
    sprintf(value, "%s", "22.22.22.22");
    ret = memlink_cmd_set_config_info(m, "master_sync_host", value);
    if (ret != MEMLINK_OK) {
        DERROR("memlink_cmd_set_config_info: %d\n", ret);
        return ++err;
    }
    ret = memlink_cmd_get_config_info(m, &newconfig);
    if (ret != MEMLINK_OK) {
        DERROR("memlink_cmd_get_config_info: %d\n", ret);
        return ++err;
    }
    if (fabs(newconfig.block_data_reduce - (config.block_data_reduce + 0.1)) > FLT_EPSILON) {
        DERROR("memlink_cmd_get_config_info: %d\n", ret);
        return ++err;
    }
    
    if (abs(newconfig.block_clean_cond - (config.block_clean_cond + 0.1)) > FLT_EPSILON) {
        DERROR("memlink_cmd_get_config_info: %d\n", ret);
        return ++err;
    }
    DINFO("newconfig.master_sync_host: %s\n", newconfig.master_sync_host);
    if (strcmp(newconfig.master_sync_host, "22.22.22.22") != 0) {
        DERROR("memlink_cmd_get_config_info: %d\n", ret);
        return ++err;
    }

    DINFO("err: %d\n", err);    
    return err;
}

int main()
{
    int ret = 0;

    ret = test();
    
    DINFO("ret: %d\n", ret);
    return ret;
}
