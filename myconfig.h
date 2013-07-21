#ifndef MEMLINK_MYCONFIG_H
#define MEMLINK_MYCONFIG_H

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>

// TODO is there a pre-defined const for this?
#define IP_ADDR_MAX_LEN         16
#define BLOCK_DATA_COUNT_MAX    16

#define CONF_LOAD_ALL		1
#define CONF_LOAD_DYNAMIC	2

typedef struct _myconfig
{
    unsigned int block_data_count[BLOCK_DATA_COUNT_MAX];
    int          block_data_count_items;
	float		 block_data_reduce;
    unsigned int dump_interval;                       // in minutes
    float        block_clean_cond;
    int          block_clean_start;
    int          block_clean_num;
	char		 host[IP_ADDR_MAX_LEN];
    int          read_port;
    int          write_port;
    int          sync_port;
    int          heartbeat_port;
    char         datadir[PATH_MAX];
    int          log_level;
    char         log_name[PATH_MAX];
    char         log_error_name[PATH_MAX];
	unsigned int log_size;
	unsigned int log_time;
	int			 log_count;    
	int			 log_rotate_type;
	int			 write_binlog;
    int          timeout;
    int          heartbeat_timeout;
    int          backup_timeout;
    int          thread_num;
    int          max_conn;                            // max connection
    int          max_read_conn;
    int          max_write_conn;
	int		     max_sync_conn;
    int          max_core;                            // maximize core file limit
	long long	 max_mem;	// maximize memory used
    int          is_daemon;                           // is run with daemon
    char         role;                                // 1 means master; 0 means slave
    char         master_sync_host[IP_ADDR_MAX_LEN];
    int          master_sync_port;
    char         vote_host[IP_ADDR_MAX_LEN];
    int          vote_port;
    unsigned int sync_check_interval;                       // in seconds
	unsigned int sync_disk_interval;
	int			 sync_mode;
    int          dumpfile_num_max;
	char		 user[128];
}MyConfig;

extern MyConfig *g_cf;

MyConfig*   myconfig_create(char *filename);
int         myconfig_change();
int			myconfig_print(MyConfig *cf);

int			myconfig_parser_create(MyConfig *cf, char *filepath, int loadflag);

#endif
