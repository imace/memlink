#include <stdio.h>
#include <stdlib.h>
#include <memlink_client.h>
#include "logfile.h"
#include "test.h"

int main()
{
	MemLink	*m;
#ifdef DEBUG
	logfile_create("stdout", 4);
#endif
	m = memlink_create("127.0.0.1", MEMLINK_READ_PORT, MEMLINK_WRITE_PORT, 30);
	if (NULL == m) {
		DERROR("memlink_create error!\n");
		return -1;
	}

	int  ret;
	char key[32];
    char *name = "test";
	//sprintf(key, "%s.haha", name);
    strcpy(key, "haha");

	ret = memlink_cmd_create_table_list(m, name, 6, "4:3:1");
	
	if (ret != MEMLINK_OK) {
		DERROR("1 memlink_cmd_create %s error: %d\n", key, ret);
		return -2;
	}
	
	int i;
	char val[64];
	char *attrstr = "7:1:1";
	int  insertnum = 100;

	for (i = 0; i < insertnum; i++) {
		sprintf(val, "%06d", i);
		ret = memlink_cmd_insert(m, name, key, val, strlen(val), attrstr, i);
		if (ret != MEMLINK_OK) {
			DERROR("insert error, key:%s, val:%s, ret:%d\n", key, val, ret);
			return -3;
		}
	}

	int				reterr = 0;
	MemLinkResult	result;
	int				range_start = 50;
	int				range_count = 50;

	ret = memlink_cmd_range(m, name, key, MEMLINK_VALUE_REMOVED, "7:1:1", range_start, range_count, &result);
	if (ret == MEMLINK_OK) {
		DERROR("range error MEMLINK_VALUE_REMOVED key:%s, ret:%d\n", key, ret);
		return -4;
	}

	ret = memlink_cmd_range(m, name, key, MEMLINK_VALUE_VISIBLE, "7:1:1", -1, 0, &result);
	if (ret == MEMLINK_OK) {
		DERROR("range error, key:%s, ret:%d\n", key, ret);
		return -4;
	}

	ret = memlink_cmd_range(m, name, key, MEMLINK_VALUE_VISIBLE, "7:1:1", range_start, range_count, &result);
	if (ret != MEMLINK_OK) {
		DERROR("range error, key:%s, ret:%d\n", key, ret);
		return -4;
	}

	//DINFO("range return count: %d\n", result.count);
	if (result.count != range_count) {
		DERROR("range count error, count:%d, key:%s\n", result.count, key);
		reterr++;
	}

	if (result.valuesize != 6) {
		DERROR("range valuesize error, valuesize:%d, key:%s\n", result.valuesize, key);
		reterr++;
	}

	if (result.attrsize != 2) {
		DERROR("range attrsize erro, attrsize:%d, key:%s\n", result.attrsize, key);
		reterr++;
	}
		
	MemLinkItem	*item = result.items;
	char testkey[64];
	int  testi = range_start;
    
    i = 0;
	while (item) {
		sprintf(testkey, "%06d", testi);
		//DINFO("range item, %d value:%s, attr:%s\n", i, item->value, item->attr);
		if (strcmp(item->value, testkey) != 0) {
			DERROR("range value error, %d value:%s, testvalue:%s\n", i, item->value, testkey);
		}
		if (strcmp(item->attr, attrstr) != 0) {
			DERROR("range attr error, attr:%s\n", item->attr);
			reterr++;	
		}

		testi++;
        i++;
		item = item->next;
	}

	memlink_result_free(&result);
	
	char *attrtest[] = {"7:1:1", "7::1", "7:1:", ":1:1", "::1", ":1:"};

	for (i = 0; i < 6; i++) {
		MemLinkResult	result2;

		ret = memlink_cmd_range(m, name, key, MEMLINK_VALUE_VISIBLE, attrtest[i], 0, insertnum, &result2);
		if (ret != MEMLINK_OK) {
			DERROR("range error, ret:%d\n", ret);
			return -8;
		}
		if (result2.count != insertnum) {
			DERROR("range return count error, attr:%s, count:%d\n", attrtest[i], result2.count);
			reterr++;
			return -8;
		}

		memlink_result_free(&result2);
	}
	
////test: range 不存在的attr
	char *attrtest2[] = {"8:0:", "7:1:0"};

	for (i = 0; i < 2; i++) {
		MemLinkResult	result2;

		ret = memlink_cmd_range(m, name, key, MEMLINK_VALUE_VISIBLE, attrtest2[i], 0, insertnum, &result2);
		if (ret != MEMLINK_OK) {
			DERROR("range error, ret:%d\n", ret);
			return -8;
		}
		if (result2.count != 0) {
			DERROR("range return count error, attr:%s, count:%d\n", attrtest2[i], result2.count);
			reterr++;
			return -8;
		}

		memlink_result_free(&result2);

	}
///end test

//test :下面是删除20个条目(50--70)，再遍历
	int del_start = 50; 
	int del_count = 20;
	//sprintf(key, "%s.haha", name);
    strcpy(key, "haha");

	for (i = del_start; i < del_start + del_count; i++) {
		sprintf(val, "%06d", i);
		ret = memlink_cmd_del(m, name, key, val, strlen(val));
		if (ret != MEMLINK_OK) {
			DERROR("del error, key:%s, val:%s\n", key, val);
			return -5;
		}
	}
	
	MemLinkResult	result5;
	ret = memlink_cmd_range(m, name, key, MEMLINK_VALUE_VISIBLE, "::", 0, insertnum, &result5);
	if (ret != MEMLINK_OK) {
		DERROR("range error, key:%s, ret:%d\n", key, ret);
		return -4;
	}
	if (result5.count != insertnum - del_count) {
		DERROR("range count error, count:%d, key:%s\n", result5.count, key);
		reterr++;
	}
	
	item = result5.items;

	testi = 0;

	while (item) {		
        //DINFO("item->value:%d, testi:%d \n", item->value, testi);
		if( testi == 50 )
			testi += 20;
		sprintf(testkey, "%06d", testi);
		if (strcmp(item->value, testkey) != 0) {
			DERROR("range value error, value:%s, testvalue:%s\n", item->value, testkey);
		}
		if (strcmp(item->attr, attrstr) != 0) {
			DERROR("range attr error, attr:%s\n", item->attr);
			reterr++;	
		}

		testi++;
		item = item->next;
	}
	memlink_result_free(&result5);
///end test

	char *newattr = "8:2:0";
	for (i = 0; i < 3; i++) {
		sprintf(val, "%06d", i);
		ret = memlink_cmd_attr(m, name, key, val, strlen(val), newattr);
		if (ret != MEMLINK_OK) {
			DERROR("change attr error, i:%d, ret:%d\n", i, ret);	
			return -9;
		}
	}
	char* attrtest3[] = {"8::", "8:2:"};
	for (i = 0; i < 2; i++) {
		MemLinkResult	result2;

		ret = memlink_cmd_range(m, name, key, MEMLINK_VALUE_VISIBLE, attrtest3[i], 0, insertnum, &result2);
		if (ret != MEMLINK_OK) {
			DERROR("range error, ret:%d\n", ret);
			return -8;
		}
		if (result2.count != 3) {
			DERROR("range return count error, attr:%s, count:%d\n", attrtest3[i], result2.count);
			reterr++;
			return -8;
		}

		memlink_result_free(&result2);
	}
	
	memlink_destroy(m);

	return reterr;
}
