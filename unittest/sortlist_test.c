#include "hashtest.h"
#include "datablock.h"
#include "utils.h"
#include <stdio.h>
#include <assert.h>

int check_values(int *values, int values_count, HashNode *node, Table *tb)
{
    DINFO("<<<<<< check values, used:%d, value count:%d >>>>>>\n", node->used, values_count);
    assert(node->used == values_count); 
    DataBlock *dbk = node->data;

    int i, vi = 0;
    int datalen = tb->valuesize + tb->attrsize;
    while (dbk) {
        char *itemdata = dbk->data;
        for (i = 0; i < dbk->data_count; i++) {
            if (dataitem_have_data(tb, node, itemdata, MEMLINK_VALUE_VISIBLE)){
                DINFO("value cmp: %d, %d\n", values[vi], *(int*)itemdata);
                assert(values[vi] == *(int*)itemdata);
                vi++;
            }
            itemdata += datalen; 
        }
        dbk = dbk->next;
    }
    return 0;
}

int add_one_value(HashTable *ht, HashNode *node, char *name, char *key, int value, char *attr, 
                    int *values, int *values_count)
{
    DINFO("add one: %d\n", value);
    //hashtable_print(ht, key);
    //check_values(values, *values_count, node);
    assert(node->used == *values_count);
    
    Table *tb = hashtable_find_table(ht, name);
    //int value = 5;
    int ret = hashtable_sortlist_insert_binattr(ht, name, key, &value, attr);
    if (ret != MEMLINK_OK) {
        DERROR("add error:%d, key:%s, value:%d\n", ret, key, value);
    }
    values[*values_count] = value;
    (*values_count)++;
    qsort(values, *values_count, sizeof(int), compare_int);
    /*int i;
    for (i = 0; i < *values_count; i++) {
        DINFO("i:%d, value:%d\n", i, values[i]);
    }*/
    DINFO("hashtable print %s ...\n", key);
    table_print(tb, key);
    DINFO("check values ...\n");
    check_values(values, *values_count, node, tb);

    return 0;
}

int main()
{
#ifdef DEBUG
	logfile_create("test.log", 3);
	//logfile_create("stdout", 4);
#endif
    HashTable *ht;

    myconfig_create("memlink.conf");
	my_runtime_create_common("memlink");
	ht = g_runtime->ht;

    char key[128];
    int  valuesize = 4;
    int  attrnum = 0; 
    int  i = 0, ret;
    unsigned int attrformat[4] = {0};
    char *name = "test";

    sprintf(key, "haha%03d", i);
    hashtable_create_table(ht, name, valuesize, attrformat, attrnum, 
                              MEMLINK_SORTLIST, MEMLINK_VALUE_UINT4);
  
    Table *tb = hashtable_find_table(ht, name);
    char attr[10] = {0x01};
    int value;
    int values[1024] = {0};
    int values_count = 0;
    HashNode *node = NULL; // = table_find(tb, key);

    for (i = 10; i < 20; i++) {
        value = i * 2;
        ret = hashtable_sortlist_insert_binattr(ht, name, key, &value, attr);
        if (ret != MEMLINK_OK) {
            DERROR("add error:%d, key:%s, value:%d\n", ret, key, i);
        }
        if (node == NULL)
            node = table_find(tb, key);
        values[values_count] = value;
        values_count++;
        assert(node->used == values_count);

        table_print(tb, key);
    }
    qsort(values, values_count, sizeof(int), compare_int);
    /*for (i = 0; i < values_count; i++) {
        DINFO("i:%d, value:%d\n", i, values[i]);
    }*/
    //hashtable_print(ht, key);
    check_values(values, values_count, node, tb);
    assert(node->used == 10);
   
    value = 100;
    DINFO("add value:%d\n", value);
    add_one_value(ht, node, name, key, value, attr, values, &values_count);

    value = 1;
    DINFO("add value:%d\n", value);
    add_one_value(ht, node, name, key, value, attr, values, &values_count);

    return 0;
}
