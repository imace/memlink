#ifndef MEMLINK_PERF
#define MEMLINK_PERF

#include <stdio.h>

#define THREAD_MAX_NUM  1000

typedef struct
{
    char    host[255];
    int     rport;
    int     wport;
    int     timeout;
    char    table[255];
    char    key[255];
    char    value[255];
    int     valuelen;
    int     valuesize; // create valuesize
    char    attrstr[255];
    int     from; // range from
    int     len;  // range len
    int     pos; // move pos
    int     popnum; // pop num
    unsigned int kind;  // range kind
    unsigned int tag; //tag
    int     testcount; // test count
    int     longconn; // is long conn
	int		tid; // thread seq id
}TestArgs;

typedef int (*TestFunc)(TestArgs *args);

typedef struct
{
    int         threads;
    TestFunc    func;
    TestArgs    *args;
}ThreadArgs;

typedef struct
{
    char        *name;
    TestFunc    func;
}TestFuncLink;

typedef struct
{
    int         threads;
    char        action[255];
    TestFunc    func;
    TestArgs    args;
}TestConfig;

#endif
