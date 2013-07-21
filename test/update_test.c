#include <stdio.h>
#include <stdlib.h>
#include <memlink_client.h>
#include "logfile.h"
#include "test.h"

int main()
{
	MemLink	*m;
#ifdef DEBUG
	logfile_create("stdout", 3);
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
		DERROR("1 memlink_cmd_create %s error: %d\n", name, ret);
		return -2;
	}

	int i;
	char val[64];
	char *attrstr = "6:3:1";
    int  insertnum = 20;

	for (i = 0; i < insertnum; i++) {
		sprintf(val, "%06d", i);
		ret = memlink_cmd_insert(m, name, key, val, strlen(val), attrstr, i);
		if (ret != MEMLINK_OK) {
			DERROR("insert error, key:%s,val:%s, ret:%d\n", key, val, ret);
			return -3;
		}
	}

//add by wyx
	ret = memlink_cmd_move(m, name, key, val, -1, -10);
	if (ret == MEMLINK_OK) {
		DERROR("move error, must novalue, key:%s, val:%s, ret:%d\n", key, "xxxx", ret);
		return -4;
	}

	ret = memlink_cmd_move(m, name, key, "xxxx", 4, 0);
	if (ret != MEMLINK_ERR_NOVAL) {
		DERROR("move error, must novalue, key:%s, val:%s, ret:%d\n", key, "xxxx", ret);
		return -4;
	}
    ret = memlink_cmd_move(m, "test", "xxxxxx", "xxxx", 4, 0);
	if (ret != MEMLINK_ERR_NOKEY) {
		DERROR("move error, must nokey, key:%s\n", key);
		return -4;
	}
//end
	for (i = 0; i < insertnum; i++) {
		sprintf(val, "%06d", i);
        //DINFO("====== insert i:%d\n", i);
		ret = memlink_cmd_move(m, name, key, val, strlen(val), 0);
		if (ret != MEMLINK_OK) {
			DERROR("move error, key:%s, val:%s, ret:%d, i:%d\n", key, val, ret, i);
			return -4;
		}
		else
			DINFO("move ok, key:%s, val:%s, ret:%d, i:%d\n", key, val, ret, i);

		MemLinkResult result;

		ret = memlink_cmd_range(m, name, key, MEMLINK_VALUE_VISIBLE, "", 0, 1, &result);
		if (ret != MEMLINK_OK) {
			DERROR("range error, key:%s, ret:%d\n", key, ret);
			return -5;
		}
		MemLinkItem *item = result.items;
			
		if (item == NULL) {
			DERROR("item is null, key:%s\n", key);
			return -6;
		}
        //DINFO("item value: %s\n", item->value);
		if (strcmp(item->value, val) != 0) {
			DERROR("after move, first line error! item->value:%s value:%s\n", item->value, val);
			return -7;
		}

		memlink_result_free(&result);
	}

    MemLinkStat     stat;

    ret = memlink_cmd_stat(m, name, key, &stat);
    if (ret != MEMLINK_OK) {
        DERROR("stat error, key:%s, ret:%d\n", key, ret);
        return -8;
    }

	
	memlink_destroy(m);

	return 0;
}
