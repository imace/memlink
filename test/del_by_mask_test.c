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
		DINFO("memlink_create error!\n");
		return -1;
	}
	
	int  ret;
	char key[64];
    char *name = "test";

	ret = memlink_cmd_create_table_list(m, name, 6, "4:3:2");
	if (ret != MEMLINK_OK) {
		DINFO("memlink_cmd_create %s error: %d\n", name, ret);
		return -2;
	}

	int i;
	char *attrstr = "8:3:1";
	char val[64];

	//sprintf(key, "%s.haha", name);
    strcpy(key, "haha");

	for (i = 0; i < 100; i++) {
		sprintf(val, "%06d", i);
		ret = memlink_cmd_insert(m, name, key, val, strlen(val), attrstr, i);
		if (ret != MEMLINK_OK) {
			DINFO("insert error, key:%s, value:%s, attr:%s, i:%d\n", key, val, attrstr, i);
			return -3;
		}
	}
	//DINFO("insert 100!\n");

	ret = memlink_cmd_del_by_attr(m, name, key, ":3:1");	
	if (ret < 0) {
		DERROR("del_by_attr key:%s, ret:%d\n", key, ret);
		return -4;
	}
	//DINFO("del_by_attr %s!\n", ":3:1");

	MemLinkResult rs;
	int frompos = 0;
	int len = 100;
	
	ret = memlink_cmd_range(m, name, key, MEMLINK_VALUE_VISIBLE,  "", frompos, len, &rs);
	if (ret != MEMLINK_OK) {
		DERROR("range error, key:%s, attr:%s\n", key, "");
		return -3;
	}
	if (rs.count != 0) {
		DERROR("error! rs.count must be 0! rs.count:%d\n", rs.count);
		return -1;
	}
	
	MemLinkStat st;
	ret = memlink_cmd_stat(m, name, key, &st);
	if (ret != MEMLINK_OK) {
		DERROR("stat err key:%s, ret:%d\n", key, ret);
		return -1;
	}

	if (st.data_used != 0){
		DERROR("error data_used must be 0, st.data_used:%d\n", st.data_used);
		return -1;
	}

    name = "test2"; 
	//sprintf(key, "%s.hehe", name);
	ret = memlink_cmd_create_table_list(m, name, 6, "4:3:2");
	if (ret != MEMLINK_OK) {
		DERROR("memlink_cmd_create %s error: %d\n", name, ret);
		return -2;
	}
	
	char* attrstr1[] = {"8::1", "7:2:1", "6:2:1", "3:3:3"};
	char* valarray[] = {"111111", "222222", "333333", "444444"};
	int num = 100;

	for (i = 0; i < num; i++) {
		int k = i%4;
		ret = memlink_cmd_insert(m, name, key, valarray[k], 6, attrstr1[k], i);
		if (ret != MEMLINK_OK) {
			DERROR("insert error, key:%s, value:%s, attr:%s, i:%d\n", key, valarray[k], attrstr, i);
			return -3;
		}
	}
	//DINFO("insert %d!\n", num);

	int spared = num;	
	for (i = 0; i < 4; i++) {
		int k = i % 4;
		int frompos = 0;
		int len = 100;

		ret = memlink_cmd_del_by_attr(m, name, key, attrstr1[k]);	
		if (ret < 0) {
			DERROR("del_by_attr key:%s, ret:%d\n", key, ret);
			return -4;
		}
		
		ret = memlink_cmd_range(m, name, key, MEMLINK_VALUE_VISIBLE,  attrstr1[k], frompos, len, &rs);
		if (ret != MEMLINK_OK) {
			DERROR("range error, key:%s, attr:%s\n", key, attrstr1[k]);
			return -3;
		}
		//MemLinkItem* item = rs.root;
		if (rs.count != 0) {
			DERROR("err! rs.count must be 0! rs.count:%d\n", rs.count);
			return -1;
		}

		MemLinkStat st;
		ret = memlink_cmd_stat(m, name, key, &st);
		if (ret != MEMLINK_OK) {
			DERROR("stat err key:%s, ret:%d\n", key, ret);
			return -1;
		}

		spared -= num/4;
		if (st.data_used != spared) {
			DERROR("st.data_used:%d\n", st.data_used);
			return -1;
		}
		/*
		while(item != NULL)
		{
			if(strcmp(item->value, valarray[k]) == 0)
			{
				DINFO("no item->value:%s\n", item->value);
				return -1;
			}
		}*/
	}
    //goto memlink_over;
	
memlink_over:
	memlink_destroy(m);

	return 0;
}
