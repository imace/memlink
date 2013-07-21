#include <stdio.h>
#include <stdlib.h>
#include <memlink_client.h>
#include "logfile.h"
#include "test.h"

int check_attr(MemLink *m, char *name, char *key, char *newattr)
{
	MemLinkResult	result;
	int				ret;

	ret = memlink_cmd_range(m, name, key, MEMLINK_VALUE_VISIBLE, "::", 0, 1, &result);
	if (ret != MEMLINK_OK) {
		DERROR("range error, key:%s, ret:%d\n", key, ret);
		return -5;
	}

	MemLinkItem	*item = result.items;

	if (NULL == item) {
		DERROR("range not return data, error!, key:%s\n", key);
		return -6;
	}
	if (strcmp(item->attr, newattr) != 0) {
		DERROR("attr set error, item->attr:%s, newattr:%s\n", item->attr, newattr);	
		return -7;
	}
	
	memlink_result_free(&result);

	return 0;
}


int main()
{
	MemLink	*m;
#ifdef DEBUG
	//logfile_create("stdout", 3);
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

	ret = memlink_cmd_create_table_list(m, name, 6, "4:3:1");
	if (ret != MEMLINK_OK) {
		DERROR("1 memlink_cmd_create %s error: %d\n", name, ret);
		return -2;
	}

	//sprintf(key, "%s.haha", name);
    strcpy(key, "haha");

	char *val	  = "111111";
	char *attrstr = "8:3:1";
	int i;
	char value[64];
	for (i = 0; i < 20; i++)
	{
		sprintf(value, "%06d", i);
		ret = memlink_cmd_insert(m, name, key, val, strlen(val), attrstr, 0);
		if (ret != MEMLINK_OK) {
			DERROR("insert error, key:%s, val:%s, attr:%s, i:%d, ret:%d\n", key, val, attrstr, 1, ret);
			return -3;
		}
	}
	
    // added by wyx 
	attrstr = "1:1:1";
	for (i = 0; i < 20; i++)
	{
		sprintf(value, "%06d", i);
		ret = memlink_cmd_attr(m, name, key, "xxxx", 4, attrstr);
		if (ret != MEMLINK_ERR_NOVAL) {
			DERROR("attr error, must novalue, key:%s, val:%s, ret:%d\n", key, "xxxx", ret);
			return -4;
		}
	}

	ret = memlink_cmd_attr(m, name, key, "xxxx", 4, attrstr);
	if (ret != MEMLINK_ERR_NOVAL) {
		DERROR("attr error, must novalue, key:%s, val:%s, ret:%d\n", key, "xxxx", ret);
		return -4;
	}
    ret = memlink_cmd_attr(m, "", "xxxxxx", "xxxx", 4, attrstr);
	if (ret != MEMLINK_ERR_PARAM) {
		DERROR("attr error, must nokey, ret:%d\n", ret);
		return -4;
	}
    ret = memlink_cmd_attr(m, "test", "xxxxxx", "xxxx", 4, attrstr);
	if (ret != MEMLINK_ERR_NOKEY) {
		DERROR("attr error, must nokey, ret:%d\n", ret);
		return -4;
	}

    // end

	char *newattr  = "7:2:1";
	ret = memlink_cmd_attr(m, name, key, val, strlen(val), newattr);
	if (ret != MEMLINK_OK) {
		DERROR("attr error, key:%s, val:%s, attr:%s, ret:%d\n", key, val, newattr, ret);
		return -4;
	}
	check_attr(m, name, key, newattr);

	newattr  = "7:2:0";
	ret = memlink_cmd_attr(m, name, key, val, strlen(val), newattr);
	if (ret != MEMLINK_OK) {
		DERROR("attr error, key:%s, val:%s, attr:%s, ret:%d\n", key, val, newattr, ret);
		return -4;
	}
	check_attr(m, name, key, newattr);

	memlink_destroy(m);

	return 0;
}

