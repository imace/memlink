#include "hashtest.h"


int check_keys (MemLinkStatSys *stat, int keys)
{
    if (stat->keys != keys)
        return -1;
    return 0;
}

int check_values(MemLinkStatSys *stat, int values)
{
    if (stat->values != values)
        return -1;
    return 0;
}

int check_blocks(MemLinkStatSys *stat, int blocks)
{
    if (stat->blocks != blocks)
        return -1;
    return 0;
}

int check_data_all(MemLinkStatSys *stat, int data_all)
{
    if (stat->data_all != data_all)
        return -1;
    return 0;
}

int check_ht_mem(MemLinkStatSys *stat, int ht_mem)
{
    if (stat->ht_mem != ht_mem)
        return -1;
    return 0;
}


int main(int argc, char **argv)
{
#ifdef DEBUG
    logfile_create("test.log", 3);
    //logfile_create("stdout", 4);
#endif
    char key[64];
    int  valuesize = 8;
    char val[64];
    unsigned int attrformat[3] = {4, 4, 3};
    unsigned int attrarray[3]  = {1, 2, 1};
    int  attrnum = 3;
    int  num = 1000;
    int  keys = 100;
    int  ret;
    char *conffile = "memlink.conf";
    char *name = "test";
    int  i;
    HashTable *ht;

    myconfig_create(conffile);

    my_runtime_create_common("memlink");

    ht = g_runtime->ht;
    
    ret = hashtable_create_table(ht, name, valuesize, attrformat, attrnum, MEMLINK_LIST, 0);
    Table *tb = hashtable_find_table(ht, name);
    //创建key
    for (i = 0; i < keys; i++) {
        sprintf(key, "test%03d", i);
        table_create_node(tb, key);
    }

    int pos = -1; 
    int j = 0;

    for (i = 0; i < keys; i++) {
        sprintf(key, "test%03d", i);
        for (j = 0; j < num; j++) {
            sprintf(val, "value%03d", j);
            ret = hashtable_insert(ht, name, key, val, attrarray, attrnum, pos);
            if (ret < 0) {
                DERROR("add value error: %d, %s\n", ret, val);
                return ret;
            }
        }
    }
    
    MemLinkStatSys stat;
    
    ret  = hashtable_stat_sys(ht, &stat);

    if (ret < 0) {
        DERROR("info_sys_stat error: %d\n", ret);
        return ret;
    }

    ret = check_keys(&stat, 100);
    if (ret < 0) {
        DERROR("check_keys error: %d\n", ret);
        return ret;
    }

    ret = check_values(&stat, 100 * 1000);
    if (ret < 0) {
        DERROR("check_values error: %d\n", ret);
        return ret;
    }

    HashNode *node;
    int size = 0;
    int blocks = 0;
    for (i = 0; i < HASHTABLE_BUNKNUM; i++) {
        node = tb->nodes[i];
        if (node) {
            size += strlen(node->key) + 1 + tb->attrnum;
        }
        while (node) {
            DataBlock *dbk = node->data;

            while(dbk) {
                size += sizeof(DataBlock);
                //DNOTE("sizeof(DataBlock): %d\n", sizeof(DataBlock));
                //DNOTE("data_count: %d\n", dbk->data_count);
                //DNOTE("node->attrsize + node->valuesize: %d\n", node->attrsize + node->valuesize);
                size += (tb->attrsize + tb->valuesize) * (dbk->data_count);
                dbk = dbk->next;
                blocks++;
            }
            size += sizeof(HashNode);
            node = node->next;
        }
    }
    ret = check_blocks(&stat, blocks);
    if (ret < 0) {
        DNOTE("stat.blocks: %d, blocks: %d\n", stat.blocks, blocks);
        DERROR("check_blocks: %d\n", ret);
        return ret;
    }
 
    int ht_mem;
    ht_mem = size;
    ret = check_ht_mem(&stat, ht_mem);
    if (ret < 0) {
        DNOTE("stat->ht_mem: %d, ht_mem: %d\n", stat.ht_mem, ht_mem);
        DERROR("check_ht_mem: %d\n", ret);
        return ret;
    }
    return 0;
}
