#include "hashtest.h"
#include "serial.h"

int main()
{
#ifdef DEBUG
	logfile_create("test.log", 3);
	//logfile_create("stdout", 4);
#endif
	HashTable* ht;
	char key[64];
	int valuesize = 8;
	char val[64];
	unsigned int attrformat[3] = {4, 3, 1};	
	unsigned char charattrformat[3] = {4, 3, 1};
	unsigned int attrarray[4][3] = { { 7, UINT_MAX, 1}, {6, 2, 1}, { 4, 1, UINT_MAX}, {8, 3, 1} }; 
	int num  = 200;
	int attrnum = 3;
	int ret;
	int i = 0;
	char *conffile;
    char *name = "test";
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
    
	///////test : hashtable_add_attr insert num value
	HashNode *node = NULL;
	DataBlock *dbk = NULL;
	char	 *item = NULL; 	
	int        pos = 0;

	for (i = 0; i < num; i++) {
		sprintf(val, "value%03d", i);
		pos = i;
		ret = hashtable_insert(ht, name, key, val, attrarray[i%4], attrnum, pos);
        DNOTE("insert value:%s, pos:%d, attrnum:%d, ret:%d\n", val, pos, attrnum, ret);
		if (ret < 0) {
			DERROR("add value err: %d, %s\n", ret, val);
			return ret;
		}

        table_print(tb, key);

		ret = table_find_value(tb, key, val, &node, &dbk, &item);
		if (ret < 0) {
			DERROR("not found value, ret:%d, key:%s, value:%s\n", ret, key, val);
			return ret;
		}
	}
	DINFO("insert %d values ok!\n", num);

	//////////test 5 :attr
	for (i = 0; i < num; i++) {
		HashNode *node = NULL;
		DataBlock *dbk = NULL;
		char	 *item = NULL; 
		int v = my_rand(num);		
		sprintf(val, "value%03d", v);
		
		int k = my_rand(4);
		//hashtable_find_value(ht, key, val, &node, &dbk, &item);
		ret = hashtable_attr(ht, name, key, val, attrarray[k], attrnum);
		if (MEMLINK_OK != ret) {
			DERROR("err hashtable_attr val:%s, k:%d\n", val, k);
			return ret;
		}
		
		ret = table_find_value(tb, key, val, &node, &dbk, &item);
		if (ret < 0) {
			DERROR("not found value: %d, %s\n", ret, val);
			return ret;
		}
		char data[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};
		char flag[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};
		ret = attr_array2binary(charattrformat, attrarray[k], attrnum, data); //to binary
		attr_array2flag(charattrformat, attrarray[k], attrnum, flag);   //to flag
		char attr[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};
		char *mdata = item + tb->valuesize;
		int j = 0;

		memcpy(attr, mdata, tb->attrsize); 
		attr[0] &= 0xfc;
		data[0] &= 0xfc;

		for (j = 0; j < ret; j++) {
		 	flag[j] = ~flag[j]; 
			if (( attr[j] & flag[j] ) != data[j]) {
				break;
            }
		}
		if (j != ret) {
			DERROR("attr error. %d\n", i);	
			break;
		}

	}
	DINFO("hashtable attr test end!\n");
	return 0;
}

