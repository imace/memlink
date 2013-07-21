#ifndef MEMLINK_COMMITLOG_H
#define MEMLINK_COMMITLOG_H

#include <stdio.h>
#include <limits.h>
#include <stdint.h>

#define COMMITLOG_SIZE           1024 * 1024 + sizeof(uint64_t) * 3

typedef struct _commitlog
{
    char filename[PATH_MAX];
    int  fd;
    char *data;
    int  len;
	int	 state; // COMMITTED/ROLLBACKED/INCOMPLETE
}CommitLog;

CommitLog*  commitlog_create();
int         commitlog_read(CommitLog *clog, int *lastlogver, int *lastlogline);
int         commitlog_write(CommitLog *clog, int logver, int logline, char *cmd, int cmdlen);
int         commitlog_write_data(CommitLog *clog, char *data, int datalen);
char*		commitlog_get_cmd(CommitLog *clog, int *cmdlen);

#endif
