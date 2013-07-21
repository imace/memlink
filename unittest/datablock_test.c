#include "hashtest.h"
#include <stdio.h>
#include <assert.h>

int main()
{
#ifdef DEBUG
	logfile_create("stdout", 3);
#endif
    HashTable *ht;

    myconfig_create("memlink.conf");
	my_runtime_create_common("memlink");
	ht = g_runtime->ht;

    char key[128];
    int  valuesize = 10;
    int  attrnum = 0; 
    int  i = 0, ret;
    unsigned int attrformat[4] = {0};
    char *tbname = "test";

    sprintf(key, "haha%03d", i);
    ret = hashtable_create_table(ht, tbname, valuesize, attrformat, attrnum, MEMLINK_LIST, 0);
    if (ret != MEMLINK_OK) {
        DERROR("create table error:%s\n", tbname);
        return -1;
    }
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        DERROR("not found table:%s\n", tbname);
        return -1;
    }

    char attr[10] = {0x01};
    char value[16]= {0};

    for (i = 0; i < 1; i++) {
        sprintf(value, "%010d", i);
        ret = hashtable_insert_binattr(ht, tbname, key, value, attr, 0);
        if (ret != MEMLINK_OK) {
            DERROR("insert error:%d, key:%s, value:%d\n", ret, key, i);
        }
    }
    DINFO("====== %d ======\n", i);
    HashNode *node = table_find(tb, key);
    table_print(tb, key);
    assert(node->data->data_count == 1);

    for ( ; i < 2; i++) {
        sprintf(value, "%010d", i);
        ret = hashtable_insert_binattr(ht, tbname, key, value, attr, 0);
        if (ret != MEMLINK_OK) {
            DERROR("add error:%d, key:%s, value:%d\n", ret, key, i);
        }
    }
    DINFO("====== %d ======\n", i);
    table_print(tb, key);

    assert(node->data->data_count == 2);

    for ( ; i < 5; i++) {
        sprintf(value, "%010d", i);
        ret = hashtable_insert_binattr(ht, tbname, key, value, attr, 0);
        if (ret != MEMLINK_OK) {
            DERROR("add error:%d, key:%s, value:%d\n", ret, key, i);
        }
        DINFO("data count:%d, used:%d\n", node->data->data_count, node->used);

        DINFO("====== %d ======\n", i);
        table_print(tb, key);

        assert(node->data->data_count == 5);
    }

    for ( ; i < 10; i++) {
        sprintf(value, "%010d", i);
        ret = hashtable_insert_binattr(ht, tbname, key, value, attr, 0);
        if (ret != MEMLINK_OK) {
            DERROR("add error:%d, key:%s, value:%d\n", ret, key, i);
        }
        DINFO("====== %d ======\n", i);
        table_print(tb, key);

        assert(node->data->data_count == 10);
    }

    for ( ; i < 23; i++) {
        sprintf(value, "%010d", i);
        ret = hashtable_insert_binattr(ht, tbname, key, value, attr, 0);
        if (ret != MEMLINK_OK) {
            DERROR("add error:%d, key:%s, value:%d\n", ret, key, i);
        }
    }
    DINFO("====== %d ======\n", i);
    table_print(tb, key);

    assert(node->data->data_count == 5);
    
    for (i = 19; i >= 10; i--) {
        sprintf(value, "%010d", i);
        ret = hashtable_del(ht, tbname, key, value);
        if (ret != MEMLINK_OK) {
            DERROR("del error: %d, key:%s,val:%s\n", ret, key, value); 
        }
        table_print(tb, key);
    }

    // insert
    for (i = 100; i < 110; i++) {
        sprintf(value, "%010d", i);
        ret = hashtable_insert_binattr(ht, tbname, key, value, attr, 0);
        if (ret != MEMLINK_OK) {
            DERROR("add error:%d, key:%s, value:%d\n", ret, key, i);
        }
        table_print(tb, key);
        if (i < 102) {
            assert(node->data->data_count == 5);
        }else if (i >= 102 && i < 107) {
            assert(node->data->data_count == 10);
        }else if (i == 107) {
            assert(node->data->data_count == 1);
        }else if (i == 108) {
            assert(node->data->data_count == 2);
        }else if (i > 108){
            assert(node->data->data_count == 5);
        }
    }

    for (i = 200; i < 213; i++) {
        sprintf(value, "%010d", i);
        ret = hashtable_insert_binattr(ht, tbname, key, value, attr, -1);
        if (ret != MEMLINK_OK) {
            DERROR("add error:%d, key:%s, value:%d\n", ret, key, i);
        }
        table_print(tb, key);

        if (i == 200) {
            assert(node->data_tail->data_count == 1);
        }else if (i == 201) {
            assert(node->data_tail->data_count == 2);
        }else if (i < 205) {
            assert(node->data_tail->data_count == 5);
        }else if (i < 210) {
            assert(node->data_tail->data_count == 10);
        }else if (i == 210) {
            assert(node->data_tail->data_count == 1);
        }else if (i == 211) {
            assert(node->data_tail->data_count == 2);
        }else if (i >= 212){
            assert(node->data_tail->data_count == 5);
        }
    }

    return 0;
}
	
