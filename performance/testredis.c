#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "hiredis.h"
#include "testframe.h"
#include "logfile.h"

//#define VALUE_SIZE		12
/*
unsigned int timediff(struct timeval *start, struct timeval *end)
{
    return 1000000 * (end->tv_sec - start->tv_sec) + (end->tv_usec - start->tv_usec);
}*/

int clear_key(char *key)
{
	int fd;
	redisReply *ret;

	ret = redisConnect(&fd, "127.0.0.1", 6379);
	if (NULL != ret) {
		DINFO("connect error! %s\n", ret->reply);
		return -1;
	}
	
	ret = redisCommand(fd, "DEL %s", key);
	if (ret->type != REDIS_REPLY_INTEGER) {
		DINFO("DEL %s error! ret: %d, %s\n", key, ret->type, ret->reply);
		return -2;
	}
	freeReplyObject(ret);
	
	close(fd);
	
	return 0;
}

int create_key(char *key)
{
	return 0;	
}

int test_insert_long(int count, int docreate)
{
	int		fd;
	struct timeval start, end;
	redisReply *ret;

	ret = redisConnect(&fd, "127.0.0.1", 6379);
	if (NULL != ret) {
		DINFO("connect error! %s\n", ret->reply);
		return -1;
	}

	int  i;
	char *key = "haha";
	char val[32];

	gettimeofday(&start, NULL);	
	for (i = 0; i < count; i++) {
		sprintf(val, "%012d", i);
		//DINFO("lpush %s, %s\n", key, val);
		ret = redisCommand(fd, "LPUSH %s %s", key, val);
		if (ret->type != REDIS_REPLY_INTEGER) {
			DINFO("lpush %s error! ret: %d, %s\n", val, ret->type, ret->reply);
			return -2;
		}
		freeReplyObject(ret);
	}
	gettimeofday(&end, NULL);	

	unsigned int tmd = timediff(&start, &end);
	double speed = ((double)count / tmd) * 1000000;
	DINFO("insert long use time: %u, speed: %.2f\n", tmd, speed);
	
	close(fd);
	return (int)speed;
}

int test_insert_short(int count, int docreate)
{
	int		fd;
	struct timeval start, end;
	redisReply *ret;
	int  i;
	char *key = "haha";
	char val[32];

	gettimeofday(&start, NULL);	

	for (i = 0; i < count; i++) {
		ret = redisConnect(&fd, "127.0.0.1", 6379);
		if (NULL != ret) {
			DINFO("connect error! %s\n", ret->reply);
			return -1;
		}

		sprintf(val, "%012d", i);
		//DINFO("lpush %s, %s\n", key, val);
		ret = redisCommand(fd, "LPUSH %s %s", key, val);
		if (ret->type != REDIS_REPLY_INTEGER) {
			DINFO("lpush %s error! ret: %d, %s\n", val, ret->type, ret->reply);
			return -2;
		}
		freeReplyObject(ret);
		close(fd);
	}
	gettimeofday(&end, NULL);	

	unsigned int tmd = timediff(&start, &end);
	double speed = ((double)count / tmd) * 1000000;
	DINFO("insert short use time: %u, speed: %.2f\n", tmd, speed);
	
	return (int)speed;
}


int test_range_long(int frompos, int slen, int count)
{
	int		fd;	
	int		endpos = frompos + slen;
	struct timeval start, end;
	redisReply *ret;

	ret = redisConnect(&fd, "127.0.0.1", 6379);
	if (NULL != ret) {
		DINFO("connect error! %s\n", ret->reply);
		return -1;
	}
	int  i;
	char *key = "haha";
	char val[32];
	char buf[128];

	gettimeofday(&start, NULL);	
	for (i = 0; i < count; i++) {
		sprintf(buf, "LRANGE %s %d %d", key, frompos, endpos - 1);
		//ret = redisCommand(fd, "LRANGE %s %d %d", key, frompos, slen);
		ret = redisCommand(fd, buf);
		if (ret->type != REDIS_REPLY_ARRAY) {
			DINFO("lrange error! %d, %s\n", ret->type, ret->reply);
			return -2;
		}
		if (ret->elements != slen) {
			DERROR("lrange count error: %d, len:%d\n", ret->elements, slen);
		}
		freeReplyObject(ret);
	}
	gettimeofday(&end, NULL);	

	unsigned int tmd = timediff(&start, &end);
	double speed = ((double)count / tmd) * 1000000;
	DINFO("range long use time: %u, speed: %.2f\n", tmd, speed);
	
	close(fd);
	return (int)speed;
}

int test_range_short(int frompos, int slen, int count)
{
	struct timeval start, end;
	redisReply *ret;
	int		endpos = frompos + slen;
	int		fd;	
	int		i;
	char	*key = "haha";
	char	val[32];
	char	buf[128];

	gettimeofday(&start, NULL);	

	for (i = 0; i < count; i++) {
		ret = redisConnect(&fd, "127.0.0.1", 6379);
		if (NULL != ret) {
			DINFO("connect error! %s\n", ret->reply);
			return -1;
		}

		sprintf(buf, "LRANGE %s %d %d", key, frompos, endpos - 1);
		//ret = redisCommand(fd, "LRANGE %s %d %d", key, frompos, slen);
		ret = redisCommand(fd, buf);
		if (ret->type != REDIS_REPLY_ARRAY) {
			DINFO("lrange error! %d, %s\n", ret->type, ret->reply);
			return -2;
		}
		if (ret->elements != slen) {
			DERROR("lrange count error: %d, len:%d\n", ret->elements, slen);
		}

		freeReplyObject(ret);
		close(fd);
	}
	gettimeofday(&end, NULL);	

	unsigned int tmd = timediff(&start, &end);
	double speed = ((double)count / tmd) * 1000000;
	DINFO("range short use time: %u, speed: %.2f\n", tmd, speed);
	
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


/*
int main(int argc, char *argv[])
{
	if (argc != 2) {
		DINFO("usage: testredis count\n");
		return 0;
	}
	int count = atoi(argv[1]);
	insert_test_long(count);

	return 0;
}
*/


