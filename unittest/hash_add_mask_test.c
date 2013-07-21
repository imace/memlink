#include "hashtest.h"

int
find_value_in_block(Table *tb, HashNode *node, DataBlock *dbk, void *value)
{
    int pos = 0;
    int datalen = tb->valuesize + tb->attrsize;
    char *data = dbk->data; 
    int i;
    
    for (i = 0; i < dbk->data_count; i++) {
        if (dataitem_have_data(tb, node, data, 0) && memcmp(value, data, tb->valuesize) == 0) {
            if (pos == 0)
                return 1;
            return pos;
        }
        pos++;
        data += datalen;
    }

    return -1;
}

int
build_data_model(HashTable *ht, char *name, char *key, int valuesize, 
                unsigned int *attrformat, int attrnum, int num)
{
    int ret;

    ret = hashtable_create_table(ht, name, valuesize, attrformat, attrnum, MEMLINK_LIST, MEMLINK_VALUE_STRING);
    if (ret != MEMLINK_OK) {
        DERROR("key create error, ret: %d, key: %s\n", ret, key);
        return ret;
    }

    int pos = -1;
    unsigned int attrarray[3] = {4, 4, 4}; 
    int i;
    char val[64];
    for (i = 0; i < num; i++) {
        snprintf(val, 64, "value%03d", i);
        ret = hashtable_insert(ht, name, key, val, attrarray, 3, pos);
        if (ret != MEMLINK_OK) {
            DERROR("add value error: %d, %s\n", ret, val);
            return ret;
        }
    }
    DINFO("insert %d value ok!\n", num);
    return 0;
}

//插入20条数据， 第一块第二块都是满的，每块10条，在第一块的任意位置插入一条数据
int
second_block_full_test(HashTable *ht, char *name, char *key, int valuesize, 
                        unsigned int *attrformat, int attrnum)
{
    int ret;
    HashNode *node;
    char val[64];
    int pos;

    ret = build_data_model(ht, name, key, valuesize, attrformat, attrnum, 20);
    if (ret < 0)
        return ret;
    
    Table *tb = hashtable_find_table(ht, name);

    node = table_find(tb, key);
    int blockmax = g_cf->block_data_count[g_cf->block_data_count_items - 1];
    DataBlock *dbk = node->data;
    DataBlock *nextdbk;

    if (dbk == NULL) {
        return -4;
    }
    nextdbk = dbk->next;

    //校验分配的两块是否是满的，而且达到了最大
    if (dbk->data_count != blockmax || dbk->visible_count + dbk->tagdel_count != dbk->data_count) {
        DERROR("the first block is not full\n");
        return -5;
    }
    if (nextdbk->data_count != blockmax || nextdbk->visible_count + nextdbk->tagdel_count != nextdbk->data_count) {
        DERROR("the second block is not full\n");
        return -6;
    }

    //在第一块的某个位置插入一个条数据

    snprintf(val, 64, "value:%03d", 99);
    pos = 3;

    unsigned int attrarray[3] = {1, 1, 1};
    ret = hashtable_insert(ht, name, key, val, attrarray, 3,  pos);
    if (ret != MEMLINK_OK) {
        DERROR("add value error: %d, %s\n", ret, val);
        return ret;
    }
    DataBlock *first = node->data;
    DataBlock *second, *third;

    second = first->next;
    if (second == NULL)
        return -7;
    third = second->next;
    if (third == NULL)
        return -8;

    if (first->data_count != blockmax || third->data_count != blockmax) {
        return -9;
    }
    DNOTE("first->data_count: %d, third->data_count: %d\n", first->data_count, third->data_count);
    if (second->data_count != 1) {
        return -10;
    }
    DNOTE("second->data_count: %d\n", second->data_count);
    
    //插入的数据是否在第一块的pos=3的位置
    pos = find_value_in_block(tb, node, first, val);
    DNOTE("pos: %d\n", pos);
    if (pos  != 3) {
        return -11;
    }

    //原来第一块的最后一条数据是否移到了第二块中
    snprintf(val, 64, "value%03d", 9);
    pos = find_value_in_block(tb, node, second, val);
    DNOTE("pos: %d\n", pos);
    if (pos != 1)
        return -12;

    hashtable_clear_all(ht);
    return 0;
}

//后一块block有空， 且是第一个位置有空
int
second_not_full_test1(HashTable *ht, char *name, char *key, int valuesize, unsigned int *attrformat, int attrnum)
{
    int ret;
    HashNode *node;
    char val[64];
    int pos;

    ret = build_data_model(ht, name, key, valuesize, attrformat, attrnum, 20);
    if (ret < 0)
        return ret;
    
    Table *tb = hashtable_find_table(ht, name); 
    node = table_find(tb, key);
    int blockmax = g_cf->block_data_count[g_cf->block_data_count_items - 1];
    DataBlock *dbk = node->data;
    DataBlock *nextdbk;
    
    if (dbk == NULL) {
        return -4;
    }
    nextdbk = dbk->next;
    //删除第二个块中的第一个数据
    snprintf(val, 64, "value%03d", 10);
    ret = hashtable_del(ht, name, key, val);
    if (ret != MEMLINK_OK) {
        DERROR("hashtable_del: %d\n", ret);
        return ret;
    }
    //在第一块的pos=3的位置插入一条数据
    snprintf(val, 64, "value%03d", 99);
    pos  = 3;
    unsigned int attrarray[3] = {2, 2, 2};

    ret = hashtable_insert(ht, name, key, val, attrarray, 3, pos);
    if (ret != MEMLINK_OK) {
        DERROR("add value error: %d, %s\n", ret, val);
        return ret;
    }

    DataBlock *first = node->data;
    DataBlock *second;

    if (first == NULL)
        return -5;
    second = first->next;
    if (second == NULL)
        return -6;

    if (first->data_count != blockmax || first->visible_count +
        first->tagdel_count != first->data_count) {
        DERROR("the first block is not full\n");
        return -7;
    }
    if (second->data_count != blockmax || second->visible_count +
        second->tagdel_count != second->data_count) {
        DERROR("the second block is not full\n");
        return -8;
    }
    snprintf(val, 64, "value%03d", 99);
    pos = find_value_in_block(tb, node, first, val);
    DNOTE("pos: %d\n", pos);
    if (pos != 3)
        return -9;

    snprintf(val, 64, "value%03d", 9);
    pos = find_value_in_block(tb, node, second, val);
    DNOTE("pos: %d\n", pos);
    if (pos != 1)
        return -10;

    hashtable_clear_all(ht);
    return 0;
}

//第一块是满的，下一块有一个空缺，且不是第一个
int
second_not_full_test2(HashTable *ht, char *name, char *key, int valuesize, unsigned int *attrformat, int attrnum)
{
    int ret;
    HashNode *node;
    char val[64];
    int pos;

    ret = build_data_model(ht, name, key, valuesize, attrformat, attrnum, 20);
    if (ret < 0)
        return ret;

    Table *tb = hashtable_find_table(ht, name); 
    node = table_find(tb, key);
    int blockmax = g_cf->block_data_count[g_cf->block_data_count_items - 1];
    DataBlock *dbk = node->data;
    DataBlock *nextdbk;

    if (dbk == NULL)
        return -4;

    nextdbk = dbk->next;

    //删除第二块中中间的某个数据
    snprintf(val, 64, "value%03d", 13);
    ret = hashtable_del(ht, name, key, val);
    if (ret != MEMLINK_OK) {
        DERROR("hashtable_del: %d\n", ret);
        return ret;
    }
    //在第一块pos=3的位置插入一条数据
    snprintf(val, 64, "value%03d", 99);
    pos = 3;
    unsigned int attrarray[3] = {2, 2, 2};
    ret = hashtable_insert(ht, name, key, val, attrarray, 3, pos);
    if  (ret != MEMLINK_OK) {
        DERROR("add value error: %d, %s\n", ret, val);
        return ret;
    }
    DataBlock *first = node->data;
    DataBlock *second;

    if (first == NULL)
        return -5;
    second = first->next;
    if (second == NULL)
        return -6;

    if (first->data_count != blockmax || first->visible_count +
            first->tagdel_count != first->data_count) {
        DERROR("the first block is not full\n");
        return -7;
    }
    if (second->data_count != blockmax || second->visible_count +
            second->tagdel_count != second->data_count) {
        DERROR("the second block is not full\n");
        return -8;
    }

    snprintf(val, 64, "value%03d", 99);
    pos = find_value_in_block(tb, node, first, val);
    DNOTE("pos: %d\n", pos);
    if (pos != 3)
        return -9;
    
    snprintf(val, 64, "value%03d", 9);
    pos = find_value_in_block(tb, node, second, val);
    DNOTE("pos: %d\n", pos);
    if (pos != 1)
        return -10;

    hashtable_clear_all(ht);
    return 0;
}

//第一块是满的， 但是能增大
int
first_is_full_test1(HashTable *ht, char *name, char *key, int valuesize, unsigned int *attrformat, int attrnum)
{
    int ret;
    HashNode *node;
    char val[64];
    int pos;

    ret = build_data_model(ht, name, key, valuesize, attrformat, attrnum, 5);
    if (ret < 0)
        return ret;

    Table *tb = hashtable_find_table(ht, name); 
    node = table_find(tb, key);
    int blockmax = g_cf->block_data_count[g_cf->block_data_count_items - 2];
    DataBlock *dbk = node->data;
    
    if (dbk->data_count != blockmax || dbk->visible_count + dbk->tagdel_count != dbk->data_count) {
        DERROR("the first block is not full\n");
        return -4;
    }

    snprintf(val, 64, "value%03d", 99);
    pos = 3;
    unsigned int attrarray[3] = {2, 2, 2};

    ret = hashtable_insert(ht, name, key, val, attrarray, 3, pos);
    if (ret != MEMLINK_OK) {
        DERROR("add value error: %d, %s\n", ret, val);
        return ret;
    }
    
    dbk = node->data;
    if (dbk->data_count != g_cf->block_data_count[g_cf->block_data_count_items - 1])
        return -5;

    pos = find_value_in_block(tb, node, dbk, val);
    DNOTE("pos: %d\n", pos);
    if (pos != 3)
        return -6;

    hashtable_clear_all(ht); 
    return 0;
}

int 
second_block_full_test2(HashTable *ht, char *name, char *key, int valuesize, unsigned int *attrformat, int attrnum)
{
    int ret;
    HashNode *node;
    char val[64];
    int pos;

    ret = build_data_model(ht, name, key, valuesize, attrformat, attrnum, 15);
    if (ret < 0)
        return ret;
    Table *tb = hashtable_find_table(ht, name); 

    node = table_find(tb, key);
    DataBlock *dbk = node->data;
    DataBlock *nextdbk;

    if (dbk == NULL) {
        return -4;
    }
    nextdbk = dbk->next;

    if (dbk->data_count != g_cf->block_data_count[g_cf->block_data_count_items - 1])
        return -5;
    if (nextdbk->data_count != g_cf->block_data_count[g_cf->block_data_count_items - 2])
        return -6;

    snprintf(val, 64, "value%03d", 99);
    pos = 3;
    unsigned int attrarray[3] = {1, 1, 1}; 
    ret = hashtable_insert(ht, name, key, val, attrarray, 3, pos);
    if (ret != MEMLINK_OK) {
        DERROR("add value error: %d, %s\n", ret, val);
        return ret;
    }
    
    dbk = node->data;

    nextdbk = dbk->next;
    if (nextdbk == NULL)
        return -7;

    if (dbk->data_count != g_cf->block_data_count[g_cf->block_data_count_items - 1])
        return -8;

    if (nextdbk->data_count != g_cf->block_data_count[g_cf->block_data_count_items - 1])
        return -9;

    pos = find_value_in_block(tb, node, dbk, val);
    DNOTE("pos: %d\n", pos);
    if (pos != 3)
        return -10;

    snprintf(val, 64, "value%03d", 9);
    pos = find_value_in_block(tb, node, nextdbk, val);
    DNOTE("pos: %d\n", pos);
    if (pos != 1)
        return -11;
    
    hashtable_clear_all(ht);
    return 0;
}

int main(int argc, char **argv)
{
#ifdef DEBUG
    logfile_create("test.log", 3);
#endif
    HashTable *ht;
    char *conffile = "memlink.conf";

    myconfig_create(conffile);
    my_runtime_create_common("memlink");
    ht = g_runtime->ht;

    char key[64];
    int  valuesize = 8;
    unsigned int attrarray[3] = {4, 4, 4};
    char *name = "test";
    snprintf(key, 64, "%s", "test");

    int ret = second_block_full_test(ht, name, key, valuesize, attrarray, 3);
    
    DNOTE("second_block_full_test: %d\n", ret);
    if (ret < 0)
        return ret;

    ret = second_not_full_test1(ht, name, key, valuesize, attrarray, 3);
    DNOTE("second_not_full_test1: %d\n", ret);

    if (ret < 0)
        return ret;
    
    ret = second_not_full_test2(ht, name, key, valuesize, attrarray, 3);
    DNOTE("second_not_full_test2: %d\n", ret);

    if (ret < 0)
        return ret;

    ret = first_is_full_test1(ht, name, key, valuesize, attrarray, 3);
    DNOTE("first_is_full_test1: %d\n", ret);
    if (ret < 0)
        return ret;

    ret = second_block_full_test2(ht, name, key, valuesize, attrarray, 3);
    DNOTE("second_block_full_test2: %d\n", ret);
    if (ret < 0)
        return ret;

    return 0;
}
