#ifndef __TESTFRAME_H__
#define __TESTFRAME_H__

#define TEST_THREAD_NUM 10

#define TESTN			4
#define INSERT_TESTS	4
#define RANGE_TESTS		8
#define RANGEN			3

#define MAX_TEST_NUM	128

typedef int (*insert_func)(int, int);
typedef int (*range_func)(int, int, int);

typedef struct {
	int isthread;
	int threads;
	int frompos;
	int len;
	int count;
	int longcon;
	int isalltest;
	int testnum[MAX_TEST_NUM];
	int testnum_count;

	int insert_test_count;
	int range_test_count;

	int range_test_range[MAX_TEST_NUM];
	int range_test_range_count;

	int range_sample_count;  // default 1000

	insert_func	ifuncs[2];
	range_func	rfuncs[2];
}TestConfig;

typedef struct {
	int		type;
	void	*func;
	int		startpos;
	int		slen;
	int		count;
}ThreadArgs;

extern TestConfig *g_tcf;

int testconfig_init(TestConfig *tcf);

#endif
