/**
 * 内存池
 * @file mem.c
 * @author zhaowei
 * @ingroup memlink
 * @{
 */
#include <stdlib.h>
#include <string.h>
#include "zzmalloc.h"
#include "logfile.h"
#include "mem.h"
#include "common.h"
//MemPool *g_mpool;

MemPool*    
mempool_create()
{
    MemPool *mp;

    mp = (MemPool*)zz_malloc(sizeof(MemPool));
    if (NULL == mp) {
        DERROR("malloc MemPool error!\n");
        return NULL;
    }
    memset(mp, 0, sizeof(MemPool));

    mp->size = MEMLINK_MEM_NUM;
    mp->freemem = (MemItem*)zz_malloc(sizeof(MemItem) * mp->size);
    if (NULL == mp->freemem) {
        DERROR("malloc MemItem error!\n");
        zz_free(mp);
        return NULL;
    }
    memset(mp->freemem, 0, sizeof(MemItem) * mp->size);
   
    //g_mpool = mp;

    return mp;
}

DataBlock*
mempool_get(MemPool *mp, int blocksize)
{
    int i;
    DataBlock *dbk = NULL, *dbn;
    int find = 0;

    //DNOTE("used: %d, blocsize: %d\n", mp->used, blocksize);
    for (i = 0; i < mp->used; i++) {
        if (mp->freemem[i].memsize == blocksize) {
            dbk = mp->freemem[i].data;
            find = 1;
            break;
        }
    }

    if (NULL == dbk) {
        dbk = (DataBlock*)zz_malloc(blocksize);
        if (NULL == dbk) {
            DERROR("malloc DataBlock error!\n");
            MEMLINK_EXIT;
            return NULL;
        }
        memset(dbk, 0x0, blocksize);
        if (find != 1) {
            if (i < mp->size) {
                mp->freemem[i].memsize = blocksize;
                mp->used += 1;
            } else {
                if (mempool_expand(mp) == -1)
                    return NULL; 

                mp->freemem[mp->used].memsize = blocksize;
                mp->used += 1;
                i = mp->used;
            }
        }
        mp->freemem[i].total += 1;
        return dbk;
    }

    mp->blocks--;
    mp->freemem[i].block_count -= 1;
    dbn = dbk->next;
    mp->freemem[i].data = dbn; 
    //memset(dbk, 0, sizeof(DataBlock) + g_cf->block_data_count * blocksize);
    memset(dbk, 0, blocksize);
    //mp->freemem[i].ht_use = mp->freemem[i].ht_use + 1;
    
    return dbk;
}

DataBlock*
mempool_get2(MemPool *mp, int blocks, int datalen)
{
    //DINFO("get size:%d, blocks:%d, datalen:%d\n", sizeof(DataBlock) + blocks * datalen, blocks, datalen);
    DataBlock *bk  = mempool_get(mp, sizeof(DataBlock) + blocks * datalen);
    bk->data_count = blocks;
    return bk;
}

int
mempool_put(MemPool *mp, DataBlock *dbk, int blocksize)
{
    int i;

    zz_check(dbk);

    dbk->data_count = 0;
    dbk->prev = NULL;
    for (i = 0; i < mp->used; i++) {
        if (mp->freemem[i].memsize == blocksize) {
            dbk->next = mp->freemem[i].data;
            mp->freemem[i].data = dbk; 
            mp->blocks++;
            mp->freemem[i].block_count += 1;
            return 0;
        }
    }
    dbk->next = NULL;
    if (i < mp->size) {
        mp->freemem[i].data = dbk;
        mp->freemem[i].memsize = blocksize;
        mp->used += 1;
    }else{
        if (mempool_expand(mp) == -1)
            return -2;
        
        mp->freemem[mp->used].memsize = blocksize;
        mp->freemem[mp->used].data = dbk;

        mp->used += 1;
    
        zz_check(dbk);
    }

    mp->blocks++;
    mp->freemem[i].block_count += 1;
    return 0;
}

int
mempool_put2(MemPool *mp, DataBlock *dbk, int datalen)
{
    return mempool_put(mp, dbk, sizeof(DataBlock) + dbk->data_count * datalen);
}
 
int
mempool_expand(MemPool *mp)
{
    int newnum = mp->size * 2;           
    MemItem  *newitems = (MemItem*)zz_malloc(sizeof(MemItem) * newnum);
    if (NULL == newitems) {
        DERROR("malloc error!\n");
        MEMLINK_EXIT;
        return -1;
    }
    
    memcpy(newitems, mp->freemem, sizeof(MemItem) * mp->used);

    zz_free(mp->freemem);
    mp->freemem = newitems;
    mp->size = newnum;

    return 0;
}

void 
mempool_free(MemPool *mp, int blocksize)
{
    int i;

    for (i = 0; i < mp->used; i++) {
        if (mp->freemem[i].memsize == blocksize) {
            DataBlock *dbk = mp->freemem[i].data;
            DataBlock *tmp; 

            while (dbk) {
                tmp = dbk;
                dbk = dbk->next;

                zz_free(tmp);
                mp->blocks--;
            }
            mp->freemem[i].data = NULL;
        }
    }
}

void
mempool_destroy(MemPool *mp)
{
    if (NULL == mp)
        return;
   
    int i;

    for (i = 0; i < mp->used; i++) {
        DataBlock *dbk = mp->freemem[i].data;
        DataBlock *tmp; 

        while (dbk) {
            tmp = dbk;
            dbk = dbk->next;

            zz_free(tmp);
        }

        mp->freemem[i].data = NULL;
    }

    zz_free(mp->freemem);
    zz_free(mp);
}

/**
 * @}
 */

