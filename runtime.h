#ifndef MEMLINK_RUNTIME_H
#define MEMLINK_RUNTIME_H

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <stdint.h>
#include "synclog.h"
#include "hashtable.h"
#include "mem.h"
#include "wthread.h"
#include "server.h"
#include "sslave.h"
#include "sthread.h"
#include "syncbuffer.h"
#include "vote.h"

typedef struct _runtime
{
    char            home[PATH_MAX]; // programe home dir
    char            conffile[PATH_MAX];
    pthread_mutex_t mutex; // write lock
    unsigned int    dumpver; // dump file version
    unsigned int    dumplogver; // log version in dump file
    unsigned int    dumplogpos; // log position in dump file
    unsigned int    logver;  // synclog version
    SyncLog         *synclog;  // current synclog
    MemPool         *mpool; 
    HashTable       *ht;
    SyncMem         *syncmem;
	volatile int	inclean;
    //char			cleankey[255];
    WThread         *wthread;
    MainServer      *server;
    SSlave          *slave; // sync slave
    SThread         *sthread; // sync thread
    unsigned int    conn_num; // current conn count
	time_t          last_dump;
	unsigned int    memlink_start;

	pthread_mutex_t	mutex_mem;
	long long		mem_used;
	unsigned int	mem_check_last;
    uint64_t voteid;
    //unsigned char   role;
    //Host			*hosts;
    unsigned int    servernums;
	struct timeval  sync_disk_last;
}Runtime;

extern Runtime  *g_runtime;

Runtime*    runtime_create_master(char *pgname, char *conffile);
Runtime*    runtime_create_slave(char *pgname, char *conffile);
void        runtime_destroy(Runtime *rt);
int         conn_check_max(Conn *conn);
int			mem_used_inc(long long size);	
int			mem_used_dec(long long size);



#endif
