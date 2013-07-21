#include "hashtest.h"

int main()
{
#ifdef DEBUG
	logfile_create("test.log", 3);
#endif
	HashTable* ht;
	char key[64];
	int valuesize = 8;
	char val[64];
	unsigned int attrformat[3] = {4, 3, 1};	
	unsigned int attrarray[6][3] = { { 7, UINT_MAX, 1}, {6, 2, 1}, { 4, 1, UINT_MAX}, 
		                    {8, 3, 1}, {8, 8, 8}, { UINT_MAX, UINT_MAX, UINT_MAX} }; 
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

	///////////begin test;
	//test1 : hashtable_add_info_attr - create key
    ret = hashtable_create_table(ht, name, valuesize, attrformat, attrnum, MEMLINK_LIST, 0);
    if (ret != MEMLINK_OK) {
        DERROR("create table error:%d\n", ret);
        return -1;
    }

    Table *tb = hashtable_find_table(ht, name);

	for (i = 0; i < num; i++) {
		sprintf(key, "heihei%03d", i);
		ret = table_create_node(tb, key);
        if (ret != MEMLINK_OK) {    
            DERROR("create node error:%d\n", ret);
            return -1;
        }
	}
	for (i = 0; i < num; i++) {
		sprintf(key, "heihei%03d", i);
		HashNode* pNode = table_find(tb, key);
		if(NULL == pNode) {
			DERROR("hashtable_add_info_attr error. can not find %s\n", key);
			return -1;
		}
	}

	///////test : hashtable_add_attr - insert num value
	HashNode *node = NULL;
	DataBlock *dbk = NULL;
	char	 *item = NULL; 	
	int pos = 0;

	for (i = 0; i < num; i++) {
		sprintf(val, "value%03d", i);
		pos = i;
		ret = hashtable_insert(ht, name, key, val, attrarray[i%4], attrnum, pos);
		if (ret < 0) {
			DERROR("add value err: %d, %s\n", ret, val);
			return ret;
		}
		
		ret = table_find_value(tb, key, val, &node, &dbk, &item);
		if (ret < 0) {
			DERROR("not found value: %d, %s\n", ret, key);
			return ret;
		}
	}
	DINFO("insert %d values ok!\n", num);

	//////////test 5 :count
	int visible_count;
	int attr_count;
	for (i = 0; i < 4; i++) {
		ret = hashtable_count(ht, name, key, attrarray[i], attrnum, &visible_count, &attr_count);
		if (ret < 0) {
			DERROR("count key err: %d, %s\n", ret, key);
			return ret;
		}
		if((visible_count) != 50) {
			DERROR("count key err: %d, %s, visible_count:%d\n", ret, key, visible_count);
			return ret;
		}
	}
	
	///attrfomat is out of bound: {8, 8, 8}
	ret = hashtable_count(ht, name, key, attrarray[4], attrnum, &visible_count, &attr_count);
	if (ret < 0) {
		DERROR("count key err: %d, %s\n", ret, key);
		return ret;
	}
	if(visible_count == 50) {
		DERROR("count key err: %d, %s, visible_count:%d\n", ret, key, visible_count);
		return ret;
	}

	//attr is none
	ret = hashtable_count(ht, name, key, attrarray[5], attrnum, &visible_count, &attr_count);
	if (ret < 0) {
		DERROR("count key err: %d, %s\n", ret, key);
		return -1;
	}
	if(visible_count != num) {
		DERROR("count key err: %d, %s, visible_count:%d\n", ret, key, visible_count);
		return -1;
	}
	DINFO("hashtable count test end!\n");
	return 0;
}


