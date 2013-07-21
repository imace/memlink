#ifndef MEMLINK_SSLAVE_H
#define MEMLINK_SSLAVE_H

#include <stdio.h>
#include "synclog.h"

#define SLAVE_STATUS_INIT	0
#define SLAVE_STATUS_SYNC	1
#define SLAVE_STATUS_DUMP	2
#define SLAVE_STATUS_RECV	3

typedef struct _sslave 
{
    int sock;
	int timeout;

    volatile pthread_t threadid;
	//int status; // 0.init 1. sync 2.dump 3.recv new log

	//int binlog_ver;
	//int binlog_index;
	//int binlog_min_ver;  the same as g_runtime->dumplogver

	//SyncLog		*binlog;

    //unsigned int logver; // last logver
    //unsigned int logline; // last logline

	//unsigned int dump_logver; // logver in master dumpfile
    //long long    dumpsize; // size in master dumpfile
    //long long    dumpfile_size; // master dumpfile size

    //int			 trycount; // count of get last sync position
    volatile int is_getdump;
	volatile int isrunning;
    //volatile int is_backup_do;
} SSlave;

SSlave* sslave_create();
void	sslave_go(SSlave *slave);
void    sslave_stop(SSlave *slave);
void	sslave_destroy(SSlave *slave);
void	sslave_close(SSlave *slave);
void    sslave_thread(SSlave *slave);

#endif
