#include <stdio.h>
#include <stdlib.h>
#include "serial.h"
#include "myconfig.h"
#include "logfile.h"

int config_print(MyConfig *cf)
{
    int i;
   
    printf("block_data_count:");
    for (i = 0; i < cf->block_data_count_items; i++) {
        printf("%d ", cf->block_data_count[i]);
    }
    printf("\n");
    printf("block_data_count_items: %d\n", cf->block_data_count_items);
    printf("block_data_reduce: %f\n", cf->block_data_reduce);
    printf("dump_interval: %u\n", cf->dump_interval);
    printf("block_clean_cond: %f\n", cf->block_clean_cond);
    printf("block_clean_start: %d\n", cf->block_clean_start);
    printf("block_clean_num: %d\n", cf->block_clean_num);
    printf("read_port: %d\n", cf->read_port);
    printf("write_port: %d\n", cf->write_port);
    printf("sync_port: %d\n", cf->sync_port);
    printf("datadir: %s\n", cf->datadir);
    printf("log_level: %d\n", cf->log_level);
    printf("log_name: %s\n", cf->log_name);
    printf("write_binlog: %d\n", cf->write_binlog);
    printf("timeout: %d\n", cf->timeout);
    printf("thread_num: %d\n", cf->thread_num);
    printf("max_conn: %d\n", cf->max_conn);
    printf("max_read_conn: %d\n", cf->max_read_conn);
    printf("max_write_conn: %d\n", cf->max_write_conn);
    printf("max_sync_conn: %d\n", cf->max_sync_conn);
    printf("max_core: %d\n", cf->max_core);
    printf("max_mem: %lld\n", cf->max_mem);
    printf("is_daemon: %d\n", cf->is_daemon);
    printf("role: %d\n", cf->role);
    printf("master_sync_host: %s\n", cf->master_sync_host);
    printf("master_sync_port: %d\n", cf->master_sync_port);
    printf("sync_check_interval: %u\n", cf->sync_check_interval);
    printf("user: %s\n", cf->user);

    return 0;
}

int main()
{
    MyConfig cf;
    
    memset(&cf, 0, sizeof(cf));

    logfile_create("stdout", 4); 

    int ret;
    char buf[10240] = {0};
    
    cf.block_data_count[0] = 1;
    cf.block_data_count[1] = 2;
    cf.block_data_count[2] = 5;
    cf.block_data_count[3] = 10;
    cf.block_data_count_items = 4;
    cf.block_data_reduce = 0.1;
    cf.dump_interval = 60;
    cf.read_port = 100;
    cf.write_port = 200;
    cf.sync_port = 300;
    cf.max_conn = 1000;
    cf.max_mem = 200000000;
    cf.sync_check_interval = 10;
    strcpy(cf.user, "zhaowei");

    config_print(&cf);

    ret = pack_config_struct(buf, &cf);
    DINFO("pack ret:%d\n", ret);

    MyConfig cf2;
    memset(&cf2, 0, sizeof(cf2));
    
    DINFO("====== unpack ======\n");
    ret = unpack_config_struct(buf, &cf2);
    DINFO("unpack ret:%d\n", ret);
    
    config_print(&cf2);

    return 0;
}

