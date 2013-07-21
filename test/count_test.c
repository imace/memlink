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
	char key[64];
    char *name = "test";	

	ret = memlink_cmd_create_table_list(m, name, 6, "4:3:2");
	if (ret != MEMLINK_OK) {
		DERROR("memlink_cmd_create %s error: %d\n", name, ret);
		return -2;
	}
    strcpy(key, "haha");
	//sprintf(key, "%s.haha", name);
	int i;
	//char* maskstr[] = {"8:3:1", "7:3:1"};
	char* maskstr = "8:3:1";
	char val[64];
    int  insertnum = 100;

	for (i = 0; i < insertnum; i++) {
		sprintf(val, "%06d", i);

		int k = i%2;
		ret = memlink_cmd_insert(m, name, key, val, strlen(val), maskstr, i);
		if (ret != MEMLINK_OK) {
			DERROR("insert error, key:%s, value:%s, mask:%s, i:%d\n", key, val, maskstr, i);
			return -3;
		}
	}

	MemLinkCount    count;
	ret = memlink_cmd_count(m, name, "kkkk", "", &count); //count不存在的key
    if (ret == MEMLINK_OK) {
        DERROR("must not count not exist key, ret:%d\n", ret);
        return -4;
    }

    ret = memlink_cmd_count(m, name, key, "", &count);  //count key的所有条目
    if (ret != MEMLINK_OK) {
        DERROR("count error, ret:%d\n", ret);
        return -4;
    }
    if (count.visible_count != insertnum) {
        DERROR("count visible_count error: %d\n", count.visible_count);
        return -5;
    }
    if (count.tagdel_count != 0) {
        DERROR("count tagdel_count error: %d\n", count.tagdel_count);
        return -5;
    }
/////////////////////added by wyx
    MemLinkCount    count1; //count key 的 mask 为 "8:3:1" 条目数
	ret = memlink_cmd_count(m, name, key, "8:3:1", &count1); 
    if (ret != MEMLINK_OK) { //err: memlink 挂掉
        DERROR("count error, ret:%d\n", ret);
        return -4;
    }
    if (count1.visible_count != insertnum) {
        printf("count1 visible_count error: %d\n", count1.visible_count);
        return -5;
    }
//end

//////////////////// TAG_DEL 10个
    int tagcount = 10;
	for (i = 0; i < tagcount; i++) {
		sprintf(val, "%06d", i*2);
		
		ret = memlink_cmd_tag(m, name, key, val, strlen(val), MEMLINK_TAG_DEL);		
		//DINFO("DEL  key:%s, val:%s\n", key, val);
		if (ret != MEMLINK_OK) {
			DERROR("del error, key:%s, val:%s\n", key, val);
			return -5;
		}

        MemLinkCount    count2;
        ret = memlink_cmd_count(m, name, key, "", &count2);
        if (ret != MEMLINK_OK) {
            DERROR("count error, ret:%d\n", ret);
            return -4;
        }
        if (count2.visible_count != insertnum - i - 1) {
            DERROR("count visible_count error: %d\n", count2.visible_count);
            return -5;
        }
        if (count2.tagdel_count != i + 1) {
            DERROR("count tagdel_count error: %d\n", count2.tagdel_count);
            return -5;
        }
	}
//////////////////////// TAG_RESTORE 前面del的条目 added by wyx
	for (i = 0; i < tagcount; i++) {
		sprintf(val, "%06d", i*2);
		
		ret = memlink_cmd_tag(m, name, key, val, strlen(val), MEMLINK_TAG_RESTORE);		
		//DINFO("RESTORE  key:%s, val:%s\n", key, val);
		if (ret != MEMLINK_OK) {
			DERROR("del error, key:%s, val:%s\n", key, val);
			return -5;
		}
	
	    MemLinkCount    count3;
	    ret = memlink_cmd_count(m, name, key, "", &count3);
	    if (ret != MEMLINK_OK) {
	        DERROR("count error, ret:%d\n", ret);
	        return -4;
	    }
	    if (count3.visible_count != insertnum + i - tagcount + 1) {
	        DERROR("count visible_count error: %d\n", count3.visible_count);
	        return -5;
	    }
	    if (count3.tagdel_count != tagcount - i - 1) {
	        DERROR("count tagdel_count error: %d\n", count3.tagdel_count);
	        return -5;
	    }
	}
//////////////// end 

/////
	for (i = 0; i < tagcount; i++) {
		sprintf(val, "%06d", i*2);
		
		ret = memlink_cmd_tag(m, name, key, val, strlen(val), MEMLINK_TAG_DEL);
		if (ret != MEMLINK_OK) {
			DERROR("del error, key:%s, val:%s\n", key, val);
			return -5;
		}
	}

	for (i = 50; i < 60; i++) {
		sprintf(val, "%06d", i);
		
		ret = memlink_cmd_del(m, name, key, val, strlen(val));
		if (ret != MEMLINK_OK) {
			DERROR("del error, key:%s, val:%s\n", key, val);
			return -5;
		}

        MemLinkCount    count2;
        ret = memlink_cmd_count(m, name, key, "", &count2);
        if (ret != MEMLINK_OK) {
            DERROR("count error, ret:%d\n", ret);
            return -4;
        }
        if (count2.visible_count != insertnum - tagcount - i - 1 + 50) {
            DERROR("count visible_count error, i:%d, count2.visible_count:%d, v:%d\n", i, count2.visible_count, insertnum - tagcount - i -1);
            return -5;
        }
        if (count2.tagdel_count != tagcount) {
            DERROR("count tagdel_count error: %d\n", count2.tagdel_count);
            return -5;
        }
	}

	memlink_destroy(m);

	return 0;
}

