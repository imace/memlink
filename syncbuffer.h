#ifndef MEMLINK_SYNCBUFFER_H
#define MEMLINK_SYNCBUFFER_H

#include <stdio.h>


//#define SYNCBUFFER_SIZE        10 * 1024 * 1024
#define SYNCBUFFER_SIZE        5 * 1024 * 1024

typedef struct _sync_buffer
{
    unsigned int index_size;
    unsigned int data_size;
    unsigned int index_pos;
    unsigned int data_pos;
    
    int start_logver;
    int last_logver;
    int start_logline;
    int last_logline;

    char *buffer;
}SyncBuffer;

typedef struct _sync_mem
{
    pthread_mutex_t   lock;

    SyncBuffer        *main;
    SyncBuffer        *auxiliary;

}SyncMem;

SyncMem *syncmem_create();
int     syncmem_destroy();
int     syncmem_print(SyncMem *sem);
int     syncmem_write(SyncMem *sem, char *data, int len, int logver, int logline);
int     syncmem_reset(SyncMem *smem, int logver, int logline, int bcount);
int     syncmem_read(SyncMem *sem, int logver, int logline, 
            int *last_logver, int *log_line, char *data, int len, char need_skip_one);

#endif
