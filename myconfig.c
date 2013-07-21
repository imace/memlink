/**
 * 配置和初始化
 * @file myconfig.c
 * @author zhaowei
 * @ingroup memlink
 * @{
 */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "base/logfile.h"
#include "base/confparse.h"
#include "mem.h"
#include "myconfig.h"
#include "base/zzmalloc.h"
#include "synclog.h"
#include "server.h"
#include "dumpfile.h"
#include "wthread.h"
#include "common.h"
#include "base/utils.h"
#include "sslave.h"
#include "runtime.h"
#include "conn.h"

MyConfig *g_cf;

static int
conf_parse_sync_ipport(void *f, char *value, int i)
{
    MyConfig *cf = (MyConfig*)f;
    
    //DERROR("value:%s\n", value);
    char *sp = strchr(value, ':'); 
    if (NULL == sp) {
        //DERROR("not found : in addr.\n");
        return FALSE;
    }
    *sp = '\0'; 
    char *v1 = value;
    char *v2 = sp + 1;

    while (isblank(*v1)) v1++;
    while (isblank(*v2)) v2++;
    
    strncpy(cf->master_sync_host, v1, 15);
    cf->master_sync_port = atoi(v2);

    return TRUE; 
}

static int
conf_parse_vote_ipport(void *f, char *value, int i)
{
    MyConfig *cf = (MyConfig*)f;

    char *sp = strchr(value, ':'); 
    if (NULL == sp) {
        DERROR("must set as xx.xx.xx.xx:xxxx\n");
        return FALSE;
    }
    *sp = '\0';
    char *v1 = value;
    char *v2 = sp + 1;

    while (isblank(*v1)) v1++;
    while (isblank(*v2)) v2++;

    strncpy(cf->vote_host, v1, 15);
    cf->vote_port = atoi(v2);

    return TRUE; 
}

int
myconfig_print(MyConfig *conf)
{
    DINFO("====== config ======\n");
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
    DINFO("host: %s\n", conf->host);
    DINFO("read_port: %d\n", conf->read_port);
    DINFO("write_port: %d\n", conf->write_port);
    DINFO("sync_port: %d\n", conf->sync_port);
    DINFO("heartbeat_port: %d\n", conf->heartbeat_port);
    DINFO("datadir: %s\n", conf->datadir);
    DINFO("log_level: %d\n", conf->log_level);
    DINFO("log_rotate_type: %d\n", conf->log_rotate_type);
    DINFO("log_name: %s\n", conf->log_name);
    DINFO("log_size: %d\n", conf->log_size);
    DINFO("log_time: %d\n", conf->log_time);
    DINFO("log_count: %d\n", conf->log_count);
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
    DINFO("daemon: %d\n", conf->is_daemon);
    DINFO("sync_master: %s:%d\n", conf->master_sync_host, conf->master_sync_port);
    DINFO("vote_server: %s:%d\n", conf->vote_host, conf->vote_port);
    DINFO("sync_check_interval: %u\n", conf->sync_check_interval);
    DINFO("sync_mode: %d\n", conf->sync_mode);
    DINFO("user: %s\n", conf->user);
    DINFO("dumpfile_num_max: %d\n", conf->dumpfile_num_max);

    DINFO("====== end ======\n");

    return 0;
}

int
myconfig_parser_load(MyConfig *cf, char *filepath, int loadflag)
{
    ConfParser * cp = confparser_create(filepath);

    ConfPairs   *levels = confpairs_create(5);
    confpairs_add(levels, "error", LOG_ERROR);
    confpairs_add(levels, "warn", LOG_WARN);
    confpairs_add(levels, "note", LOG_NOTE);
    confpairs_add(levels, "notice", LOG_NOTICE);
    confpairs_add(levels, "info", LOG_INFO);

    ConfPairs   *rotatetypes = confpairs_create(3);
    confpairs_add(rotatetypes, "no", LOG_ROTATE_NO);
    confpairs_add(rotatetypes, "size", LOG_ROTATE_SIZE);
    confpairs_add(rotatetypes, "time", LOG_ROTATE_TIME);

    ConfPairs   *roles = confpairs_create(3);
    confpairs_add(roles, "master", ROLE_MASTER);
    confpairs_add(roles, "backup", ROLE_BACKUP);
    confpairs_add(roles, "slave", ROLE_SLAVE);

    ConfPairs   *syncmods = confpairs_create(2);
    confpairs_add(syncmods, "master-slave", MODE_MASTER_SLAVE);
    confpairs_add(syncmods, "master-backup", MODE_MASTER_BACKUP);

    if (loadflag == CONF_LOAD_ALL) {
        confparser_add_param(cp, cf->block_data_count, "block_data_count", CONF_INT, 
                    BLOCK_DATA_COUNT_MAX, NULL);
        confparser_add_param(cp, &cf->block_data_reduce, "block_data_reduce", CONF_FLOAT, 0, NULL);
        confparser_add_param(cp, &cf->dump_interval, "dump_interval", CONF_INT, 0, NULL);
        confparser_add_param(cp, &cf->sync_check_interval, "sync_check_interval", CONF_INT, 0, NULL);
        confparser_add_param(cp, &cf->block_clean_cond, "block_clean_cond", CONF_FLOAT, 0, NULL);
        confparser_add_param(cp, &cf->block_clean_start, "block_clean_start", CONF_INT, 0, NULL);
        confparser_add_param(cp, &cf->block_clean_num, "block_clean_num", CONF_INT, 0, NULL);
        confparser_add_param(cp, cf->host, "host", CONF_STRING, 0, NULL);
        confparser_add_param(cp, &cf->read_port, "read_port", CONF_INT, 0, NULL);
        confparser_add_param(cp, &cf->write_port, "write_port", CONF_INT, 0, NULL);
        confparser_add_param(cp, &cf->sync_port, "sync_port", CONF_INT, 0, NULL);
        confparser_add_param(cp, cf->datadir, "data_dir", CONF_STRING, 0, NULL);
        confparser_add_param(cp, &cf->log_level, "log_level", CONF_ENUM, 0, levels);
        confparser_add_param(cp, cf->log_name, "log_name", CONF_STRING, 0, NULL);
        confparser_add_param(cp, cf->log_error_name, "log_error_name", CONF_STRING, 0, NULL);
        confparser_add_param(cp, &cf->log_size, "log_size", CONF_INT, 0, NULL);
        confparser_add_param(cp, &cf->log_time, "log_time", CONF_INT, 0, NULL);
        confparser_add_param(cp, &cf->log_count, "log_count", CONF_INT, 0, NULL);
        confparser_add_param(cp, &cf->log_rotate_type, "log_rotate_type", CONF_ENUM, 0, rotatetypes);
        confparser_add_param(cp, &cf->timeout, "timeout", CONF_INT, 0, NULL);
        confparser_add_param(cp, &cf->thread_num, "thread_num", CONF_INT, 0, NULL);
        confparser_add_param(cp, &cf->write_binlog, "write_binlog", CONF_BOOL, 0, NULL);
        confparser_add_param(cp, &cf->max_conn, "max_conn", CONF_INT, 0, NULL);
        confparser_add_param(cp, &cf->max_read_conn, "max_read_conn", CONF_INT, 0, NULL);
        confparser_add_param(cp, &cf->max_write_conn, "max_write_conn", CONF_INT, 0, NULL);
        confparser_add_param(cp, &cf->max_sync_conn, "max_sync_conn", CONF_INT, 0, NULL);
        confparser_add_param(cp, &cf->max_core, "max_core", CONF_INT, 0, NULL);
        confparser_add_param(cp, &cf->max_mem, "max_mem", CONF_INT, 0, NULL);
        confparser_add_param(cp, &cf->is_daemon, "daemon", CONF_BOOL, 0, NULL);
        confparser_add_param(cp, &cf->role, "role", CONF_ENUM, 0, roles);
        confparser_add_param(cp, cf, "sync_master", CONF_USER, 0, conf_parse_sync_ipport);
        confparser_add_param(cp, &cf->sync_check_interval, "sync_check_interval", CONF_INT, 0, NULL);
        confparser_add_param(cp, &cf->sync_disk_interval, "sync_disk_interval", CONF_INT, 0, NULL);
        confparser_add_param(cp, &cf->sync_mode, "sync_mode", CONF_ENUM, 0, syncmods);
        confparser_add_param(cp, cf->user, "user", CONF_STRING, 0, NULL);
        confparser_add_param(cp, &cf->heartbeat_timeout, "heartbeat_timeout", CONF_INT, 0, NULL);
        confparser_add_param(cp, cf, "vote_server", CONF_USER, 0, conf_parse_vote_ipport);
        confparser_add_param(cp, &cf->dumpfile_num_max, "dumpfile_num_max", CONF_INT, 0, NULL);

    }else if (loadflag == CONF_LOAD_DYNAMIC) {

    }
    int retcode = 0;
    if (confparser_parse(cp) < 0) {
        retcode = -1;
        goto myconfig_parse_load_over;
    }

    int i;
    for (i = 0; i < BLOCK_DATA_COUNT_MAX; i++) {
        if (cf->block_data_count[i] == 0) {
            break;
        }
    }
    cf->block_data_count_items = i;
    qsort(cf->block_data_count, cf->block_data_count_items, sizeof(int), compare_int);

    // minute to second
    cf->dump_interval *= 60;
    // m to byte
    cf->log_size *= 1048576;
    // minute to second
    cf->log_time *= 60;
    // m to byte
    cf->max_mem  *= 1048576;

myconfig_parse_load_over:
    confpairs_destroy(levels);
    confpairs_destroy(rotatetypes);
    confpairs_destroy(roles);
    confpairs_destroy(syncmods);
    confparser_destroy(cp);

    return retcode;
}

MyConfig*
myconfig_create(char *filename)
{
    MyConfig *mcf;

    mcf = (MyConfig*)zz_malloc(sizeof(MyConfig));
    if (NULL == mcf) {
        DERROR("malloc MyConfig error!\n");
        return NULL;
    }
    memset(mcf, 0, sizeof(MyConfig));

    // set default value
    /*mcf->block_data_count[0]    = 1;
    mcf->block_data_count[1]    = 5;
    mcf->block_data_count[2]    = 10;
    mcf->block_data_count[3]    = 20;
    mcf->block_data_count[4]    = 50;
    mcf->block_data_count_items = 5;*/
    //mcf->block_data_count_items = 5;
    mcf->block_clean_cond  = 0.5;
    mcf->block_clean_start = 3;
    mcf->block_clean_num   = 100;
    mcf->dump_interval = 60 * 60; // 1 hour
    mcf->sync_check_interval = 60;
    mcf->read_port  = 11001;
    mcf->write_port = 11002;
    mcf->sync_port  = 11003;
    mcf->log_level  = 3; 
    mcf->log_size   = 200 * 1024 * 1024;  // 200M
    mcf->log_time   = 1440 * 60; // 24h
    mcf->log_count  = 10;
    mcf->timeout    = 30;
    mcf->max_conn   = 1000;
    mcf->max_mem    = 0;
    mcf->sync_mode  = MODE_MASTER_SLAVE;
    mcf->heartbeat_timeout = 5;
    mcf->dumpfile_num_max = 20;

    strcpy(mcf->host, "0.0.0.0");

    snprintf(mcf->datadir, PATH_MAX, "data");


    if (myconfig_parser_load(mcf, filename, CONF_LOAD_ALL) != 0) {
        DERROR("parse config %s error!\n", filename);
        MEMLINK_EXIT;
    }
    
    //FILE    *fp;
    //char    filepath[PATH_MAX];
    // FIXME: must absolute path
    //snprintf(filepath, PATH_MAX, "etc/%s", filename);

    /*
    int lineno = 0;
    fp = fopen(filename, "r");
    if (NULL == fp) {
        DERROR("open config file error: %s\n", filename);
        MEMLINK_EXIT;
    }

    char buffer[2048];
    while (1) {
        if (fgets(buffer, 2048, fp) == NULL) {
            //DINFO("config file read complete!\n");
            break;
        }
        lineno ++;
        //DINFO("buffer: %s\n", buffer);
        if (buffer[0] == '#' || buffer[0] == '\r' || buffer[0] == '\n') { // skip comment
            continue;
        }
        char *sp = strchr(buffer, '=');
        if (sp == NULL) {
            DERROR("config file error at line %d: %s\n", lineno, buffer);
            MEMLINK_EXIT;
        }

        char *end = sp - 1;
        while (end > buffer && isblank(*end)) {
            end--;
        }
        *(end + 1) = 0;

        char *start = sp + 1;
        while (isblank(*start)) {
            start++;
        }
        
        end = start;
        while (*end != 0) {
            if (*end == '\r' || *end == '\n') {
                end -= 1;
                while (end > start && isblank(*end)) {
                    end--;
                }
                *(end + 1) = 0;
                break;
            }
            end += 1;
        }
        
        if (strcmp(buffer, "block_data_count") == 0) {
            char *ptr = start;
            int  i = 0;
            while (start) {
                //DINFO("start:%s\n", start);
                while (isblank(*start)) {
                    start++;
                }
                ptr = strchr(start, ',');
                if (ptr) {
                    *ptr = 0;
                    ptr++;
                }
                
                mcf->block_data_count[i] = atoi(start);
                start = ptr;
                //DINFO("i: %d, %d\n", i, mcf->block_data_count[i]);
                i++;
                if (i >= BLOCK_DATA_COUNT_MAX) {
                    DERROR("config error: block_data_count have too many items.\n");
                    MEMLINK_EXIT;
                }
            }
            mcf->block_data_count_items = i;
            qsort(mcf->block_data_count, i, sizeof(int), compare_int);

            //for (i = 0; i < BLOCK_DATA_COUNT_MAX; i++) {
            //    DINFO("block_data_count: %d, %d\n", i, mcf->block_data_count[i]);
            //}
        }else if (strcmp(buffer, "block_data_reduce") == 0) {
            mcf->block_data_reduce = atof(start);
        }else if (strcmp(buffer, "dump_interval") == 0) {
            mcf->dump_interval = atoi(start);
        }else if (strcmp(buffer, "sync_check_interval") == 0) {
            mcf->sync_check_interval = atoi(start); 
        }else if (strcmp(buffer, "block_clean_cond") == 0) {
            mcf->block_clean_cond = atof(start);
        }else if (strcmp(buffer, "block_clean_start") == 0) {
            mcf->block_clean_start = atoi(start);
        }else if (strcmp(buffer, "block_clean_num") == 0) {
            mcf->block_clean_num = atoi(start);
        }else if (strcmp(buffer, "host") == 0) {
            snprintf(mcf->host, IP_ADDR_MAX_LEN, "%s", start);
        }else if (strcmp(buffer, "read_port") == 0) {
            mcf->read_port = atoi(start);
        }else if (strcmp(buffer, "write_port") == 0) {
            mcf->write_port = atoi(start);
        }else if (strcmp(buffer, "sync_port") == 0) {
            mcf->sync_port = atoi(start);
        }else if (strcmp(buffer, "data_dir") == 0) {
            snprintf(mcf->datadir, PATH_MAX, "%s", start);
        }else if (strcmp(buffer, "log_level") == 0) {
            mcf->log_level = atoi(start);
        }else if (strcmp(buffer, "log_name") == 0) {
            snprintf(mcf->log_name, PATH_MAX, "%s", start);    
        }else if (strcmp(buffer, "log_error_name") == 0) {
            snprintf(mcf->log_error_name, PATH_MAX, "%s", start);    
        }else if (strcmp(buffer, "log_size") == 0) {
            mcf->log_size = atoi(start) * 1024 * 1024;
        }else if (strcmp(buffer, "log_time") == 0) {
            mcf->log_time = atoi(start) * 60;
        }else if (strcmp(buffer, "log_count") == 0) {
            mcf->log_count = atoi(start);
        }else if (strcmp(buffer, "log_rotate_type") == 0) {
            if (strcmp(start, "no") == 0) {
                mcf->log_rotate_type = LOG_ROTATE_NO;
            }else if (strcmp(start, "size") == 0) {
                mcf->log_rotate_type = LOG_ROTATE_SIZE;
            }else if (strcmp(start, "time") == 0) {
                mcf->log_rotate_type = LOG_ROTATE_TIME;
            }else{
                DERROR("config log_rotate_type must be: no/size/time\n");
                MEMLINK_EXIT;
            }
            mcf->timeout = atoi(start);
        }else if (strcmp(buffer, "timeout") == 0) {
            mcf->timeout = atoi(start);
        }else if (strcmp(buffer, "thread_num") == 0) {
            int tn = atoi(start);
            mcf->thread_num = tn > MEMLINK_MAX_THREADS ? MEMLINK_MAX_THREADS : tn;
        }else if (strcmp(buffer, "write_binlog") == 0) {
            if (strcmp(start, "yes") == 0) {
                mcf->write_binlog = 1;
            }else if (strcmp(start, "no") == 0){
                mcf->write_binlog = 0;
            }else{
                DERROR("write_binlog must set to yes/no !\n");
                MEMLINK_EXIT;
            }
        }else if (strcmp(buffer, "max_conn") == 0) {
            mcf->max_conn = atoi(start);
            if (mcf->max_conn < 0) {
                DERROR("max_conn must not smaller than 0.\n");
                MEMLINK_EXIT;
            }
        }else if (strcmp(buffer, "max_read_conn") == 0) {
            mcf->max_read_conn = atoi(start);
            if (mcf->max_read_conn < 0) {
                DERROR("max_read_conn must >= 0 and <= max_conn\n");
                MEMLINK_EXIT;
            }
        }else if (strcmp(buffer, "max_write_conn") == 0) {
            mcf->max_write_conn = atoi(start);
            if (mcf->max_write_conn < 0) {
                DERROR("max_write_conn must >= 0 and <= max_conn\n");
                MEMLINK_EXIT;
            }
        }else if (strcmp(buffer, "max_sync_conn") == 0) {
            mcf->max_sync_conn = atoi(start);
            if (mcf->max_sync_conn < 0) {
                DERROR("max_sync_conn must >= 0 and <= max_conn\n");
                MEMLINK_EXIT;
            }
        }else if (strcmp(buffer, "max_core") == 0) {
            mcf->max_core = atoi(start);
        }else if (strcmp(buffer, "max_mem") == 0) {
            mcf->max_mem = atoi(start) * 1048576;
        }else if (strcmp(buffer, "is_daemon") == 0) {
            if (strcmp(start, "yes") == 0) {
                mcf->is_daemon = 1;
            }else if (strcmp(start, "no") == 0){
                mcf->is_daemon = 0;
            }else{
                DERROR("is_daemon must set to yes/no !\n");
                MEMLINK_EXIT;
            }
        }else if (strcmp(buffer, "role") == 0) {
            if (strcmp(start, "master") == 0) {
                mcf->role = ROLE_MASTER;
            }else if (strcmp(start, "backup") == 0) {
                mcf->role = ROLE_BACKUP;
            }else if (strcmp(start, "slave") == 0) {
                mcf->role = ROLE_SLAVE;
            }else{
                DERROR("role must set to master/backup/slave!\n");
                MEMLINK_EXIT;
            }
        }else if (strcmp(buffer, "master_sync_host") == 0) {
            snprintf(mcf->master_sync_host, IP_ADDR_MAX_LEN, "%s", start);
        }else if (strcmp(buffer, "master_sync_port") == 0) {
            mcf->master_sync_port = atoi(start);
        }else if (strcmp(buffer, "sync_check_interval") == 0) {
            mcf->sync_check_interval = atoi(start);
        }else if (strcmp(buffer, "sync_disk_interval") == 0) {
            mcf->sync_disk_interval = atoi(start);
        }else if (strcmp(buffer, "sync_mode") == 0) {
            if (strcmp(start, "master-slave") == 0) {
                mcf->sync_mode = MODE_MASTER_SLAVE;
            }else if (strcmp(start, "master-backup") == 0) {
                mcf->sync_mode = MODE_MASTER_BACKUP;
            }else{
                DERROR("sync_mode must set to master-slave/master-backup\n");
                MEMLINK_EXIT;
            }
        }else if (strcmp(buffer, "user") == 0) {
            snprintf(mcf->user, 128, "%s", start);
        }else if (strcmp(buffer, "heartbeat_timeout") == 0) {
            mcf->heartbeat_timeout = atoi(start);
        }else if (strcmp(buffer, "vote_server") == 0) {
            char *colon = strchr(start, ':');
            if (NULL == colon) {
                DERROR("vote_server must set as: xx.xx.xx.xx:xxxx\n");
                MEMLINK_EXIT;
            }
            *colon = 0;
            char *hport = colon + 1;
            snprintf(mcf->vote_host, IP_ADDR_MAX_LEN, "%s", start);
            mcf->vote_port = atoi(hport);
        } else if (strcmp(buffer, "dumpfile_num_max") == 0) {
            mcf->dumpfile_num_max = atoi(start);
        }

    }

    fclose(fp);
    */

    // check config 
    //myconfig_print(mcf);

    if (mcf->max_conn <= 0 || mcf->max_conn < mcf->max_read_conn || 
        mcf->max_conn < mcf->max_write_conn || mcf->max_conn < mcf->max_sync_conn) {
        DERROR("config max_conn must >= 0 and >= max_read_conn, >= max_write_conn, >= max_sync_conn.\n");
        MEMLINK_EXIT;
    }

    if (mcf->max_read_conn == 0) {
        mcf->max_read_conn = mcf->max_conn;
    }
    if (mcf->max_write_conn == 0) {
        mcf->max_write_conn = mcf->max_conn;
    }
    if (mcf->max_sync_conn == 0) {
        mcf->max_sync_conn = mcf->max_conn;
    }

    g_cf = mcf;
    //DINFO("create MyConfig ok!\n");

    return mcf;
}

int
myconfig_change()
{
    char *conffile = g_runtime->conffile;
    char conffile_tmp[PATH_MAX];

    DINFO("change config file...\n");

    snprintf(conffile_tmp, PATH_MAX, "%s.tmp", conffile);

    FILE *fp = fopen64(conffile_tmp, "wb");
    char line[512] = {0};

    snprintf(line, 512, "block_data_count = %d,%d,%d,%d\n", g_cf->block_data_count[0],
        g_cf->block_data_count[1], g_cf->block_data_count[2], g_cf->block_data_count[3]);
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "block_data_reduce = %0.1f\n", g_cf->block_data_reduce);
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "dump_interval = %d\n", g_cf->dump_interval);
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "block_clean_cond = %0.1f\n", g_cf->block_clean_cond);
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "block_clean_start = %d\n", g_cf->block_clean_start);
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "block_clean_num = %d\n", g_cf->block_clean_num);
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "read_port = %d\n", g_cf->read_port);
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "write_port = %d\n", g_cf->write_port);
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "sync_port = %d\n", g_cf->sync_port);
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "data_dir = %s\n", g_cf->datadir);
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "log_level = %d\n", g_cf->log_level);
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "log_name = %s\n", g_cf->log_name);
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "timeout = %d\n", g_cf->timeout);
    ffwrite(line, strlen(line), 1, fp);
    
    snprintf(line, 512, "thread_num = %d\n", g_cf->thread_num);
    ffwrite(line, strlen(line), 1, fp);
    
    if (g_cf->write_binlog == 1)
        snprintf(line, 512, "write_binlog = %s\n", "yes");
    else
        snprintf(line, 512, "write_binlog = %s\n", "no");
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "max_conn = %d\n", g_cf->max_conn);
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "max_read_conn = %d\n", g_cf->max_read_conn);
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "max_write_conn = %d\n", g_cf->max_write_conn);
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "max_core = %d\n", g_cf->max_core);
    ffwrite(line, strlen(line), 1, fp);

    if (g_cf->is_daemon == 1)
        snprintf(line, 512, "is_daemon = %s\n", "yes");
    else
        snprintf(line, 512, "is_daemon = %s\n", "no");
    ffwrite(line, strlen(line), 1, fp);
    
    snprintf(line, 512, "%s\n", "# master/backup/slave");
    ffwrite(line, strlen(line), 1, fp);

    if (g_cf->role == ROLE_MASTER)
        snprintf(line, 512, "role = %s\n", "master");
    else if (g_cf->role == ROLE_BACKUP)
        snprintf(line, 512, "role = %s\n", "backup");
    else if (g_cf->role == ROLE_SLAVE)
        snprintf(line, 512, "role = %s\n", "slave");
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "master_sync_host = %s\n", g_cf->master_sync_host);
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "master_sync_port = %d\n", g_cf->master_sync_port);
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "sync_check_interval = %d\n", g_cf->sync_check_interval);
    ffwrite(line, strlen(line), 1, fp);

    snprintf(line, 512, "user = %s\n", g_cf->user);
    ffwrite(line, strlen(line), 1, fp);
    
    int ret;
    ret = rename(conffile_tmp, conffile);

    if (ret < 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("conffile rename error %s\n",  errbuf);
    }
    fclose(fp);
    return 1;
}


/**
 * @}
 */
