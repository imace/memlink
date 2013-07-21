#include "hashtest.h"

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
	unsigned int attrformat[3]   = {4, 3, 1};	
	unsigned int attrarray[6][3] = {{7, UINT_MAX, 1}, 
                                    {6, 2, 1}, 
                                    {4, 1, UINT_MAX}, 
		                            {8, 3, 1}, 
                                    {8, 8, 8}, 
                                    { UINT_MAX, UINT_MAX, UINT_MAX}}; 
	int  attrnum = 3;
	int  ret;
	int  i = 0;
	char *conffile;
    char *name = "test";

	conffile = "memlink.conf";
	DINFO("config file: %s\n", conffile);
	myconfig_create(conffile);

	my_runtime_create_common("memlink");
	ht = g_runtime->ht;


	ret = hashtable_create_table(ht, name, valuesize, attrformat, attrnum, MEMLINK_LIST, MEMLINK_VALUE_STRING);
    
    Table *tb = hashtable_find_table(ht, name);
	///////////begin test;
	//test1 : hashtable_add_info_attr - craete key
	/*for (i = 0; i < 1; i++) {
		sprintf(key, "heihei%03d", i);
		ret = hashtable_key_create_attr(ht, key, valuesize, attrformat, attrnum, MEMLINK_LIST, MEMLINK_VALUE_STRING);
        if (ret != MEMLINK_OK) {
            DERROR("key create error, ret:%d, key:%s\n", ret, key);
            return -1;
        }
	}*/
    /*
	for (i = 0; i < 1; i++) {
		sprintf(key, "heihei%03d", i);
		HashNode* pNode = table_find(tb, key);
		if(NULL == pNode) {
			DERROR("hashtable_add_info_attr error. can not find %s\n", key);
			return -1;
		}
	}*/

	///////test : hashtable_add_attr - insert 'num' value
	HashNode *node = NULL;
	DataBlock *dbk = NULL;
	char	 *item = NULL; 
	int num = 1000;	
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

    node = table_find(tb, key);
    DINFO("node used:%d, all:%d\n", node->used, node->all);
	//////////test 
	HashTableStat stat;
	ret = hashtable_stat(ht, name, key, &stat);
    if (ret != MEMLINK_OK) {
        DERROR("stat error, ret:%d, key:%s\n", ret, key);
        return -1;
    }
    DINFO("blocks:%d, used:%d, data:%d\n", stat.blocks, stat.data_used, stat.data);
	if (stat.data_used != num && stat.data != num) {
		DERROR("error stat.data_used:%d, key:%s\n", stat.data_used, key);
		return -1;
	}

	///del 500
	for (i = 0; i < num/2; i++) {
		sprintf(val, "value%03d", i*2);
		ret = hashtable_del(ht, name, key, val);
		if (ret < 0) {
			DERROR("hashtable_del error, ret:%d value:%s\n", ret, val);
			return ret;
		}
	}
    DINFO("del 500 node used:%d, all:%d\n", node->used, node->all);

	ret = hashtable_clean(ht, name, key);
	if (ret < 0) {
		DERROR("hashtable_clean error: %d\n", ret);
		return ret;
	}
    DINFO("del 500 clean node used:%d, all:%d\n", node->used, node->all);

	HashTableStat stat1;
	ret = hashtable_stat(ht, name, key, &stat1);
    if (ret != MEMLINK_OK) {
        DERROR("stat error, ret:%d, key:%s\n", ret, key);
        return -1;
    }
    DINFO("after clean blocks:%d, used:%d, data:%d\n", 
            stat1.blocks, stat1.data_used, stat1.data);
	if (stat1.data_used != num/2 || stat1.data != num/2) {
		DERROR("ERROR stat.data_used:%d, stat.data:%d, key:%s\n",
			    stat1.data_used, stat1.data, key);
		return -1;
	}

	///del all the rest
	for(i = 0; i < num/2; i++) {
		sprintf(val, "value%03d", i*2+1);
		ret = hashtable_del(ht, name, key, val);
		if (ret < 0) {
			DERROR("hashtable_del error: %d  val:%s\n", ret, val);
			return ret;
		}
	}
    DINFO("del 1000 node used:%d, all:%d\n", node->used, node->all);
	
	ret = hashtable_clean(ht, name, key);
	if (ret < 0) {
		DERROR("hashtable_clean error: %d\n", ret);
		return ret;
	}
    DINFO("del 1000 clean node used:%d, all:%d\n", node->used, node->all);
    
	table_print(tb, key);
	ret = hashtable_stat(ht, name, key, &stat1);
    if (ret != MEMLINK_OK) {
        DERROR("stat error, ret:%d, key:%s\n", ret, key);
        return -1;
    }
    DINFO("after del all clean blocks:%d, used:%d, data:%d\n", 
            stat1.blocks, stat1.data_used, stat1.data);

	if(stat1.data_used != 0 || stat1.data != 0) {
		DERROR("err stat.data_used:%d, stat.data:%d, key:%s\n",
			    stat1.data_used, stat1.data, key);
		return -1;
	}
	
	DINFO("hashtable clean test end!\n");
	return 0;
}

