#include <string.h>

#include "base/confparse.h"
#include "myconfig.h"
#include "logfile.h"
#include "base/zzmalloc.h"

int main(int argc, char **argv)
{
#if DEBUG
    logfile_create("stdout", 4);
#endif

    MyConfig *conf = (MyConfig *)zz_malloc(sizeof(MyConfig));
    memset(conf, 0, sizeof(MyConfig));
    ConfPairs *cpair;

    ConfParser *cf_p = confparser_create("./test.conf");
    confparser_add_param(cf_p, &conf->block_data_count, "block_data_count", CONF_INT, BLOCK_DATA_COUNT_MAX, NULL);
    confparser_add_param(cf_p, &conf->block_data_count_items, "block_data_count_items", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->block_data_reduce, "block_data_reduce", CONF_FLOAT, 0, NULL);
    confparser_add_param(cf_p, &conf->dump_interval, "dump_interval", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->block_clean_cond, "block_clean_cond", CONF_FLOAT, 0, NULL);
    confparser_add_param(cf_p, &conf->block_clean_start, "block_clean_start", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->block_clean_num, "block_clean_num", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->read_port, "read_port", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->write_port, "write_port", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->sync_port, "sync_port", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->heartbeat_port, "heartbeat_port", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->datadir, "datadir", CONF_STRING, 0, NULL);
    confparser_add_param(cf_p, &conf->log_level, "log_level", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->log_name, "log_name", CONF_STRING, 0, NULL);
    confparser_add_param(cf_p, &conf->write_binlog, "write_binlog", CONF_BOOL, 0, NULL);
    confparser_add_param(cf_p, &conf->timeout, "timeout", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->heartbeat_timeout, "heartbeat_timeout", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->backup_timeout, "backup_timeout", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->thread_num, "thread_num", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->max_conn, "max_conn", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->max_read_conn, "max_read_conn", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->max_write_conn, "max_write_conn", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->max_sync_conn, "max_sync_conn", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->max_core, "max_core", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->max_mem, "max_mem", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->is_daemon, "is_daemon", CONF_BOOL, 0, NULL);
    confparser_add_param(cf_p, &conf->master_sync_host, "master_sync_host", CONF_STRING, 0, NULL);
    confparser_add_param(cf_p, &conf->master_sync_port, "master_sync_port", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->vote_host, "vote_host", CONF_STRING, 0, NULL);
    confparser_add_param(cf_p, &conf->vote_port, "vote_port", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &conf->sync_check_interval, "sync_check_interval", CONF_INT, 0, NULL);
    
    cpair = confpairs_create(2);
    confpairs_add(cpair, "master-slave", 1);
    confpairs_add(cpair, "master-backup", 2);

    confparser_add_param(cf_p, &conf->sync_mode, "sync_mode", CONF_ENUM, 0, cpair);
    confparser_add_param(cf_p, &conf->user, "user", CONF_STRING, 0, NULL);

    if (confparser_parse(cf_p) < 0) {
        DERROR("*** error\n");
        return -1;
    }

    confpairs_destroy(cpair);

    int i;
    for (i = 0; i < BLOCK_DATA_COUNT_MAX; i++) {
        if (conf->block_data_count[i] <= 0)
            break;
        DINFO("block_data_count[%d]: %d\n", i, conf->block_data_count[i]);
    }
    DINFO("block_data_count_items: %d\n", conf->block_data_count_items);
    DINFO("dump_interval: %d\n", conf->dump_interval);
    DINFO("block_data_reduce: %f\n", conf->block_data_reduce);
    DINFO("block_clean_cond: %f\n", conf->block_clean_cond);
    DINFO("block_clean_start: %d\n", conf->block_clean_start);
    DINFO("block_clean_num: %d\n", conf->block_clean_num);
    DINFO("read_port: %d\n", conf->read_port);
    DINFO("write_port: %d\n", conf->write_port);
    DINFO("sync_port: %d\n", conf->sync_port);
    DINFO("heartbeat_port: %d\n", conf->heartbeat_port);
    DINFO("datadir: %s\n", conf->datadir);
    DINFO("log_level: %d\n", conf->log_level);
    DINFO("log_name: %s\n", conf->log_name);
    DINFO("write_binlog: %d\n", conf->write_binlog);
    DINFO("timeout: %d\n", conf->timeout);
    DINFO("heartbeat_timeout: %d\n", conf->heartbeat_timeout);
    DINFO("backup_timeout: %d\n", conf->backup_timeout);
    DINFO("thread_num: %d\n", conf->thread_num);
    DINFO("max_conn: %d\n", conf->max_conn);
    DINFO("max_read_conn: %d\n", conf->max_read_conn);
    DINFO("max_write_conn: %d\n", conf->max_write_conn);
    DINFO("max_sync_conn: %d\n", conf->max_sync_conn);
    DINFO("max_core: %d\n", conf->max_core);
    DINFO("max_mem: %lld\n", conf->max_mem);
    DINFO("is_daemon: %d\n", conf->is_daemon);
    DINFO("master_sync_host: %s\n", conf->master_sync_host);
    DINFO("master_sync_port: %d\n", conf->master_sync_port);
    DINFO("vote_host: %s\n", conf->vote_host);
    DINFO("vote_port: %d\n", conf->vote_port);
    DINFO("sync_check_interval: %u\n", conf->sync_check_interval);
    DINFO("sync_mode: %d\n", conf->sync_mode);
    DINFO("user: %s\n", conf->user);

    zz_free(conf);

    return 0;
}
