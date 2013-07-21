/**
 * 数据redo log
 * @file synclog.c
 * @author zhaowei
 * @ingroup memlink
 * @{
 */
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "base/logfile.h"
#include "myconfig.h"
#include "mem.h"
#include "dumpfile.h"
#include "synclog.h"
#include "base/zzmalloc.h"
#include "common.h"
#include "base/utils.h"
#include "base/md5.h"
#include "runtime.h"

SyncLog*
synclog_create()
{
    SyncLog *slog;

    slog = (SyncLog*)zz_malloc(sizeof(SyncLog));
    if (NULL == slog) {
        DERROR("malloc SyncLog error!\n");
        MEMLINK_EXIT;
        return NULL;
    }
    memset(slog, 0, sizeof(SyncLog));
    
    //slog->headlen = sizeof(short) + sizeof(int) + sizeof(int);

    snprintf(slog->filename, PATH_MAX, "%s/%s", g_cf->datadir, SYNCLOG_NAME);
    DINFO("synclog filename: %s\n", slog->filename);

    int ret;
    //struct stat stbuf;

    // check file exist
    /*
    ret = stat(slog->filename, &stbuf);
    char errbuf[1024];
    strerror_r(errno, errbuf, 1024);
    //DINFO("stat: %d, err: %s\n", ret,  errbuf);
    if (ret == -1 && errno == ENOENT) { // not found file, check last log id from disk filename
        DINFO("not found sync logfile, check version in disk.\n");
        unsigned int lastver = synclog_lastlog();
        DINFO("synclog_lastlog: %d\n", lastver);
        g_runtime->logver = lastver;
    }*/


    DINFO("try open sync logfile ...\n");
    //slog->fd = open(slog->filename, O_RDWR|O_CREAT|O_APPEND, 0644);
    slog->fd = open(slog->filename, O_RDWR|O_CREAT, 0644);
    if (slog->fd == -1) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("open synclog %s error: %s\n", slog->filename,  errbuf);
        zz_free(slog);
        MEMLINK_EXIT;
        return NULL;
    }

    //int len = slog->headlen + SYNCLOG_INDEXNUM * sizeof(int);
    int len = SYNCLOG_HEAD_LEN + SYNCLOG_INDEXNUM * sizeof(int);
    slog->len = len;
    int end = lseek(slog->fd, 0, SEEK_END);
    DINFO("synclog end: %d\n", end);
    lseek(slog->fd, 0, SEEK_SET);

    if (end == 0 || end < len) { // new file
        g_runtime->logver = synclog_lastlog();

        /*if (end > 0 && end < len) {
            //truncate_file(slog->fd, 0);

            g_runtime->logver = synclog_lastlog();
            DINFO("end < len, synclog_lastlog: %d\n", g_runtime->logver);
        }*/

        unsigned short format = DUMP_FORMAT_VERSION;
        unsigned int   newver = g_runtime->logver + 1;
        unsigned int   synlen = SYNCLOG_INDEXNUM;

        //DINFO("synclog new ver: %d, %x, synlen: %d, %x\n", newver, newver, synlen, synlen);
        if (writen(slog->fd, &format, sizeof(short), 0) < 0) {
            DERROR("write synclog format error: %d\n", format);
            MEMLINK_EXIT;
        }
        if (writen(slog->fd, &newver, sizeof(int), 0) < 0) {
            DERROR("write synclog newver error: %d\n", newver);
            MEMLINK_EXIT;
        }
        /*if (writen(slog->fd, &g_cf->role, sizeof(char), 0) < 0) {
            DERROR("write synclog role error: %d\n", g_cf->role);
            MEMLINK_EXIT;
        }*/

        if (writen(slog->fd, &synlen, sizeof(int), 0) < 0) {
            DERROR("write synclog synlen error: %d\n", synlen);
            MEMLINK_EXIT;
        }

        truncate_file(slog->fd, len);
        slog->pos = slog->len;

        g_runtime->logver = newver;
        slog->version = newver;
    }
       
    DINFO("mmap file ... len:%d, fd:%d\n", slog->len, slog->fd);
    slog->index = mmap(NULL, slog->len, PROT_READ|PROT_WRITE, MAP_SHARED, slog->fd, 0);
    if (slog->index == MAP_FAILED) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("synclog mmap error: %s\n",  errbuf);
        MEMLINK_EXIT;
    }

    if (end >= len) {
        g_runtime->logver = *(unsigned int*)(slog->index + sizeof(short));
        slog->version = g_runtime->logver;

        /*char role = *(slog->index + SYNCLOG_HEAD_LEN - sizeof(int));
        if (role != g_cf->role) {
            synclog_rotate(slog);
        }*/
        
        DINFO("validate synclog ...\n");
        if ((ret = synclog_validate(slog)) < 0) {
            DERROR("synclog_validate error: %d\n", ret);
        }
    }

    DINFO("=== runtime logver: %u file logver: %u ===\n", g_runtime->logver, (unsigned int)*(slog->index + sizeof(short)));
    DINFO("index_pos: %d, pos: %d\n", slog->index_pos, slog->pos);
    g_runtime->synclog = slog;

    return slog;
}

SyncLog*
synclog_open(char *filename)
{
    SyncLog    *slog = NULL;

    if (!isfile(filename)) {
        return NULL;
    }
    
    slog = (SyncLog*)zz_malloc(sizeof(SyncLog));
    if (NULL == slog) {
        DERROR("malloc SyncLog error!\n");
        MEMLINK_EXIT;
        return NULL;
    }
    memset(slog, 0, sizeof(SyncLog));
    
    if (strchr(filename, '/') == NULL) {
        snprintf(slog->filename, PATH_MAX, "%s/%s", g_cf->datadir, filename);
    }else{
        snprintf(slog->filename, PATH_MAX, "%s", filename);
    }
    
    slog->fd = open(slog->filename, O_RDWR);
    if (slog->fd == -1) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("open synclog %s error: %s\n", slog->filename,  errbuf);
        zz_free(slog);
        MEMLINK_EXIT;
        return NULL;
    }

    int len = SYNCLOG_HEAD_LEN + SYNCLOG_INDEXNUM * sizeof(int);
    slog->len = len;

    slog->index = mmap(NULL, slog->len, PROT_READ|PROT_WRITE, MAP_SHARED, slog->fd, 0);
    if (slog->index == MAP_FAILED) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("synclog mmap error: %s\n",  errbuf);
        MEMLINK_EXIT;
    }

    slog->index_pos = 0;
    slog->pos       = lseek(slog->fd, 0, SEEK_END);

    int i;
    int *idxdata = (int*)(slog->index + SYNCLOG_HEAD_LEN);
    for (i = 0; i < SYNCLOG_INDEXNUM; i++) {
        if (idxdata[i] == 0) {
            slog->index_pos = i;
            break;
        }
    }
    if (slog->index_pos == 0 && i == SYNCLOG_INDEXNUM) {
        slog->index_pos = SYNCLOG_INDEXNUM;
    }

    slog->version = *(unsigned int*)(slog->index + sizeof(short));

    return slog;
}

int
synclog_new(SyncLog *slog)
{
    slog->fd = open(slog->filename, O_RDWR|O_CREAT, 0644);
    if (slog->fd == -1) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("open synclog %s error: %s\n", slog->filename,  errbuf);
        zz_free(slog);
        MEMLINK_EXIT;
        return -1;
    }

    unsigned short format = DUMP_FORMAT_VERSION;
    unsigned int   newver = g_runtime->synclog->version + 1;
    unsigned int   synlen = SYNCLOG_INDEXNUM;
  
    slog->version = newver;
    
    DINFO("synclog new ver: %d, %x, synlen: %d, %x\n", newver, newver, synlen, synlen);
    if (writen(slog->fd, &format, sizeof(short), 0) < 0) {
        DERROR("write synclog format error: %d\n", format);
        MEMLINK_EXIT;
    }
    if (writen(slog->fd, &newver, sizeof(int), 0) < 0) {
        DERROR("write synclog newver error: %d\n", newver);
        MEMLINK_EXIT;
    }
    /*if (writen(slog->fd, &g_cf->role, sizeof(char), 0) < 0) {
        DERROR("write synclog role error: %d\n", g_cf->role);
        MEMLINK_EXIT;
    }*/
    if (writen(slog->fd, &synlen, sizeof(int), 0) < 0) {
        DERROR("write synclog synlen error: %d\n", synlen);
        MEMLINK_EXIT;
    }
    
    fsync(slog->fd);

    truncate_file(slog->fd, slog->len);

    slog->index = mmap(NULL, slog->len, PROT_READ|PROT_WRITE, MAP_SHARED, slog->fd, 0);
    if (slog->index == MAP_FAILED) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("synclog mmap error: %s\n",  errbuf);
        MEMLINK_EXIT;
    }

    slog->pos       = slog->len;
    slog->index_pos = 0;
    g_runtime->logver += 1;

    return 0;
}

/**
 * Rotate sync log if the current sync log is not empty. The current sync log 
 * is renamed to bin.log.xxx.
 *
 * @param slog sync log
 */
int
synclog_rotate(SyncLog *slog)
{
    unsigned int    newver; 
    char            newfile[PATH_MAX];
    int             ret;
    
    if (slog->index_pos == 0) {
        DWARNING("rotate cancle, no data!\n");
        return 0;
    }

    memcpy(&newver, slog->index + sizeof(short), sizeof(int));

    ret = munmap(slog->index, slog->len);
    if (ret == -1) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("munmap error: %s\n",  errbuf);
    }
    slog->index = NULL;
    close(slog->fd);
    slog->fd = -1;
    
    snprintf(newfile, PATH_MAX, "%s.%d", slog->filename, newver);
    DINFO("rotate to: %s\n", newfile);
    if (rename(slog->filename, newfile) == -1) {
        DERROR("rename error: %s\n", newfile);
    }
    //g_runtime->logver += 1;

    synclog_new(slog);
    
    return 0;
}



int 
synclog_validate(SyncLog *slog)
{
    int i = 0;
    int looplen = (slog->len - SYNCLOG_HEAD_LEN) / sizeof(int); // index zone length
    char *data  = slog->index + SYNCLOG_HEAD_LEN; // skip to index
    unsigned int *loopdata = (unsigned int*)data;
    unsigned int lastidx   = slog->len;
    char dumpfile[PATH_MAX];
    int dumplogver, dumplogpos, ret;
    
    //add by lanwenhong
    snprintf(dumpfile, PATH_MAX, "%s/%s", g_cf->datadir, DUMP_FILE_NAME);
    if (isfile(dumpfile) != 0 && g_cf->role == ROLE_SLAVE) {
        FILE *fp = NULL;
        unsigned int offset = 0;

        fp = fopen64(dumpfile, "rb");
        offset = sizeof(short) + sizeof(int);
        fseek(fp, offset, SEEK_SET);
        ret = ffread(&dumplogver, sizeof(int), 1, fp);
        ret = ffread(&dumplogpos, sizeof(int), 1, fp);
        //DNOTE("dumlogver: %d, g_runtime->logver: %d\n", dumplogver, g_runtime->logver);
        if (dumplogver == g_runtime->logver) {
            i = dumplogpos;
        }
        //DNOTE("dumplogpos: %d\n", dumplogpos);
        fclose(fp);
        
    }    
    //modify by lanwenhong
    for (; i < looplen; i++) {
        if (loopdata[i] == 0) {
            break;
        }
        lastidx = loopdata[i];
    }
    
    slog->index_pos = i;
    //索引第一个是0
    if (i == 0) {
        slog->pos = slog->len;
        return 0;
    }
    else if (i == dumplogpos) {
        lastidx = loopdata[i - 1];
        if (lastidx == 0) {
            slog->pos = slog->len;
            slog->index_pos = dumplogpos;
            return 0;
        }
    }
    

    unsigned int dlen;
    int            filelen = lseek(slog->fd, 0, SEEK_END);
    unsigned int idx;
    
    DINFO("filelen: %d, lastidx: %d, i: %d\n", filelen, lastidx, i);

    if (lastidx >= filelen) {
        idx = slog->index_pos - 1;
        i = 1;
        while ((lastidx = loopdata[idx-i]) >= filelen) {
            i++;
        }
        DERROR("synclog lost the last %d line(s) data\n", i);
        MEMLINK_EXIT;
    }

    //unsigned int oldidx = lastidx; 
    while (lastidx < filelen) {
        //DINFO("check offset: %d\n", lastidx);
        // skip logver and logline
        idx = lastidx;
        idx += sizeof(int) + sizeof(int);
        if (idx >= filelen) {
            DERROR("syslog data too small, at: %u, index: %d\n", idx, lastidx);
            MEMLINK_EXIT;
        }
        int cur = lseek(slog->fd, idx, SEEK_SET);
        if (readn(slog->fd, &dlen, sizeof(int), 0) != sizeof(int)) {
            DERROR("synclog readn error, lastidx: %u, cur: %u\n", lastidx, cur);
            MEMLINK_EXIT;
        }

        idx = cur + dlen + sizeof(int);

        if (filelen == idx) { // size ok
            slog->pos = filelen;
            break;
        }else if (filelen < idx) { // too small
            DERROR("synclog data too small, at:%u, index:%d\n", cur, lastidx);
            MEMLINK_EXIT;
            /*
            DINFO("index: %p, loopdata: %p\n", slog->index, loopdata);
            slog->index_pos -= 1;
            loopdata[slog->index_pos] = 0;
            slog->pos = cur;

            DINFO("v: %d\n", *(unsigned int*)(slog->index + 10 + (i-1) * 4));
            DINFO("v: %d\n", *(unsigned int*)(slog->index + 10 + i * 4));
            //msync(slog->index, slog->len, MS_SYNC);
            //lseek(slog->fd, idx, SEEK_SET);
            break;
            */
        }else{
            if (slog->index_pos >= SYNCLOG_INDEXNUM) {
                DERROR("sync index is full, but still have datas, synclog data too large\n");
                MEMLINK_EXIT;
            }
            DINFO("revise synclog index lost: %d\n", slog->index_pos);
            loopdata[slog->index_pos] = idx;
            lastidx = idx;
            slog->index_pos++;
/*
            DERROR("index too small, at:%u, index:%d\n", cur, i);
            MEMLINK_EXIT;
            lastidx = lastidx + sizeof(int) + dlen; // skip to next
            if (i == 0 || lastidx > oldidx) { // add index
                loopdata[slog->index_pos] = lastidx;
                slog->index_pos += 1;
                DNOTE("lastidx: %d, oldidx: %d\n", lastidx, oldidx);
            }*/
        }
    }
    
    DNOTE("sync_validate index_pos:%d, pos:%d\n", slog->index_pos, slog->pos);
    return 0;
}

/**
 * Write a record to sync log
 *
 * @param slog sync log
 * @param data log record
 * @param datalen log record length
 */
int
synclog_write(SyncLog *slog, char *data, int datalen)
{
    int offset;
    int wlen = datalen;
    int wpos = 0;
    int ret;
    int cur;
    char *wdata = data;
    //int pos = lseek(slog->fd, 0, SEEK_CUR);
    //char buf[128];
    
    if (slog->index_pos == SYNCLOG_INDEXNUM) {
        synclog_rotate(slog);
    }

    // add logver/logline for master
    if (g_cf->role == ROLE_MASTER) {
        //int count = 0;
        wlen = datalen + sizeof(int) + sizeof(int);
        wdata = (char *)alloca(wlen);
        memcpy(wdata, &g_runtime->logver, sizeof(int));
        memcpy(wdata + sizeof(int), &slog->index_pos, sizeof(int));
        memcpy(wdata + sizeof(int) + sizeof(int), data, datalen);
    }

#ifdef DEBUG
    char tmpbuf[512];
    DINFO("write log: %s\n", formath(wdata, wlen, tmpbuf, 512));
#endif
    DINFO("datalen: %d, wlen: %d, pos:%d, index_pos:%d\n", datalen, wlen, slog->pos, slog->index_pos);
    cur = lseek(slog->fd, slog->pos, SEEK_SET);
    /*
    DINFO("write synclog, cur: %u, pos: %d, %d, wlen: %d, %s\n", cur, slog->pos, 
                (unsigned int)lseek(slog->fd, 0, SEEK_CUR),
                wlen, formath(data, wlen, buf, 128));
    ret = write(slog->fd, "zhaowei", 7);
    DINFO("write test ret: %d, cur: %d\n", ret, (unsigned int)lseek(slog->fd, 0, SEEK_CUR));
    */    
    offset = wlen;
    while (wlen > 0) {
        /*int old = lseek(slog->fd, 0, SEEK_CUR);
        char databuf[1023];
        int rsize = read(slog->fd, databuf, 23);
        DINFO("read: %s\n", formath(databuf, rsize, buf, 128)); 
        lseek(slog->fd, old, SEEK_SET);
        */

        //DINFO("write pos: %u wpos:%d, wlen:%d, data:%s\n", (unsigned int)lseek(slog->fd, 0, SEEK_CUR), wpos, wlen, formath(data+wpos, wlen, buf, 128));
        DINFO("write pos: %u wpos:%d, wlen:%d\n", (unsigned int)lseek(slog->fd, 0, SEEK_CUR), wpos, wlen);
        ret = write(slog->fd, wdata + wpos, wlen);
        DINFO("write return:%d\n", ret);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }else{
                char errbuf[1024];
                strerror_r(errno, errbuf, 1024);
                DERROR("write synclog error: %s\n",  errbuf);
                MEMLINK_EXIT;
                return -1;
            }
        }else{
            wlen -= ret; 
            wpos += ret;
        }
    }
    
    DINFO("after write pos: %u\n", (unsigned int)lseek(slog->fd, 0, SEEK_CUR));
    //unsigned int *idxdata = (unsigned int*)(slog->index + sizeof(short) + sizeof(int) * 2);
    unsigned int *idxdata = (unsigned int*)(slog->index + SYNCLOG_HEAD_LEN);
    DINFO("write index: %u, %u\n", slog->index_pos, slog->pos);
    idxdata[slog->index_pos] = slog->pos;
    slog->index_pos ++;
    slog->pos += offset;
    return 0;
}

int
synclog_version(SyncLog *slog, unsigned int *logver)
{
    lseek(slog->fd, sizeof(short), SEEK_SET);        

    unsigned int ver;

    int ret = readn(slog->fd, &ver, sizeof(int), 0);
    if (ret != sizeof(int)) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("readn error: %d, %s\n", ret,  errbuf);
        return -1;
    }

    *logver = ver;
    return 0;
}

void
synclog_close(SyncLog *slog)
{
    if (NULL == slog)
        return;
    
    if (munmap(slog->index, slog->len) == -1) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("munmap error: %s\n",  errbuf);
    }
    slog->index = NULL;
    close(slog->fd); 
    slog->fd = -1;
    slog->index_pos = 0;
    slog->pos = 0;
    slog->version = 0;
}

void
synclog_destroy(SyncLog *slog)
{
    if (NULL == slog)
        return;
    
    if (munmap(slog->index, slog->len) == -1) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("munmap error: %s\n",  errbuf);
    }
   
    close(slog->fd); 
    zz_free(slog);
}

/**
 * synclog_lastlog - find the log number for the latest log file.
 */
int
synclog_lastlog()
{
    DIR     *mydir; 
    struct  dirent *nodes;
    //struct  dirent *result;
    int     maxid = 0;

    mydir = opendir(g_cf->datadir);
    if (NULL == mydir) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("opendir %s error: %s\n", g_cf->datadir,  errbuf);
        MEMLINK_EXIT;
        return 0;
    }
    //DINFO("readdir ...\n");
    //while (readdir_r(mydir, nodes, &result) == 0 && nodes) {
    while ((nodes = readdir(mydir)) != NULL) {
        DINFO("name: %s\n", nodes->d_name);
        if (strncmp(nodes->d_name, "bin.log.", 8) == 0) {
            int binid = atoi(&nodes->d_name[8]);
            if (binid > maxid) {
                maxid = binid;
            }
        }
    }
    closedir(mydir);

    return maxid;
}

/* find previous binlog
 */
int
synclog_prevlog(int curid)
{
    DIR     *mydir; 
    struct  dirent *nodes;
    int     maxid = 0;

    if (curid == 0) {
        curid = INT_MAX;
    }

    mydir = opendir(g_cf->datadir);
    if (NULL == mydir) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("opendir %s error: %s\n", g_cf->datadir,  errbuf);
        MEMLINK_EXIT;
        return 0;
    }
    while ((nodes = readdir(mydir)) != NULL) {
        //DINFO("name: %s\n", nodes->d_name);
        if (strncmp(nodes->d_name, "bin.log.", 8) == 0) {
            int binid = atoi(&nodes->d_name[8]);
            if (binid > maxid && curid > binid) {
                maxid = binid;
            }
        }
    }
    closedir(mydir);

    return maxid;
}

/*
 * find all binlog
 */
int
synclog_scan_binlog(int *result, int rsize)
{
    DIR     *mydir; 
    struct  dirent *nodes;
    int     minid = g_runtime->dumplogver;
    //int     logids[10000] = {0};
    int     i = 0;

    DINFO("try get synclog ...\n");
    mydir = opendir(g_cf->datadir);
    if (NULL == mydir) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("opendir %s error: %s\n", g_cf->datadir,  errbuf);
        return -2; 
    } 
       //本地没有dump.dat文件    
       if (g_runtime->dumplogver == 0) {
        minid = 1;
    }    
    while ((nodes = readdir(mydir)) != NULL) {
        //DINFO("name: %s\n", nodes->d_name);
        if (strncmp(nodes->d_name, "bin.log.", 8) == 0) {
            int binid = atoi(&nodes->d_name[8]);
            if (binid >= minid) {
                //logids[i] = binid;
                if (i >= rsize) {
                    DERROR("array to store binlog, too small.\n");
                    return -100;
                }
                result[i] = binid;
                i++;
            }   
        }   
    }   
    closedir(mydir);        

    qsort(result, i, sizeof(int), compare_int);

    return i;
}

/**
 * truncate bin.log
 * @param slog
 * @param index  last used index, have data
 */

/*
int
synclog_truncate(SyncLog *slog, int index)
{
    if (NULL == slog || index >= SYNCLOG_INDEXNUM - 1) {
        DERROR("synclog truncate error, slog:%p, index:%d\n", slog, index);
        return -1;
    }

    int *idxdata = (int*)(slog->index + SYNCLOG_HEAD_LEN);
    int i, ret;
    for (i = index + 1; i < slog->index_pos; i++) {
        idxdata[i] = 0; 
    }

    unsigned int lastpos = idxdata[index];

    ret = lseek(slog->fd, lastpos + SYNCPOS_LEN, SEEK_SET);
    if (ret < 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("lseek error: %u, %s\n", (unsigned int)(lastpos + SYNCPOS_LEN),  errbuf);
        MEMLINK_EXIT;
    }

    unsigned short len;
    ret = readn(slog->fd, &len, sizeof(short), 0);
    if (ret != sizeof(short)) {
        DERROR("read len error: %d\n", ret);
        MEMLINK_EXIT;
    }

    lastpos += SYNCPOS_LEN + sizeof(short) + len;

    DINFO("truncate synclog to %d\n", lastpos);
    ret = ftruncate(slog->fd, lastpos);
    if (ret == -1) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("ftruncate synclog error: %s\n",  errbuf);
        MEMLINK_EXIT;
    }

    slog->index_pos = index + 1;
    slog->pos       = lastpos;

    return 0;
}
*/

int
synclog_truncate(SyncLog *slog, unsigned int logver, unsigned int dumplogpos)
{
    if (NULL == slog) {
        DERROR("synclog truncate error, slog:%p, logver:%d, dumplogpos: %d\n", slog, logver, dumplogpos);
        return -1;
    }

    int *idxdata = (int*)(slog->index + SYNCLOG_HEAD_LEN);
    int i, ret;
    for (i = 0; i < SYNCLOG_INDEXNUM; i++) {
        idxdata[i] = 0; 
    }

    //DINFO("truncate synclog to %d\n", lastpos);
    ret = ftruncate(slog->fd, SYNCLOG_HEAD_LEN + SYNCLOG_INDEXNUM * sizeof(int));
    if (ret == -1) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("ftruncate synclog error: %s\n",  errbuf);
        MEMLINK_EXIT;
    }

    slog->index_pos = dumplogpos;
    slog->pos       = SYNCLOG_HEAD_LEN + SYNCLOG_INDEXNUM * sizeof(int);
    
    memcpy(slog->index + sizeof(short), &logver, sizeof(int)); 
    return 0;
}

/**
 * only used by slave
 */
int
synclog_resize(unsigned int logver, unsigned int logline)
{
    char logname[PATH_MAX];
    char logname_new[PATH_MAX];
    int  i, ret;

    // find the binlog file
    DINFO("resize logver:%d, runtime logver:%d\n", logver, g_runtime->logver);
    if (logver < g_runtime->logver) {
        synclog_destroy(g_runtime->synclog);
       
        snprintf(logname, PATH_MAX, "%s/bin.log", g_cf->datadir);
        snprintf(logname_new, PATH_MAX, "%s/bin.log.%d", g_cf->datadir, g_runtime->synclog->version);

        DINFO("rotate bin.log to %s\n", logname_new);
        ret = rename(logname, logname_new);
        if (ret < 0) {
            char errbuf[1024];
            strerror_r(errno, errbuf, 1024);
            DERROR("rename %s error: %d, %s\n", logname, ret,  errbuf);
            MEMLINK_EXIT;
        }

        snprintf(logname, PATH_MAX, "%s/bin.log.%d", g_cf->datadir, logver);
        snprintf(logname_new, PATH_MAX, "%s/bin.log", g_cf->datadir);

        DINFO("restore %s to bin.log\n", logname);
        ret = rename(logname, logname_new);
        if (ret < 0) {
            char errbuf[1024];
            strerror_r(errno, errbuf, 1024);
            DERROR("rename %s error: %d, %s\n", logname, ret,  errbuf);
            MEMLINK_EXIT;
        }

        unsigned int oldlogver = g_runtime->logver;
        //snprintf(logname, PATH_MAX, "%s/bin.log.%d", g_cf->datadir, logver);
        //g_runtime->synclog = synclog_open(logname);
        DINFO("open synclog ...\n");
        g_runtime->synclog = synclog_create();
        if (NULL == g_runtime->synclog) {
            DERROR("open synclog error: %s\n", logname);
            MEMLINK_EXIT;
            return -1;
        }
        //g_runtime->logver = logver;
  
        
        for (i = logver + 1; i <= oldlogver; i++) {
            snprintf(logname, PATH_MAX, "%s/bin.log.%d", g_cf->datadir, i);
            snprintf(logname_new, PATH_MAX, "%s/rm.bin.log.%d", g_cf->datadir, i);

            DINFO("try rename %s to %s\n", logname, logname_new);
            if (isfile(logname)) {
                //ret = unlink(logname);
                DINFO("ok, rename %s to %s\n", logname, logname_new);
                ret = rename(logname, logname_new);
                if (ret < 0) {
                    char errbuf[1024];
                    strerror_r(errno, errbuf, 1024);
                    DERROR("rename %s error: %d, %s\n", logname, ret,  errbuf);
                    MEMLINK_EXIT;
                }
            }
        }
    }

    // find position in binlog file
    DINFO("resize logline:%d, runtime logline:%d\n", logline, g_runtime->synclog->index_pos);
    if (logline < g_runtime->synclog->index_pos) {
        //synclog_truncate(g_runtime->synclog, logline);

        /*
        int *idxdata = (int*)(g_runtime->synclog->index + SYNCLOG_HEAD_LEN);

        for (i = logline + 1; i < g_runtime->synclog->index_pos; i++) {
            idxdata[i] = 0; 
        }

        unsigned int lastpos = idxdata[logline];
        
        ret = lseek(g_runtime->synclog->fd, lastpos + SYNCPOS_LEN, SEEK_SET);
        if (ret < 0) {
            char errbuf[1024];
            strerror_r(errno, errbuf, 1024);
            DERROR("lseek error: %u, %s\n", lastpos + SYNCPOS_LEN,  errbuf);
            MEMLINK_EXIT;
        }
        
        unsigned short len;
        ret = readn(g_runtime->synclog->fd, &len, sizeof(short), 0);
        if (ret != sizeof(short)) {
            DERROR("read len error: %d\n", ret);
            MEMLINK_EXIT;
        }

        lastpos += SYNCPOS_LEN + sizeof(short) + len;
      
        DINFO("truncate synclog to %d\n", lastpos);
        ret = ftruncate(g_runtime->synclog->fd, lastpos);
        if (ret == -1) {
            char errbuf[1024];
            strerror_r(errno, errbuf, 1024);
            DERROR("ftruncate synclog error: %s\n",  errbuf);
            MEMLINK_EXIT;
        }
        */
    }
    

    return 0;
}

int synclog_clean(unsigned int logver, unsigned int dumplogpos)
{
    DIR     *mydir; 
    struct  dirent *nodes;
    char logname[PATH_MAX];
    char logname_new[PATH_MAX];

    DINFO("try get synclog ...\n");
    mydir = opendir(g_cf->datadir);
    if (NULL == mydir) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("opendir %s error: %s\n", g_cf->datadir,  errbuf);
        return -2; 
    }   
    while ((nodes = readdir(mydir)) != NULL) {
        //DINFO("name: %s\n", nodes->d_name);
        if (strncmp(nodes->d_name, "bin.log.", 8) == 0) {
            int binid = atoi(&nodes->d_name[8]);

            snprintf(logname, PATH_MAX, "%s/bin.log.%d", g_cf->datadir, binid);
            snprintf(logname_new, PATH_MAX, "%s/rm.bin.log.%d", g_cf->datadir, binid);
            if (isfile(logname)) {
                int ret;

                DINFO("ok, rename %s to %s\n", logname, logname_new);
                ret = rename(logname, logname_new);
                if (ret < 0) {
                    char errbuf[1024];
                    strerror_r(errno, errbuf, 1024);
                    DERROR("rename %s error: %d, %s\n", logname, ret,  errbuf);
                    MEMLINK_EXIT;
                }
            }
        }   
    }  

    synclog_close(g_runtime->synclog);
    snprintf(logname, PATH_MAX, "%s/bin.log", g_cf->datadir);
    snprintf(logname_new, PATH_MAX, "%s/rm.bin.log", g_cf->datadir);
    if (isfile(logname)) {
        rename(logname, logname_new);
    }
    g_runtime->logver = g_runtime->synclog->version = logver - 1;
    synclog_new(g_runtime->synclog);
    g_runtime->synclog->index_pos = dumplogpos;
    closedir(mydir);
    //synclog_truncate(g_runtime->synclog, logver, dumplogpos);    
    return 1;
}

void
synclog_sync_disk(int fd, short event, void *arg)
{
    DINFO("=== sync binlog to disk. ===\n");
#ifdef __linux
    fdatasync(g_runtime->synclog->fd);
#else // FreeBSD, MacOSX not have fdatasync
    fsync(g_runtime->synclog->fd);
#endif

    struct timeval    tv;
    struct event *syncevt = arg;

    evutil_timerclear(&tv);
    tv.tv_sec = g_cf->sync_disk_interval; 
    event_add(syncevt, &tv);
}

int
synclog_read_data(char *binlogname, int fromline, int toline, char *md5str)
{
    int fd;
    int ret = 0;

    fd = open(binlogname, O_RDONLY);
    if (-1 == fd) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("open file %s error! %s\n", binlogname, errbuf);
        MEMLINK_EXIT;
    }

    int len = lseek(fd, 0, SEEK_END);

    char *addr = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("synclog mmap %s error: %s\n", binlogname, errbuf);
        MEMLINK_EXIT;
    }
    unsigned int indexlen = *(unsigned int *)(addr + SYNCLOG_HEAD_LEN - sizeof(int));
    char *data = addr + SYNCLOG_HEAD_LEN + indexlen *sizeof(int);
    char *enddata = addr + len;
    int  *indexdata = (int *)(addr + SYNCLOG_HEAD_LEN);

    int fpos, tpos;
    char *fdata, *tdata;

    fpos = indexdata[fromline];
    tpos = indexdata[toline];
    fdata = addr + fpos;
    tdata = addr + tpos;

    if (fdata < data || fdata > enddata) {
        ret = -1;
        goto read_data_end;
    }
    
    if (fpos == 0 &&  tpos == 0) {
        ret = -1;
        goto read_data_end;
    }
    int datalen = tdata - fdata;
    int cmdlen = 0;
    
    datalen += sizeof(int) * 3;

    memcpy(&cmdlen, tdata + sizeof(int) *2, sizeof(int));

    datalen += cmdlen;




    /*
    if (tdata < data || tdata > enddata) {
        ret = -2;
        goto read_data_end;
    }*/
    
    //if (tdata - fdata > 0)
    md5(md5str, (unsigned char *)fdata, datalen); 
read_data_end:
    munmap(addr, len);
    close(fd);

    return ret;
}

int
synclog_reset(char *binlogname, int fromline, int toline)
{    
    int fd;
    
    DINFO("fromline: %d, toline: %d\n", fromline, toline);
    synclog_destroy(g_runtime->synclog);
    fd = open(binlogname, O_RDWR|O_CREAT, 0644);
    if (-1 == fd) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("open file %s error! %s\n", binlogname, errbuf);
        MEMLINK_EXIT;
    }

    int len = lseek(fd, 0, SEEK_END);

    char *addr = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("synclog mmap %s error: %s\n", binlogname, errbuf);
        MEMLINK_EXIT;
    }
    int  *indexdata = (int *)(addr + SYNCLOG_HEAD_LEN);
    char *enddata = addr + len;
    
    int fpos, tpos;
    char *fdata, *tdata;

    fpos = indexdata[fromline];
    tpos = indexdata[toline];
    fdata = addr + fpos;
    tdata = addr + tpos;
    
    DINFO("fdata: %p, tdata: %p, enddata: %p\n", fdata, tdata, enddata);
    int i;
    for (i = fromline; i <= toline; i++) {
        indexdata[i] = 0;
    }
    
    int newlen = len - (enddata - fdata);
    munmap(addr, len);

    truncate_file(fd, newlen);
#ifdef __linux
    fdatasync(fd);
#else // FreeBSD, MacOSX not have fdatasync
    fsync(fd);
#endif
    close(fd);

    if (toline - fromline < BINLOG_CHECK_COUNT) {
        if (g_runtime->synclog->version == 1) {
            DINFO("g_runtime->synclog->version : %d\n", g_runtime->synclog->version);
            //synclog_destroy(g_runtime->synclog);
            //g_runtime->synclog = synclog_create();
            //return 0;
            goto reset_end;
        }
        int n;
        int logids[10000] = {0};
        char logname[PATH_MAX];
        char cur[PATH_MAX];
        n = synclog_scan_binlog(logids, 10000);
        DINFO("=====n: %d\n", n);
        snprintf(logname, PATH_MAX, "%s/bin.log.%d", g_cf->datadir, logids[n - 1]);
        snprintf(cur, PATH_MAX, "%s/bin.log", g_cf->datadir);
        
        DINFO("cur: %s, logname: %s\n", cur, logname);
        rename(logname, cur);
        goto reset_end;
        //g_runtime->synclog = synclog_create();
        //return 0;
    }
reset_end:
    g_runtime->synclog = synclog_create();

    return 0;
}

/**
 * @}
 */
