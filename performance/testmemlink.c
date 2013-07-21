#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>
#include <getopt.h>
#include <memlink_client.h>
#include "logfile.h"
#include "utils.h"
#include "testframe.h"

#define VALUE_SIZE		12
#define INSERT_POS		0


int clear_key(char *key)
{
	MemLink	*m;
    
    m = memlink_create("127.0.0.1", 11001, 11002, 30);
	if (NULL == m) {
		DERROR("memlink_create error!\n");
		return -1;
	}

	int  ret;
	//char key[32];
	//sprintf(key, "haha");

    ret = memlink_cmd_rmkey(m, key);
    if (ret != MEMLINK_OK) {
        DERROR("rmkey error! ret:%d\n", ret);
        return -1;
    }
    
    MemLinkStat     stat;
    ret = memlink_cmd_stat(m, key, &stat);
    if (ret != MEMLINK_ERR_NOKEY) {
        DERROR("stat result error! ret:%d\n", ret);
        return -2;
    }

    memlink_destroy(m);
    return 0;
}

int create_key(char *key)
{
	MemLink	*m;
    
    m = memlink_create("127.0.0.1", 11001, 11002, 30);
	if (NULL == m) {
		DERROR("memlink_create error!\n");
		return -1;
	}

	int  ret;
    ret = memlink_cmd_create_list(m, key, VALUE_SIZE, "4:3:1");
    if (ret != MEMLINK_OK) {
        DERROR("create list error! ret:%d\n", ret);
        return -1;
    }
    
    memlink_destroy(m);

    return 0;
}


int test_insert_long(int count, int docreate)
{
	MemLink	*m;
	struct timeval start, end;

	gettimeofday(&start, NULL);
	m = memlink_create("127.0.0.1", 11001, 11002, 30);
	if (NULL == m) {
		DERROR("memlink_create error!\n");
		return -1;
	}

	int  ret;
	char key[32];

	sprintf(key, "haha");
    if (docreate == 1) {
        ret = memlink_cmd_create_list(m, key, VALUE_SIZE, "4:3:1");

        if (ret != MEMLINK_OK) {
            DERROR("create list %s error: %d\n", key, ret);
            return -2;
        }
    }
	
	char val[64];
	char *maskstr = "6:2:1";

	int i;
	for (i = 0; i < count; i++) {
		sprintf(val, "%020d", i);
		ret = memlink_cmd_insert(m, key, val, strlen(val), maskstr, INSERT_POS);
		if (ret != MEMLINK_OK) {
			DERROR("insert error, i:%d, val:%s, ret:%d\n", i, val, ret);
			return -3;
		}
	}
	gettimeofday(&end, NULL);

    double speed = 0;
   
    if (g_tcf->isthread == 0) {
        unsigned int tmd = timediff(&start, &end);
        speed = ((double)count / tmd) * 1000000;
        DINFO("insert long use time: %u, speed: %.2f\n", tmd, speed);
    }

	memlink_destroy(m);

	return (int)speed;
}


int test_insert_short(int count, int docreate)
{
	MemLink	*m;
	struct timeval start, end;
	int iscreate = 0;

	int i, ret;
	char *maskstr = "6:2:1";
	char *key = "haha";

	gettimeofday(&start, NULL);
	for (i = 0; i < count; i++) {
		m = memlink_create("127.0.0.1", 11001, 11002, 30);
		if (NULL == m) {
			DERROR("memlink_create error!\n");
			return -1;
		}

		if (docreate == 1 && iscreate == 0) {
			ret = memlink_cmd_create_list(m, key, VALUE_SIZE, "4:3:1");

			if (ret != MEMLINK_OK) {
				DERROR("create list %s error: %d\n", key, ret);
				return -2;
			}
			iscreate = 1;
		}

		char val[64];

		sprintf(val, "%020d", i);
		ret = memlink_cmd_insert(m, key, val, strlen(val), maskstr, INSERT_POS);
		if (ret != MEMLINK_OK) {
			DERROR("insert error, i:%d, val:%s, ret:%d\n", i, val, ret);
			return -3;
		}
		memlink_destroy(m);
	}
	gettimeofday(&end, NULL);

    double speed = 0;
    if (g_tcf->isthread == 0) {
        unsigned int tmd = timediff(&start, &end);
        speed = ((double)count / tmd) * 1000000;
        DINFO("insert short use time: %u, speed: %.2f\n", tmd, speed);
    }
	
	return (int)speed;
}



int test_range_long(int frompos, int rlen, int count)
{
	MemLink	*m;
	struct timeval start, end;

	m = memlink_create("127.0.0.1", 11001, 11002, 30);
	if (NULL == m) {
		DERROR("memlink_create error!\n");
		return -1;
	}

	int  ret;
	char key[32];
	char val[32];

	sprintf(key, "haha");
	int i;

	gettimeofday(&start, NULL);
	for (i = 0; i < count; i++) {
		sprintf(val, "%020d", i);
		MemLinkResult	result;
		ret = memlink_cmd_range(m, key, MEMLINK_VALUE_VISIBLE, "", frompos, rlen, &result);
		if (ret != MEMLINK_OK) {
			DERROR("insert error, i:%d, val:%s, ret:%d\n", i, val, ret);
			return -3;
		}
		//DINFO("range count: %d\n", result.count);
		memlink_result_free(&result);
	}
	gettimeofday(&end, NULL);

    double speed = 0;

    if (g_tcf->isthread == 0) {
        unsigned int tmd = timediff(&start, &end);
        speed = ((double)count / tmd) * 1000000;
        DINFO("range long use time: %u, speed: %.2f\n", tmd, speed);
    }
	
	memlink_destroy(m);

	return (int)speed;
}

int test_range_short(int frompos, int rlen, int count)
{
	MemLink	*m;
	struct timeval start, end;
	int i, ret;
	char *key = "haha";

	gettimeofday(&start, NULL);

	for (i = 0; i < count; i++) {
		m = memlink_create("127.0.0.1", 11001, 11002, 30);
		if (NULL == m) {
			DERROR("memlink_create error!\n");
			return -1;
		}

		char val[32];

		sprintf(val, "%020d", i);
		MemLinkResult	result;
		ret = memlink_cmd_range(m, key, MEMLINK_VALUE_VISIBLE, "", frompos, rlen, &result);
		if (ret != MEMLINK_OK) {
			DERROR("insert error, i:%d, val:%s, ret:%d\n", i, val, ret);
			return -3;
		}
		//DINFO("range count: %d\n", result.count);
		memlink_result_free(&result);
		memlink_destroy(m);
	}
	gettimeofday(&end, NULL);

    double speed = 0;

    if (g_tcf->isthread == 0) {
        unsigned int tmd = timediff(&start, &end);
        speed = ((double)count / tmd) * 1000000;
        DINFO("range short use time: %u, speed: %.2f\n", tmd, speed);
    }

	return (int)speed;
}

int testconfig_init(TestConfig *tcf)
{
	tcf->ifuncs[0] = test_insert_long;
	tcf->ifuncs[1] = test_insert_short;
	tcf->rfuncs[0] = test_range_long;
	tcf->rfuncs[1] = test_range_short;

	return 0;
}

