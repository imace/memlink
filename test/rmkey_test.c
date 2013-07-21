#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
		DERROR("create %s error: %d\n", key, ret);
		return -2;
	}

    ret = memlink_cmd_rmkey(m, name, key);
    if (ret != MEMLINK_ERR_NOKEY) {
        DERROR("rmkey error, key:%s, ret:%d\n", key, ret);
        return -3;
    }

    ret = memlink_cmd_create_node(m, name, "haha");
    if (ret != MEMLINK_OK) {
        DERROR("create node error:%d\n", ret);
        return -1;
    }

    ret = memlink_cmd_rmkey(m, name, key);
    if (ret != MEMLINK_OK) {
        DERROR("rmkey error, key:%s, ret:%d\n", key, ret);
        return -3;
    }

	ret = memlink_cmd_create_node(m, name, "haha");
	if (ret != MEMLINK_OK) {
		DERROR("create %s error: %d\n", key, ret);
		return -4;
	}

	memlink_destroy(m);

	return 0;
}

