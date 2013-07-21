#ifndef MEMLINK_INFO_H
#define MEMLINK_INFO_H

#include "common.h"
#include "conn.h"
#include <time.h>

#define	CONN_INFO_MEMBER \
	int  fd;\
	char client_ip[16];\
	int  port;\
	int  cmd_count;\
	struct timeval start;

typedef struct _conn_info
{
	CONN_INFO_MEMBER
}ConnInfo;

typedef struct _rw_conn_info
{
	CONN_INFO_MEMBER
}RwConnInfo;

typedef struct _sync_conn_info
{
	CONN_INFO_MEMBER

	int  logver;
	int  logline;
	int  delay;

	unsigned char status;
    char push_log_stop;
}SyncConnInfo;

int info_sys_stat(MemLinkStatSys *stat);
int info_read_conn(Conn *conn);
int info_write_conn(Conn *conn);
int info_sync_conn(Conn *conn);
int info_sys_config(Conn *conn);
int info_sys_config(Conn *conn);

#endif
