#ifndef MEMLINK_SYNCLOG_H
#define MEMLINK_SYNCLOG_H

#include <stdio.h>
#include <limits.h>

#define SYNCLOG_NAME "bin.log"
/**
 * header and index area are mapped in memory address space.
 */
typedef struct _synclog
{
    char    filename[PATH_MAX]; // file path
    int     fd;                 // open file descriptor
    char    *index;             // mmap addr
    int     len;                // mmap len
    unsigned int    index_pos;  // last index pos
    unsigned int    pos;        // last write data pos
    unsigned int    version;
}SyncLog;

SyncLog*    synclog_create();
SyncLog*    synclog_open(char *filename);
int         synclog_new(SyncLog *slog);
int         synclog_validate(SyncLog *slog);
int         synclog_write(SyncLog *slog, char *data, int datalen);
void        synclog_close(SyncLog *slog);
void        synclog_destroy(SyncLog *slog);
int         synclog_rotate(SyncLog *slog);
int			synclog_version(SyncLog *slog, unsigned int *logver);
int         synclog_lastlog();
int			synclog_prevlog(int curid);
int			synclog_scan_binlog(int *result, int rsize);
int         synclog_truncate(SyncLog *slog, unsigned int logver, unsigned int dumplogpos);
int         synclog_resize(unsigned int logver, unsigned int logline);
int         synclog_clean(unsigned int logver, unsigned int dumplogpos);
void		synclog_sync_disk(int fd, short event, void *arg);
int         synclog_read_data(char *binlogname, int fromline, int toline, char *md5str);
int         synclog_reset(char *binlogname, int fromline, int toline);

#endif
