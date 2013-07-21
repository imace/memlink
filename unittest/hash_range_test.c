#include "hashtest.h"
#include "memlink_client.h"

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
	unsigned int attrarray[4][3] = {{ 7, 2, 1}, {6, 2, 1}, { 4, 1, UINT_MAX}, {8, 3, 1}}; 
	int num     = 200;
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

	ret = hashtable_create_table(ht, name, valuesize, attrformat, attrnum, MEMLINK_LIST, 0);
    if (ret != MEMLINK_OK) {
        DERROR("create table error:%d\n", ret);
        return -1;
    }
    Table *tb = hashtable_find_table(ht, name);
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
	int pos = 0;

	for (i = 0; i < num; i++) {
		sprintf(val, "value%03d", i);
		pos = i;
        DINFO("add key:%s, val:%s pos:%d\n", key, val, pos);
		ret = hashtable_insert(ht, name, key, val, attrarray[3], attrnum, pos); //{8, 3, 1}
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

    // change attr
	for (i = 0; i < 100; i++) {
		sprintf(val, "value%03d", i*2);
        DINFO("set attr, key:%s, val:%s, attr: %d:%d:%d\n", key, val, 
                attrarray[2][0], attrarray[2][1], attrarray[2][2]);
		ret = hashtable_attr(ht, name, key, val, attrarray[2], attrnum); //{ 4, 1, UINT_MAX}
		if (MEMLINK_OK != ret) {
			DINFO("err hashtable_attr val:%s, i:%d\n", val, i);
			return ret;
		}
	}
	
	//////////test 5 :range
	unsigned int attrarray2[6][3] = { {8, 3, 1}, {8, UINT_MAX, 1}, 
						 {UINT_MAX, 3, 1}, {4, 1, 1} , 
						 {UINT_MAX, UINT_MAX, 1},  {UINT_MAX, UINT_MAX, UINT_MAX}}; 
	for (i = 0; i < 50; i++) {
		int k; 
		if (i < 25) {
			k  = 1;  //{8, UINT_MAX, 1} attrnum:3		
			num = 100;
		}else{	
			k = 5;  //{UINT_MAX, UINT_MAX, UINT_MAX} attrnum:0	
			num = 200;
		}
		int frompos = my_rand(num);
		int len = my_rand(num);
		int realnum;

		if (frompos < num) {
			int tmp = num - frompos;
			(len < tmp)?(realnum = len):(realnum = tmp);
		}else{
			realnum = 0;
        }
		DINFO("frompos:%d, len:%d, i:%d\n", frompos, len, i);
	
        Conn    conn;
        memset(&conn, 0, sizeof(Conn));
		ret = hashtable_range(ht, name, key, MEMLINK_VALUE_VISIBLE, attrarray2[k], attrnum, 
			            frompos, len, &conn);
        
        if (len <= 0 && MEMLINK_ERR_RANGE_SIZE == ret) {
            DINFO("len == 0, ret error ok!\n");
            continue; 
        }

		if (MEMLINK_OK != ret) {
			DERROR("ret:%d, len:%d\n", ret, len);
			return -1;
		}

		MemLinkResult   result;
        memlink_result_parse(conn.wbuf, &result);
		
		if (result.count != realnum) {			
			DERROR("error!! realnum:%d, retnum:%d, i:%d\n", realnum, result.count, i);
			//return -1;
		}else{
			int j = frompos;
            MemLinkItem *items = result.items;
            int k;
            for (k = 0; k < result.count; k++) {
                DINFO("value:%s, attr:%s\n", items[k].value, items[k].attr);
                int jj;
				if (i < 25) {
					jj = j*2 + 1;
				}else{
					jj = j;
                }
				sprintf(val, "value%03d", jj);
                int ret = memcmp(items[k].value, val, result.valuesize);
                if (ret != 0) {
                    DERROR("error, pos:%d, val:%s\n", jj, val);
                    return -1;
                }
                j++;
            }
		}
        memlink_result_free(&result);
	}
	DINFO("hashtable range test end!\n");

	return 0;
}

