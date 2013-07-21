#ifndef MEMLINK_BACKUP_H
#define MEMLINK_BACKUP_H

#include <stdio.h>
#include "conn.h"
#include "commitlog.h"

typedef struct _backup_sync_conn
{
	CONN_MEMBER
}BackupSyncConn;

int		backup_init();
int		backup_ready(Conn *conn, char *data, int datalen);
int     switch_backup(int voteid, char *data, int datalen);

#endif
