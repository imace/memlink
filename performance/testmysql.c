#include <stdio.h>
#include <stdlib.h>
#include <mysql.h>
#include "testframe.h"
#include "logfile.h"

#define MYSQL_HOST	"127.0.0.1"
#define MYSQL_USER	"root"
#define MYSQL_PASS	""
#define MYSQL_DB	"mytest"

/*
unsigned int timediff(struct timeval *start, struct timeval *end)
{
    return 1000000 * (end->tv_sec - start->tv_sec) + (end->tv_usec - start->tv_usec);
}
*/

int clear_key(char *key)
{
	MYSQL	mysql;
	struct timeval start, end;

	mysql_init(&mysql);

	if (!mysql_real_connect(&mysql, MYSQL_HOST, MYSQL_USER, MYSQL_PASS, MYSQL_DB, 0, NULL, 0)) {
		DERROR("mysql connect error! %s\n", mysql_error(&mysql));
		return -1;
	}
	
	int  ret;
	char sql[1024];
	int  sqllen;
	//char sql_format

	//char *sql_format = "insert into ThreadList values (1, '%12d', 0, now())";
	sqllen = sprintf(sql, "delete from ThreadList where forumid=1");

	ret = mysql_real_query(&mysql, sql, sqllen);
	if (ret != 0) {
		DERROR("mysql query error: %s\n", mysql_error(&mysql));
		return -2;
	}

	mysql_close(&mysql);

	return 0;
}

int create_key(char *key)
{

	return 0;
}

int test_insert_long(int count, int docreate)
{
	MYSQL	mysql;
	struct timeval start, end;

	mysql_init(&mysql);

	if (!mysql_real_connect(&mysql, MYSQL_HOST, MYSQL_USER, MYSQL_PASS, MYSQL_DB, 0, NULL, 0)) {
		DERROR("mysql connect error! %s\n", mysql_error(&mysql));
		return -1;
	}
	
	int  i;
	int  ret;
	char *key = "haha";
	char val[32];
	char sql[1024];
	int  sqllen;
	//char sql_format

	char *sql_format = "insert into ThreadList values (1, '%12d', 0, now())";

	gettimeofday(&start, NULL);	
	for (i = 0; i < count; i++) {
		sqllen = snprintf(sql, 1024, sql_format, i);
		ret = mysql_real_query(&mysql, sql, sqllen);
		if (ret != 0) {
			DERROR("mysql query error: %s\n", mysql_error(&mysql));
			return -2;
		}
	}
	gettimeofday(&end, NULL);	

	unsigned int tmd = timediff(&start, &end);
	double speed = ((double)count / tmd) * 1000000;
	DINFO("insert long use time: %u, speed: %.2f\n", tmd, speed);
	

	mysql_close(&mysql);

	return (int)speed;
}


int test_insert_short(int count, int docreate)
{
	MYSQL	mysql;
	struct timeval start, end;

	int  i;
	int  ret;
	char *key = "haha";
	char val[32];
	char sql[1024];
	int  sqllen;
	//char sql_format

	char *sql_format = "insert into ThreadList values (1, '%12d', 0, now())";

	gettimeofday(&start, NULL);	

	for (i = 0; i < count; i++) {
		mysql_init(&mysql);

		if (!mysql_real_connect(&mysql, MYSQL_HOST, MYSQL_USER, MYSQL_PASS, MYSQL_DB, 0, NULL, 0)) {
			DERROR("mysql connect error! %s\n", mysql_error(&mysql));
			return -1;
		}
		sqllen = snprintf(sql, 1024, sql_format, i);
		ret = mysql_real_query(&mysql, sql, sqllen);
		if (ret != 0) {
			DERROR("mysql query error: %s\n", mysql_error(&mysql));
			return -2;
		}

		mysql_close(&mysql);
	}
	gettimeofday(&end, NULL);	

	unsigned int tmd = timediff(&start, &end);
	double speed = ((double)count / tmd) * 1000000;
	DINFO("insert short use time: %u, speed: %.2f\n", tmd, speed);
	


	return (int)speed;

	return 0;
}

int test_range_long(int frompos, int slen, int count)
{
	MYSQL		mysql;
	MYSQL_RES	*result;
	struct timeval start, end;

	DINFO("count: %d\n", count);
	mysql_init(&mysql);

	if (!mysql_real_connect(&mysql, MYSQL_HOST, MYSQL_USER, MYSQL_PASS, MYSQL_DB, 0, NULL, 0)) {
		DERROR("mysql connect error! %s\n", mysql_error(&mysql));
		return -1;
	}
	
	int  i;
	int  ret;
	char *key = "haha";
	char val[32];
	char sql[1024];
	int  sqllen;
	//char sql_format

	char *sql_format = "select threadid from  ThreadList where forumid=1 order by reply_time limit %d,%d";

	gettimeofday(&start, NULL);	
	for (i = 0; i < count; i++) {
		sqllen = snprintf(sql, 1024, sql_format, frompos, slen);
		//DINFO("sql: %s\n", sql);
		ret = mysql_real_query(&mysql, sql, sqllen);
		if (ret != 0) {
			DERROR("mysql query error: %s\n", mysql_error(&mysql));
			return -2;
		}
		result = mysql_store_result(&mysql);
		if (result == NULL) {
			DERROR("mysql result error: %s\n", mysql_error(&mysql));
			return -2;
		}
		if ((ret = mysql_num_rows(result)) != slen) {
			DERROR("mysql query count error: %d\n", ret);
			return -2;
		}
		mysql_free_result(result);
	}
	gettimeofday(&end, NULL);	

	unsigned int tmd = timediff(&start, &end);
	double speed = ((double)count / tmd) * 1000000;
	DINFO("range long use time: %u, speed: %.2f\n", tmd, speed);
	
	mysql_close(&mysql);

	return (int)speed;
}

int test_range_short(int frompos, int slen, int count)
{
	MYSQL		mysql;
	MYSQL_RES	*result;
	struct timeval start, end;
	int  i;
	int  ret;
	char *key = "haha";
	char val[32];
	char sql[1024];
	int  sqllen;
	//char sql_format

	char *sql_format = "select threadid from  ThreadList where forumid=1 order by reply_time limit %d,%d";

	gettimeofday(&start, NULL);	

	for (i = 0; i < count; i++) {
		mysql_init(&mysql);

		if (!mysql_real_connect(&mysql, MYSQL_HOST, MYSQL_USER, MYSQL_PASS, MYSQL_DB, 0, NULL, 0)) {
			DERROR("mysql connect error! %s\n", mysql_error(&mysql));
			return -1;
		}

		sqllen = snprintf(sql, 1024, sql_format, frompos, slen);
		//DINFO("sql: %s\n", sql);
		ret = mysql_real_query(&mysql, sql, sqllen);
		if (ret != 0) {
			DERROR("mysql query error: %s\n", mysql_error(&mysql));
			return -2;
		}
		result = mysql_store_result(&mysql);
		if (result == NULL) {
			DERROR("mysql result error: %s\n", mysql_error(&mysql));
			return -2;
		}
		if ((ret = mysql_num_rows(result)) != slen) {
			DERROR("mysql query count error: %d\n", ret);
			return -2;
		}
		mysql_free_result(result);

		mysql_close(&mysql);
	}
	gettimeofday(&end, NULL);	

	unsigned int tmd = timediff(&start, &end);
	double speed = ((double)count / tmd) * 1000000;
	DINFO("range shortuse time: %u, speed: %.2f\n", tmd, speed);

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


