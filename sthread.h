#ifndef MEMLINK_STHREAD_H
#define MEMLINK_STHREAD_H

#include "conn.h"
#include <limits.h>
#include "synclog.h"
#include "info.h"

#define NOT_SEND        0
#define SEND_LOG        1
#define SEND_DUMP       2


typedef struct _sthread
{
	int    sock;
	struct event_base *base;
	struct event event;
	int    conn_sync;
	unsigned short conns;
    char   stop;
    int    push_stop_nums;
	SyncConnInfo *sync_conn_info;
}SThread;

typedef struct __syncconn
{
	CONN_MEMBER

	struct event sync_write_evt;
	struct event sync_check_interval_evt;
	struct event sync_read_evt;

	struct timeval interval;
	
	unsigned char status;
	unsigned char cmd;

	SyncLog *synclog;
	int dump_fd;
    //int bpos;
    int blogver;
    int blogline;
    char need_skip_one;
}SyncConn;

SThread *sthread_create();
void sthread_destroy(SThread *st);

int sdata_ready(Conn *conn, char *data, int datalen);
int check_binlog_local(SyncConn *conn, unsigned int log_ver, unsigned int log_line);
void sync_conn_destroy(Conn *conn);

#endif
