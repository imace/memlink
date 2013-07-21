#include "hashtest.h"

int main(int argc, char **argv)
{
#ifdef DEBUG
    logfile_create("test.log", 3);
#endif
    char *conffile = "memlink.conf";
    MemPool *mp;
    MemItem *item_100;
    MemItem *item_10;

    myconfig_create(conffile);
    my_runtime_create_common("memlink");
    mp = g_runtime->mpool;

    item_100 = (MemItem *)zz_malloc(sizeof(MemItem));
    memset(item_100, 0x0, sizeof(MemItem));

    item_10 = (MemItem *)zz_malloc(sizeof(MemItem));
    memset(item_10, 0x0, sizeof(MemItem));

    DataBlock *dbk;
    int i;
    for (i = 0; i < 100; i++) {
        dbk = mempool_get(mp, 100 + sizeof(DataBlock));
        dbk->next = item_100->data;
        item_100->data = dbk;
    }

    for (i = 0; i < mp->used; i++) {
        if (mp->freemem[i].memsize == 100 + sizeof(DataBlock)) {
            break;
        }
    }
    if (i == mp->used) {
        DERROR("i == mp->used\n"); 
        return -1;
    }
    if (mp->freemem[i].total != 100) {
        DERROR("mp->freemem[i].total != 100\n");
        return -1;
    }
    if (mp->freemem[i].block_count != 0) {
        DERROR("mp->freemem[i].block_count != 0\n");
        return -1;
    }

    return 0;
}
