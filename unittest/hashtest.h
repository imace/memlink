#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "zzmalloc.h"
#include "logfile.h"
#include "hashtable.h"
#include "mem.h"
#include "myconfig.h"
#include "common.h"
#include "runtime.h"
#include "datablock.h"
#include "syncbuffer.h"

#define MY_PRINT(format,args...)  \
{\
	printf("%s:%d @ ", __FILE__, __LINE__);\
	printf(format, args...);\
}

//#define DEBUG
int my_rand(int base);
Runtime* my_runtime_create_common(char *pgname);

int my_rand(int base)
{
	int i = -1;
	usleep(10);
	srand((unsigned)time(NULL)+rand());
	
	while (i < 0 )
	{
		i = rand()%base;
	}
	return i;
}

Runtime*
my_runtime_create_common(char *pgname)
{
    Runtime *rt = (Runtime*)zz_malloc(sizeof(Runtime));
    if (NULL == rt) {
        DERROR("malloc Runtime error!\n");
        MEMLINK_EXIT;
        return NULL; 
    }
    memset(rt, 0, sizeof(Runtime));
    g_runtime = rt;

    realpath(pgname, rt->home);
    char *last = strrchr(rt->home, '/');  
    if (last != NULL) {
        *last = 0;
    }
    DINFO("home: %s\n", rt->home);

    int ret;
    ret = pthread_mutex_init(&rt->mutex, NULL);
    if (ret != 0) {
        DERROR("pthread_mutex_init error: %s\n", strerror(errno));
        MEMLINK_EXIT;
        return NULL;
    }
    DINFO("mutex init ok!\n");

    rt->mpool = mempool_create();
    if (NULL == rt->mpool) {
        DERROR("mempool create error!\n");
        MEMLINK_EXIT;
        return NULL;
    }
    DINFO("mempool create ok!\n");

    rt->ht = hashtable_create();
    if (NULL == rt->ht) {
        DERROR("hashtable_create error!\n");
        MEMLINK_EXIT;
        return NULL;
    }
    DINFO("hashtable create ok!\n");

    rt->server = NULL;
    rt->synclog = NULL;

    return rt;
}

