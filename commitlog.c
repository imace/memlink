#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "commitlog.h"
#include "base/zzmalloc.h"
#include "base/logfile.h"
#include "synclog.h"
#include "mem.h"
#include "dumpfile.h"
#include "synclog.h"
#include "common.h"
#include "base/utils.h"
#include "runtime.h"
#include "myconfig.h"
#include "base/pack.h"

#define COMMITLOG_NAME     "commit.log"

CommitLog*
commitlog_create()
{
    CommitLog *clog;

    clog = (CommitLog *)zz_malloc(sizeof(CommitLog));
    if (NULL == clog) {
        DERROR("malloc CommitLog error!\n");
        MEMLINK_EXIT;
    }
    memset(clog, 0x0, sizeof(CommitLog));
    snprintf(clog->filename, PATH_MAX, "%s/%s", g_cf->datadir, COMMITLOG_NAME);
    DINFO("commitlog filename: %s\n", clog->filename);

    int havefile = isfile(clog->filename);

    clog->fd = open(clog->filename, O_RDWR | O_CREAT, 0644);
    if (clog->fd == -1) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("open clog %s error: %s\n", clog->filename,  errbuf);
        zz_free(clog);
        MEMLINK_EXIT;
    }
    
    truncate_file(clog->fd, COMMITLOG_SIZE);

    clog->data = mmap(NULL, COMMITLOG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, clog->fd, 0);
    if (clog->data == MAP_FAILED) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("clog mmap error %s\n",  errbuf);
        MEMLINK_EXIT;
    }

    if (!havefile) {
        // set lastver, lastline, lastret, logver, logline, cmdlen to 0
        memset(clog->data, 0, sizeof(int) * 6);
    }
    return clog;
}

int
commitlog_read(CommitLog *clog, int *lastlogver,  int *lastlogline)
{
    return unpack(clog->data, 0, "ii", lastlogver, lastlogline);
}

int
commitlog_write(CommitLog *clog, int logver, int logline, char *cmd, int cmdlen)
{
    int count = 0;
    int ret = pack(clog->data, 0, "ii", logver, logline);
    count += ret;
    
    DINFO("===============in commitlog_write cmdlen: %d\n", cmdlen);
    //int nlogver, nlogline;
    //unpack(clog->data, 0, "ii", &nlogver, &nlogline);
    //DINFO("===============in commitlog_write logver: %d, logline: %d\n", nlogver, nlogline);
    memcpy(clog->data+sizeof(int)*2, cmd, cmdlen);
    count += cmdlen;
    clog->len = count;
    DINFO("===============in commitlog_write len: %d\n", count);
    return ret;
}
int
commitlog_write_data(CommitLog *clog, char *data, int datalen)
{
    //memcpy(clog->data + sizeof(int) *2, data, datalen);
    memcpy(clog->data, data, datalen);
    clog->len = datalen;
    return datalen;
}

char*
commitlog_get_cmd(CommitLog *clog, int *cmdlen)
{
    int datalen;
    char* cmd = clog->data + sizeof(int)*2;
    //memcpy(cmdlen, cmd, sizeof(int));

    datalen = clog->len - sizeof(int)*2;
    memcpy(cmdlen, &datalen, sizeof(int));
    //memcpy(cmdlen, cmd, sizeof(int));
    
    return cmd;
}
