#ifndef _MEMLINK_MB_H
#define _MEMLINK_MB_H

#include "conn.h"
#include "wthread.h"

int    mb_data_ready(Conn *conn, char *data, int datalen);
int    mb_data_wrote(Conn *conn);
void   mb_conn_destroy(int fd, short event, void *arg);
int    mb_reconn(BackupItem *bitem);
int    master_ready(Conn *conn, char *data, int datalen);
int    mb_connect(WThread *wt);
void   mb_send_timeout(int fd, short event, void *arg);
int    master_init(int voteid, char *data, int datalen);
MBconn *master_connect_backup(BackupItem *bitem);
int    switch_master(int voteid, char *data, int datalen);
int    mb_binfo_update(int voteid, char *data, int datalen);
void   mb_conn_destroy_delay(Conn *conn);

#endif
