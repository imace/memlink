#include "hashtest.h"

int main()
{
#ifdef DEBUG
	logfile_create("stdout", 3);
#endif
	HashTable* ht;
	char key[64];
	int valuesize = 8;
	char val[64];
	unsigned int attrformat[3]   = {4, 3, 1};
	unsigned int attrarray[3][3] = { {8, 3, 1}, { 7, 2,1}, {6, 2, 1} }; 
	int num  = 200;
	int attrnum = 3;
	int ret;
	int i = 0;
    char *name = "test";
	char *conffile;
	conffile = "memlink.conf";
	DINFO("config file: %s\n", conffile);
	myconfig_create(conffile);

	my_runtime_create_common("memlink");
	ht = g_runtime->ht;

    ret = hashtable_create_table(ht, name, valuesize, attrformat, attrnum, 
                    MEMLINK_LIST, MEMLINK_VALUE_STRING);
    if (ret != MEMLINK_OK) {
        DERROR("create table error:%d\n", ret);
        return -1;
    }
    Table *tb = hashtable_find_table(ht, name);
	///////////begin test;
	//test1 : hashtable_add_info_attr - create key
	for (i = 0; i < num; i++) {
		sprintf(key, "heihei%03d", i);
		table_create_node(tb, key);
	}
	for (i = 0; i < num; i++) {
		sprintf(key, "heihei%03d", i);
		HashNode* pNode = table_find(tb, key);
		if (NULL == pNode) {
			DERROR("table_find error. can not find %s\n", key);
			return -1;
		}
	}

    //test : hashtable_add_attr insert num value
	DINFO("1 insert 1000 \n");

	int pos = 0;
	HashNode *node = NULL;
	DataBlock *dbk = NULL;
	char	 *item = NULL; 
	num = 200;

	for (i = 0; i < num; i++) {
		sprintf(val, "value%03d", i);
		pos = i;
		ret = hashtable_insert(ht, name, key, val, attrarray[i%3], 3, pos);
		if (ret < 0) {
			DERROR("hashtable_add_attr err! ret:%d, val:%s, pos:%d \n", ret, val, pos);
			return ret;
		}
	}

	DINFO("2 insert %d\n", num);
    
    MemLinkStat stat;
    ret = hashtable_stat(ht, name, key, &stat);
    if (ret != MEMLINK_OK) {
        DERROR("stat error!\n");
    }

    DINFO("blocks:%d, data_used:%d\n", stat.blocks, stat.data_used);

	for (i = 0; i < num; i++) {
		sprintf(val, "value%03d", i);
		ret = table_find_value(tb, key, val, &node, &dbk, &item);
		if (ret < 0) {
			DERROR("not found value: %d, %s\n", ret, val);
			return ret;
		}
	}
	
	//hashtable_print(g_runtime->ht, key);
	sprintf(val, "value%03d", 50);
	ret = hashtable_del(ht, name, key, val);
	if (ret < 0) {
		DERROR("del value: %d, %s\n", ret, val);
		return ret;
	}
    DINFO("deleted %s\n", val);	
	table_print(tb, key);

    ret = table_find_value(tb, key, val, &node, &dbk, &item);
    if (ret >= 0) {
        DERROR("found value: %d, %s\n", ret, val);
        return ret;
    }

	ret = hashtable_insert(ht, name, key, val, attrarray[2], attrnum, 100);
	if (ret < 0) {
		DERROR("add value err: %d, %s\n", ret, val);
		return ret;
	}

	ret = table_find_value(tb, key, val, &node, &dbk, &item);
	if (ret < 0) {
		DERROR("not found value: %d, %s\n", ret, val);
		return ret;
	}
	DINFO("test: insert ok!\n");

    table_print(tb, key);
    /////////// hashtable_del
	for (i = 0; i < num; i++) {
		sprintf(val, "value%03d", i);
		//pos = i;
		ret = hashtable_del(ht, name, key, val);
		if (ret < 0) {
			DERROR("hashtable_del err! ret:%d, val:%s \n", ret, val);
			return ret;
		}
	}
	DINFO("del %d!\n", num);

	for (i = 0; i < num; i++) {
		sprintf(val, "value%03d", i);
		ret = table_find_value(tb, key, val, &node, &dbk, &item);
		if (ret >= 0) {
			DERROR("err should not found value: %d, %s\n", ret, key);
			return ret;
		}
	}
	
	DINFO("test: del ok!\n");
	return 0;
}
