#include "hashtest.h"

int main()
{
#ifdef DEBUG
	logfile_create("test.log", 3);
	//logfile_create("stdout", 4);
#endif
	char key[64];
	int  valuesize = 8;
	char val[64];
	HashTable   *ht;
	unsigned int attrformat[3]   = {4, 3, 1};
	unsigned int attrarray[3][3] = { {8, 3, 1}, { 7, 2,1}, {6, 2, 1} }; 
	int  num     = 50;
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

    ret = hashtable_create_table(ht, name, valuesize, attrformat, attrnum, 
                                        MEMLINK_LIST, MEMLINK_VALUE_STRING);
    Table *tb = hashtable_find_table(ht, name);
	///////////begin test;
	//test1 : hashtable_add_info_attr - create key
	for (i = 0; i < num; i++) {
		sprintf(key, "heihei%03d", i);
		ret = table_create_node(tb, key);
        if (ret != MEMLINK_OK) {
            DERROR("create key %s error!\n", key);
            return -1;
        }
	}
	for (i = 0; i < num; i++) {
		sprintf(key, "heihei%03d", i);
		HashNode* pNode = table_find(tb, key);
		if (NULL == pNode) {
			DERROR("hashtable_add_info_attr error. can not find %s\n", key);
			return -1;
		}
	}

	///////test : hashtable_add_attr insert 200 value
	HashNode *node = NULL;
	DataBlock *dbk = NULL;
	char	 *item = NULL; 	
	int        pos = 0;

	for (i = 0; i < num; i++) {
		sprintf(val, "value%03d", i);
		pos = i;
        DINFO("insert value:%s, pos:%d\n", val, pos);
		hashtable_insert(ht, name, key, val, attrarray[i%3], attrnum, pos);
		if (ret < 0) {
			DERROR("insert value err, ret:%d, key:%s, value:%s\n", ret, key, val);
			return ret;
		}

        table_check(tb, key);

		ret = table_find_value(tb, key, val, &node, &dbk, &item);
		if (ret < 0) {
			DERROR("not found value, ret:%d, key:%s, value:%s\n", ret, key, val);
			return ret;
		}

	}
	DNOTE("insert %d  ok!\n", num);
    //hashtable_print(ht, key);

	///////test3 : hashtable_move_
	for (i = 0; i < 100; i++) {
		int v   = my_rand(num);
		int pos = 0;
		sprintf(val, "value%03d", v);
		while (1) {
			pos = my_rand(num);
			if (pos != v)
				break;
		}
		DINFO("+++++=== hashtable_move key:%s value:%s, pos:%d ===+++++\n", key, val, pos);
		int ret = hashtable_move(ht, name, key, val, pos);
		if (MEMLINK_OK != ret) {
			DERROR("err hashtable_move value:%s, pos:%d\n", val, pos);
			break;
		}
	    //DataBlock *dbk = node->data;
	    //int count = 0;
	    //char *posaddr  = NULL;
	    //DataBlock *last = NULL;
        /*
	    posaddr = dataitem_lookup(node, pos, 0, &dbk);
        DINFO("posaddr:%p\n", posaddr);
		if (NULL == posaddr) {
			DERROR("not found value:%s, pos:%d, posaddr:%p, dbk:%p\n",val, pos, posaddr, dbk);
			return -1;
		}else{
			ret = memcmp(val, posaddr, valuesize);
			if (0 == ret){
				DINFO("pos:%d is ok!\n", pos);
			}else{
				DERROR("err value! pos:%d is not the right value%s!\n", pos, val);
				return -1;
			}
		}*/

	}
	//hashtable_print(ht, key);
	DINFO("hashtable_move test end!\n");

	return 0;
}

