#include "hashtest.h"

int check2(HashTableStat *stat, int vs, int ms, int blocks, int data, int datau, int mem, int memu)
{
	if (stat->valuesize != vs) {
	    DERROR("valuesize error: %d\n", stat->valuesize);
	}

	if (stat->attrsize != ms) {
		DERROR("attrsize error: %d\n", stat->attrsize);
	}

	if (stat->blocks != blocks) {
		DERROR("blocks error: %d\n", stat->blocks);
	}
	
	if (stat->data != data)  {
		DERROR("data error: %d\n", stat->data);
	}

	if (stat->data_used != datau) {
		DERROR("data_used error: %d\n", stat->data_used);
	}
	
	if (stat->mem != mem) {
		DERROR("mem  error: %d   %d\n", stat->mem, mem);
	}

	//if (stat->mem_used != memu) {
		//printf("mem_used error: %d      %d\n", stat->mem_used, memu);
	//}
	return 0;
}

int main()
{
#ifdef DEBUG
	logfile_create("test.log", 3);
	//logfile_create("stdout", 4);
#endif
	HashTable* ht;
	char key[64];
	int  valuesize = 8;
	char val[64];
	unsigned int  attrformat[3] = {4, 3, 1};	
	unsigned int attrarray[6][3] = { { 7, UINT_MAX, 1}, {6, 2, 1}, { 4, 1, UINT_MAX}, 
		                    {8, 3, 1}, {8, 8, 8}, { UINT_MAX, UINT_MAX, UINT_MAX} }; 
	int num  = 199;
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

	hashtable_create_table(ht, name, valuesize, attrformat, attrnum, MEMLINK_LIST, 0);
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
			printf("hashtable_add_info_attr error. can not find %s\n", key);
			return -1;
		}
	}

	///////test : hashtable_add_attr insert num value
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

	//////////test 8:stat
	HashTableStat stat;
	hashtable_stat(ht, name, key, &stat);
	int ms  = sizeof(HashNode) + (10 * 100 + sizeof(DataBlock)) * 2;
	int mus = 0;
	check2(&stat, valuesize, 2, 2, 200, 199, ms, mus);
	
	DINFO("hashtable stat test end!\n");
	return 0;
}



