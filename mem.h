#ifndef MEMLINK_MEM_H
#define MEMLINK_MEM_H

#include <stdio.h>

#define MEMLINK_MEM_NUM     100

#if defined(__APPLE__) || defined(__FreeBSD__)
#define fopen64 fopen
#endif

typedef struct _data_block
{
	unsigned short		data_count; // data count in one block
    unsigned short      visible_count; // visible item count
    unsigned short      tagdel_count;  // tag delete item count, invisible
    struct _data_block  *prev;
    struct _data_block  *next;
    char                data[0];
}DataBlock;

typedef struct _data_block_one
{
	unsigned short		data_count; // data count in one block
    unsigned short      visible_count; // visible item count
    unsigned short      tagdel_count;  // tag delete item count, invisible
    char                data[0];
}DataBlockOne;

typedef struct _mem_item
{
    int          memsize;
	unsigned int block_count;
    unsigned int total;
    DataBlock    *data;
}MemItem;

typedef struct _mempool
{
    MemItem     *freemem;
    int         size;  // freemem size
    int         used; // freemem used size
	int			blocks;
}MemPool;

//extern MemPool  *g_mpool;

MemPool*    mempool_create();
DataBlock*  mempool_get(MemPool *mp, int blocksize);
DataBlock*  mempool_get2(MemPool *mp, int blocks, int datalen);
int         mempool_put(MemPool *mp, DataBlock *dbk, int blocksize);
int         mempool_put2(MemPool *mp, DataBlock *dbk, int datalen);
int         mempool_expand(MemPool *mp);
void        mempool_free(MemPool *mp, int blocksize);
void        mempool_destroy(MemPool *mp);

#endif
