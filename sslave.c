/**
 * 从数据同步线程
 * @file sslave.c
 * @author zhaowei
 * @ingroup memlink
 * @{
 */
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h> 
#include <netinet/tcp.h> 
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>

#include "logfile.h"
#include "zzmalloc.h"
#include "base/utils.h"
#include "base/md5.h"
#include "base/pack.h"
#include "base/defines.h"
#include "network.h"
#include "serial.h"
#include "synclog.h"
#include "common.h"
#include "dumpfile.h"
#include "sslave.h"
#include "runtime.h"

#define MAX_SYNC_PREV   100
//#define SYNCPOS_LEN   (sizeof(int)+sizeof(int))

ssize_t
sslave_readn(SSlave *ss, int fd, void *vptr, size_t n)
{
    size_t  nleft;
    ssize_t nread;
    char    *ptr;

    ptr = vptr;
    nleft = n;
    
    while (nleft > 0) {
        if (ss->isrunning == FALSE)
            return -1;
        if (timeout_wait_read(fd, 1) != TRUE) {
            DINFO("read timeout.\n");
            continue; 
        }
        if ((nread = read(fd, ptr, nleft)) < 0) {
            char errbuf[1024];
            strerror_r(errno, errbuf, 1024);
            //DERROR("nread: %d, error: %s\n", nread,  errbuf);
            if (errno == EINTR) {
                nread = 0;
            }else {
                char errbuf[1024];
                strerror_r(errno, errbuf, 1024);
                DERROR("readn error: %s\n",  errbuf);
                //MEMLINK_EXIT;
                return -1;
            }
        }else if (nread == 0) {
            DERROR("read 0, maybe conn close.\n");
            break;
        }
        nleft -= nread;
        ptr += nread;
    }

    return (n - nleft);
}

void static 
clean(void *arg)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)arg;
    pthread_mutex_unlock(mutex);
}

#ifdef RECV_LOG_BY_PACKAGE 
static int
sslave_recv_package_log(SSlave *ss)
{
    char recvbuf[1024 * 1024];
    int  checksize = SYNCPOS_LEN + sizeof(int);
    int  ret;
    unsigned char first_check = 0;
    char *ptr = NULL;
    unsigned int package_len;
    unsigned int  rlen = 0;
    unsigned int    logver;
    unsigned int    logline;
    // send sync
    
    DINFO("slave recv log ...\n");
    while (1) {
        //读数据包长度
        ret = sslave_readn(ss, ss->sock, &package_len, sizeof(int));
        if (ret < sizeof(int)) {
            DERROR("read sync package_len too short: %d, close\n", ret);
            close(ss->sock);
            ss->sock = -1;
            return -1;
        }
        //读取数据包
        //unsigned int need = package_len - sizeof(int);
        unsigned int need = package_len;
        ret = sslave_readn(ss, ss->sock, recvbuf, need);
        if (ret < need) {
            DERROR("read sync command set too short: %d, close\n", ret);
            close(ss->sock);
            ss->sock = -1;
            return -1;
        }
        DINFO("package_len: %d\n", package_len);
        int count = 0;//统计读出命令的字节数
        ptr = recvbuf;
        //while (count < package_len - sizeof(int)) {
        while (count < package_len) {
            memcpy(&logver, ptr, sizeof(int));
            memcpy(&logline, ptr + sizeof(int), sizeof(int));
            memcpy(&rlen, ptr + SYNCPOS_LEN, sizeof(int));
            DINFO("logver: %d, logline: %d\n, rlen: %d", logver, logline, rlen);
            
            DINFO("ss->logver: %d, ss->logline: %d\n", g_runtime->synclog->version, g_runtime->synclog->index_pos);            
            //只需校验第一次接收到的命令
            if (first_check == 0) {
                if ( g_runtime->synclog->index_pos != 0 && logver == g_runtime->synclog->version && logline == g_runtime->synclog->index_pos - 1) {
                    //int pos = g_runtime->synclog->index_pos;
                    int *indxdata = (int *)(g_runtime->synclog->index + SYNCLOG_HEAD_LEN);
                    DINFO("logline: %d, indxdata[logline]=%d\n", logline, indxdata[logline]);        
                    if (indxdata[logline] != 0) {
                        count += SYNCPOS_LEN + sizeof(int) + rlen;
                        DINFO("package_len: %d, count: %d\n", package_len, count);
                        DINFO("---------------------skip\n");
                        ptr += SYNCPOS_LEN + sizeof(int) + rlen;
                        continue;
                    }
                    first_check = 1;
                }
            }
            unsigned int size = checksize + rlen;
            char cmd;
            memcpy(&cmd, ptr + SYNCPOS_LEN + sizeof(int), sizeof(char));
            if (ss->isrunning == FALSE) {
                close(ss->sock);
                ss->sock = -1;
                return 0;
            }
            DINFO("=====lock\n");
            pthread_cleanup_push(clean, (void *)&(g_runtime->mutex));
            pthread_mutex_lock(&g_runtime->mutex);
            
            ret = wdata_apply(ptr + SYNCPOS_LEN, rlen, MEMLINK_NO_LOG, NULL);
            DINFO("wdata_apply return: %d\n", ret);
            if (ss->isrunning == TRUE && ret == 0) {
                int sret = 0;

                sret = synclog_write(g_runtime->synclog, ptr, size);
                if (sret < 0) {
                    DERROR("synclog_write error: %d\n", sret);
                    MEMLINK_EXIT;
                }
                sret = syncmem_write(g_runtime->syncmem, ptr + SYNCPOS_LEN, rlen,
                    g_runtime->synclog->version, g_runtime->synclog->index_pos-1);
                if (sret < 0) {
                    DERROR("synclog_write error: %d\n", sret);
                    MEMLINK_EXIT;
                }
            }
            pthread_mutex_unlock(&g_runtime->mutex);
            pthread_cleanup_pop(0);
            DINFO("====unlock\n");

            ptr += SYNCPOS_LEN + sizeof(int) + rlen;
            count += SYNCPOS_LEN + sizeof(int) + rlen;
        }

    }
    return 0;
}

#else
static int
sslave_recv_log(SSlave *ss)
{
    char recvbuf[1024];
    int  checksize = SYNCPOS_LEN + sizeof(int);
    int  ret;
    unsigned int  rlen = 0;
    unsigned int    logver;
    unsigned int    logline;
    // send sync

    DINFO("slave recv log ...\n");
    while (1) {
        ret = readn(ss->sock, recvbuf, checksize, 0);
        if (ret < checksize) {
            DERROR("read sync head too short: %d, close\n", ret);
            close(ss->sock);
            ss->sock = -1;
            return -1;
        }
        //DINFO("readn return:%d\n", ret);
        memcpy(&rlen, recvbuf + SYNCPOS_LEN, sizeof(int));
        ret = readn(ss->sock, recvbuf + checksize, rlen, 0);
        if (ret < rlen) {
            DERROR("read sync too short: %d, close\n", ret);
            close(ss->sock);
            ss->sock = -1;
            return -1;
        }
        //DINFO("recv log len:%d\n", rlen);
        memcpy(&logver, recvbuf, sizeof(int));
        memcpy(&logline, recvbuf + sizeof(int), sizeof(int));
        DINFO("logver:%d, logline:%d\n", logver, logline);
        // after getdump, must not skip first one
        if (logver == g_runtime->synclog->version && logline == g_runtime->synclog->index_pos && g_runtime->synclog->index_pos != 0) {
            //skip first one
            //continue;
            //int pos = g_runtime->synclog->index_pos;
            int *indxdata = (int *)(g_runtime->synclog->index + SYNCLOG_HEAD_LEN);

            //上一条已经记录了
            if (indxdata[logline] != 0) {
                continue;
            }
        }

        unsigned int size = checksize + rlen;
        struct timeval start, end;
        char cmd;

        memcpy(&cmd, recvbuf + SYNCPOS_LEN + sizeof(int), sizeof(char));
        pthread_mutex_lock(&g_runtime->mutex);
        gettimeofday(&start, NULL);
        ret = wdata_apply(recvbuf + SYNCPOS_LEN, rlen, 0, NULL);
        DINFO("wdata_apply return:%d\n", ret);
        if (ret == 0) {
            //DINFO("synclog index_pos:%d, pos:%d\n", g_runtime->synclog->index_pos, g_runtime->synclog->pos);
            synclog_write(g_runtime->synclog, recvbuf, size);
        }
        gettimeofday(&end, NULL);
        DINFO("cmd:%d %d %u us\n", cmd, ret, timediff(&start, &end));
        pthread_mutex_unlock(&g_runtime->mutex);

    }

    return 0;
}
#endif

/**
 * load master dump info
 */
int
sslave_load_master_dump_info(SSlave *ss, char *dumpfile, long long *filesize, long long *dumpsize, unsigned int *dumpver, unsigned int *logver)
{
    int  ret;
    long long fsize;
    long long dsize;
    unsigned int mylogver;
    
    if (!isfile(dumpfile)) {
        return -1;
    }

    FILE    *dumpf;    
    dumpf = (FILE*)fopen64(dumpfile, "r");
    if (dumpf == NULL) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("open file %s error! %s\n", dumpfile,  errbuf);
        return -1;
    }

    fseek(dumpf, 0, SEEK_END);
    fsize = ftell(dumpf);

    if (filesize) {
        *filesize = fsize;
    }

    if (fsize >= DUMP_HEAD_LEN) {
        fseek(dumpf, DUMP_HEAD_LEN - sizeof(long long), SEEK_SET);
        ret = fread(&dsize, 1, sizeof(long long), dumpf);
        if (ret != sizeof(long long)) {
            char errbuf[1024];
            strerror_r(errno, errbuf, 1024);
            DERROR("fread error, ret:%d, %s\n", ret,  errbuf);
            fclose(dumpf);
            return -1;
            //goto read_dump_over;
        }
        if (dumpsize) {
            *dumpsize = dsize;
        }

        //int pos = sizeof(short) + sizeof(int);
        int pos = sizeof(short);
        fseek(dumpf, pos, SEEK_SET);

        ret = fread(&mylogver, 1, sizeof(int), dumpf);
        if (ret != sizeof(int)) {
            char errbuf[1024];
            strerror_r(errno, errbuf, 1024);
            DERROR("fread error: %s\n",  errbuf);
            fclose(dumpf);
            return -1;
            //goto read_dump_over;
        }
        if (dumpver) {
            *dumpver = mylogver;
        }
        ret = fread(&mylogver, 1, sizeof(int), dumpf);
        if (ret != sizeof(int)) {
            char errbuf[1024];
            strerror_r(errno, errbuf, 1024);
            DERROR("fread error: %s\n",  errbuf);
            fclose(dumpf);
            return -1;
            //goto read_dump_over;
        }
        if (logver) {
            *logver = mylogver;
        }


    }
    fclose(dumpf);

    return 0;
}

/* 
 * find previous binlog position for sync
 */
/*
static int
sslave_prev_sync_pos(SSlave *ss)
{
    1char filepath[PATH_MAX];
    //int bver = ss->binlog_ver;
    int ret;
    int i;

    for (i = ss->trycount; i < MAX_SYNC_PREV; i++) {
        ss->trycount += 1;        

        if (ss->binlog_index == 0) {
            if (ss->binlog) {
                synclog_destroy(ss->binlog);
                ss->binlog = NULL;
            }
            ret = synclog_prevlog(ss->binlog_ver);        
            if (ret <= 0) {
                DERROR("synclog_prevlog error: %d\n", ret);
                return -1;
            }
            DINFO("prev binlog: %d, current binlog: %d\n", ret, ss->binlog_ver);
            ss->binlog_ver   = ret;
            ss->binlog_index = 0;

            if (ss->binlog_ver != g_runtime->logver) {
                snprintf(filepath, PATH_MAX, "%s/bin.log.%d", g_cf->datadir, ss->binlog_ver);
            }else{
                snprintf(filepath, PATH_MAX, "%s/bin.log", g_cf->datadir);
            }
            DINFO("try open synclog: %s\n", filepath);
            ss->binlog = synclog_open(filepath);
            if (NULL == ss->binlog) {
                DERROR("open synclog error: %s\n", filepath);
                return -1;
            }

            ss->binlog_index = ss->binlog->index_pos;
        }
        
        if (NULL == ss->binlog) {
            if (ss->binlog_ver != g_runtime->logver) {
                snprintf(filepath, PATH_MAX, "%s/bin.log.%d", g_cf->datadir, ss->binlog_ver);
            }else{
                snprintf(filepath, PATH_MAX, "%s/bin.log", g_cf->datadir);
            }
            
            DINFO("open synclog: %s\n", filepath);
            ss->binlog = synclog_open(filepath);
            if (NULL == ss->binlog) {
                DERROR("open synclog error: %s\n", filepath);
                return -1;
            }
            ss->binlog_index = ss->binlog->index_pos;
        }

        DINFO("synclog index_pos:%d, pos:%d, len:%d\n", ss->binlog->index_pos, ss->binlog->pos, ss->binlog->len);
        ss->binlog_index -= 1;
        int *idxdata = (int*)(ss->binlog->index + SYNCLOG_HEAD_LEN);
        int pos      = idxdata[ss->binlog_index];
        
        DINFO("=== binlog_index:%d, pos: %d ===\n", ss->binlog_index, pos);

        lseek(ss->binlog->fd, pos, SEEK_SET);
        unsigned int logver, logline;
        ret = readn(ss->binlog->fd, &logver, sizeof(int), 0);
        if (ret != sizeof(int)) {
            char errbuf[1024];
            strerror_r(errno, errbuf, 1024);
            DERROR("read logver error! %s\n",  errbuf);
            continue;
            //return -1;
        }
        ret = readn(ss->binlog->fd, &logline, sizeof(int), 0);
        if (ret != sizeof(int)) {
            char errbuf[1024];
            strerror_r(errno, errbuf, 1024);
            DERROR("read logline error! %s\n",  errbuf);
            continue;
            //return -1;
        }
        
        DINFO("read logver:%d, logline:%d\n", logver, logline);
        ss->logver  = logver;
        ss->logline = logline;

        break;
    }
    if (ss->trycount >= MAX_SYNC_PREV)
        return -1;

    return 0;
}
*/


/**
 * send command and recv response
 */
int 
sslave_do_cmd(SSlave *ss, char *sendbuf, int buflen, char *recvbuf, int recvlen)
{
    int ret;

    DINFO("send buflen: %d\n", buflen);
    ret = writen(ss->sock, sendbuf, buflen, ss->timeout);
    if (ret != buflen) {
        sslave_close(ss);
        DINFO("cmd writen error: %d, close.\n", ret);
        return -1;
    }
    
    ret = readn(ss->sock, recvbuf, sizeof(int), ss->timeout);
    if (ret != sizeof(int)) {
        sslave_close(ss);
        DINFO("cmd readn error: %d, close.\n", ret);
        return -2;
    }
    
    unsigned int len;
    memcpy(&len, recvbuf, sizeof(int));
    DINFO("cmd recv len:%d\n", len);
    
    if (len + sizeof(int) > recvlen) {
        sslave_close(ss);
        DERROR("recv data too long: %d\n", (int)(len + sizeof(short)));
        return -3;
    }

    ret = readn(ss->sock, recvbuf + sizeof(int), len, ss->timeout);
    if (ret != len) {
        sslave_close(ss);
        DINFO("cmd readn error: %d, close.\n", ret);
        return -4;
    }
    
    return len + sizeof(int);
}

/**
 * get dump.dat from master
 */
int
sslave_do_getdump(SSlave *ss)
{
    char    dumpfile[PATH_MAX];
    char    dumpfile_tmp[PATH_MAX];

    snprintf(dumpfile, PATH_MAX, "%s/dump.master.dat", g_cf->datadir);
    snprintf(dumpfile_tmp, PATH_MAX, "%s/dump.master.dat.tmp", g_cf->datadir);

    long long     tmpsize  = 0;
    long long     dumpsize = 0;
    unsigned int dumpver  = 0;
    unsigned int logver   = 0;
    int             ret;
    char         first = TRUE;

    ret = sslave_load_master_dump_info(ss, dumpfile_tmp, &tmpsize, &dumpsize, &dumpver, &logver);
    if (ret == -1)
        first = TRUE;
    else
        first = FALSE;

    char sndbuf[1024];
    int  sndlen;
    char recvbuf[1024];

    // do getdump
    //DINFO("try getdump, dumpver:%d, dumpsize:%lld, filesize:%lld\n", dumpver, dumpsize, tmpsize);
    DNOTE("try getdump, dumpver:%d, dumpsize:%lld, filesize:%lld\n", dumpver, dumpsize, tmpsize);
    sndlen = cmd_getdump_pack(sndbuf, dumpver, tmpsize); 
    ret = sslave_do_cmd(ss, sndbuf, sndlen, recvbuf, 1024); 
    if (ret < 0) {
        DERROR("cmd getdump error: %d\n", ret);
        return -1;
    }
    unsigned short retcode;
    unsigned long long size, rlen = 0;
    /*
    memcpy(&retcode, recvbuf + sizeof(int), sizeof(short));
    memcpy(&dumpver, recvbuf + CMD_REPLY_HEAD_LEN, sizeof(int));
    memcpy(&size, recvbuf + CMD_REPLY_HEAD_LEN + sizeof(int), sizeof(long long));
    DNOTE("retcode:%d, dumpver:%d, dump size: %lld\n", retcode, dumpver, size);
    */
    char md5[33] = {0};
    char dumpfilemd5[PATH_MAX]; 
    if (first == TRUE) {
        unpack(recvbuf + sizeof(int), ret, "hsil", &retcode, md5, &dumpver, &size);
        snprintf(dumpfilemd5, PATH_MAX, "%s/dump.master.dat.md5", g_cf->datadir);
        FILE *fp = fopen(dumpfilemd5, "wb");
        fwrite(md5, sizeof(char), 32, fp);
        fclose(fp);
    } else {
        unpack(recvbuf + sizeof(int), ret,  "hil", &retcode, &dumpver, &size);
    }

    char    dumpbuf[8192];
    int     buflen = 0;
    int     fd = 0;
    int        oflag; 

    if (retcode == CMD_GETDUMP_OK) {
        oflag = O_CREAT|O_WRONLY|O_APPEND;
    }else{
        oflag = O_CREAT|O_WRONLY|O_TRUNC;
    }

    fd = open(dumpfile_tmp, oflag, 0644);
    if (fd == -1) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("open dumpfile %s error! %s\n", dumpfile_tmp,  errbuf);
        MEMLINK_EXIT;
    }

    int rsize = 8192;
    while (rlen < size) {
        rsize = size - rlen;
        if (rsize > 8192) {
            rsize = 8192;
        }
        ret = readn(ss->sock, dumpbuf, rsize, ss->timeout);    
        if (ret < 0) {
            DERROR("read dump error: %d\n", ret);
            goto sslave_do_getdump_error;
        }
        if (ret == 0) {
            DERROR("read eof! close conn:%d\n", ss->sock);
            goto sslave_do_getdump_error;
        }
        buflen = ret;
        ret = writen(fd, dumpbuf, buflen, 0);
        if (ret < 0) {
            char errbuf[1024];
            strerror_r(errno, errbuf, 1024);
            DERROR("write dump error: %d, %s\n", ret,  errbuf);
            goto sslave_do_getdump_error;
        }

        rlen += ret;
        DINFO("recv dump size:%lld\n", rlen);
    }

    close(fd);
    
    ret = rename(dumpfile_tmp, dumpfile);
    if (ret == -1) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("dump file rename error: %s\n",  errbuf);
        MEMLINK_EXIT; 
    }
    char md5_local[33] = {0};
    ret = md5_file(dumpfile, md5_local, 32);
    
    if (strcmp(md5_local, md5)) {
        DERROR("md5_local: %s, md5: %s\n", md5_local, md5);
        goto sslave_do_getdump_error;
    }
    return 0;

sslave_do_getdump_error:
    close(ss->sock);
    ss->sock = -1;
    close(fd);
    return -1;
}

/**
 * apply sync,getdump commands before recv server push log
 */

int
get_binlog_md5(int logver, int logline, char *md5)
{
    char binlog[PATH_MAX];
    int ret;
    
    snprintf(binlog, PATH_MAX, "%s/bin.log", g_cf->datadir);
    if (logver == 0 && logline == 0) {
        DINFO("sync 0, 0, need not check md5\n");
        return 0;
    }
    if (logline - BINLOG_CHECK_COUNT >= 0) {
        DWARNING("fromline: %d, toline: %d\n", logline - BINLOG_CHECK_COUNT, logline);
        ret = synclog_read_data(binlog, logline - BINLOG_CHECK_COUNT, logline, md5);
        DWARNING("synclog_read_data: %d\n", ret); 
        if ( ret == -1 || ret == -2)
            return 0;
        return BINLOG_CHECK_COUNT;
    } else {
        synclog_read_data(binlog, 0, logline, md5); 
        return logline;
    }
    return 0;  
}

int
sslave_conn_init(SSlave *ss)
{
    char sndbuf[1024];
    int  sndlen;
    char recvbuf[1024];
    char md5[33] = {0};
    int  bcount = 0;
    int  ret;
    unsigned int  dumpver;
    char    mdumpfile[PATH_MAX];
    int  sndlogver = 0, sndlogline = 0;
    char md5_check_err = FALSE;
    unsigned int logver = 0;
    int dumplogpos = 0;
    int dumplogver = 0;

    snprintf(mdumpfile, PATH_MAX, "%s/dump.master.dat", g_cf->datadir);

    if (isfile(mdumpfile)) {
        ss->is_getdump = TRUE;
        FILE *fp = NULL;
        unsigned int offset = 0;
        uint64_t dlvnum = 0;
        uint64_t lvnum = 0;

        fp = fopen64(mdumpfile, "rb");
        offset = sizeof(short) + sizeof(int);
        fseek(fp, offset, SEEK_SET);
        ret = ffread(&dumplogver, sizeof(int), 1, fp);
        ret = ffread(&dumplogpos, sizeof(int), 1, fp);
        fclose(fp);

        dlvnum = dumplogver;
        dlvnum = dlvnum << 32;
        dlvnum |= dumplogpos;

        lvnum = g_runtime->synclog->version;
        lvnum = lvnum << 32;
        lvnum |= g_runtime->synclog->index_pos;
        if (dlvnum == lvnum) {
            sndlogver = dumplogver;
            sndlogline = dumplogpos;
        } else if (lvnum > dlvnum) {
            sndlogver = g_runtime->synclog->version;
            sndlogline = g_runtime->synclog->index_pos - 1;
        } else {
            DERROR("g_runtime->synclog->version: %d, g_runtime->synclog->index: %d\n", g_runtime->synclog->version,
                g_runtime->synclog->index_pos);
            DERROR("dumplogver: %d, dumplogline: %d\n", dumplogver, dumplogpos);
            DERROR("binlog in local may be error\n");
        }
    } else {
        sndlogver = 0;
        sndlogline = 0;
    }
    DINFO("slave init ...\n");

    while (1) {
        /*
        if (g_runtime->synclog->version == 1 && g_runtime->synclog->index_pos == 0 && logver == 0 && dumplogpos == 0) {
            sndlogver = 0;
            sndlogline = 0;
        } else {
            if (g_runtime->synclog->index_pos != 0) {
                sndlogver = g_runtime->synclog->version;
                //if (ss->is_getdump == 0)
                if (g_runtime->synclog->version != logver || g_runtime->synclog->index_pos != dumplogpos)
                    sndlogline = g_runtime->synclog->index_pos - 1;
                else
                    sndlogline = g_runtime->synclog->index_pos;

            } else {
                sndlogver = g_runtime->synclog->version;
                sndlogline = g_runtime->synclog->index_pos;
            }
        }*/
        DINFO("sndlogver: %d, sendlogline: %d\n", sndlogver, sndlogline);
        bcount = get_binlog_md5(sndlogver, sndlogline, md5); 
        DINFO("send md5: %s, bcount: %d\n", md5, bcount);
        if (md5[0] == '\0')
            sndlen = cmd_sync_pack(sndbuf, sndlogver, sndlogline, 0, md5);
        else
            sndlen = cmd_sync_pack(sndbuf, sndlogver, sndlogline, bcount, md5);
        ret = sslave_do_cmd(ss, sndbuf, sndlen, recvbuf, 1024);
        if (ret < 0) {
            DINFO("cmd sync error: %d\n", ret);
            return -1;
        }
        char syncret; 
        int  i = sizeof(int);

        syncret = recvbuf[i];
        if (syncret == CMD_SYNC_OK) { // ok, recv synclog
            DINFO("sync ok! try recv push message.\n");
            if (md5_check_err == TRUE) {
                DERROR("binlog in disk may change, need restart memlink\n");
                MEMLINK_EXIT;
            }
            return 0;
        }
        if (syncret == CMD_SYNC_MD5_ERROR) {
            //SThread *st = g_runtime->sthread;
            //st->stop = TRUE;
            //等待sthread线程停止推送binlog日志
            //while (st->push_stop_nums < st->conns) {
                //usleep(10);
            //}
            DERROR("md5 is error, exit\n");
            MEMLINK_EXIT;
            //DNOTE("st->push_stop_nums: %d, st->conns: %d\n", st->push_stop_nums, st->conns);
            char binlogname[PATH_MAX];
            snprintf(binlogname, PATH_MAX, "%s/bin.log", g_cf->datadir);
            synclog_reset(binlogname, sndlogline - bcount, sndlogline);
            sndlogver = g_runtime->synclog->version;
            sndlogline = g_runtime->synclog->index_pos - 1;
            md5_check_err = TRUE;
            continue;
        }
        if (syncret == CMD_SYNC_FAILED && ss->is_getdump == FALSE) {
            if (sslave_do_getdump(ss) == 0) {
                DINFO("load dump ...\n");
                hashtable_clear_all(g_runtime->ht);

                dumpfile_load(g_runtime->ht, mdumpfile, 1);
                g_runtime->synclog->index_pos = g_runtime->dumplogpos;
                
                sslave_load_master_dump_info(ss, mdumpfile, NULL, NULL, &dumpver, &logver);
                synclog_clean(logver, g_runtime->dumplogpos);
                dumplogpos = g_runtime->dumplogpos;

                dumpfile(g_runtime->ht);
                DINFO("logver: %d, dumplogpos: %d\n", logver, g_runtime->dumplogpos);
                memcpy((g_runtime->synclog->index + sizeof(short)), &logver, sizeof(int));
                ss->is_getdump = TRUE;
                sndlogver = g_runtime->synclog->version;
                sndlogline = g_runtime->synclog->index_pos;
            }else{
                DERROR("The slave data may be error!\n");
                return -1;
            }
        } else {
            DERROR("The data on master may be change!\n");
            MEMLINK_EXIT;
        }
    } 
    return 0;
}

void sig_slaver_handler()
{
    //DINFO("slaver pid: %d\n", (int)pthread_self());
    DINFO("============slave thread will exit\n");
    if (g_runtime->slave->sock > 0) {
        close(g_runtime->slave->sock);
        g_runtime->slave->sock = -1;
    }
    g_runtime->slave->threadid = 0;
    pthread_exit(NULL);
    return;

}
/**
 * slave sync thread
 * 1.find local sync logver/logline 
 * 2.send sync command to server or get dump
 * 3.recv server push log
 */
void*
sslave_run(void *args)
{
    SSlave    *ss = (SSlave*)args;
    int        ret;
    struct  sigaction sigact;
    //int     error;

    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;

    sigact.sa_handler = sig_slaver_handler;
    sigaction(SIGUSR1, &sigact, NULL);

    // do not start immediately, wait for some initialize
    while (ss->isrunning == 0) {
        sleep(1);
    }

    DINFO("sslave run ...\n");
    while (1) {
        if (ss->sock <= 0 && ss->isrunning == TRUE) {
            DINFO("connect to master %s:%d timeout:%d\n", g_cf->master_sync_host, g_cf->master_sync_port, ss->timeout);
            ret = tcp_socket_connect(g_cf->master_sync_host, g_cf->master_sync_port, ss->timeout, TRUE);
            if (ret <= 0) {
                DWARNING("connect error! ret:%d, continue.\n", ret);
                sleep(1);
                continue;
            }
            ss->sock = ret;

            DINFO("connect to master ok!\n");
            // do sync, getdump
            ret = sslave_conn_init(ss);
            if (ret < 0) {
                DINFO("slave conn init error!\n");
                sleep(1);
                continue;
            }
            //break;
        }
        // recv sync log from master
#ifdef RECV_LOG_BY_PACKAGE
        if (ss->sock > 0 && ss->isrunning == TRUE)
            ret = sslave_recv_package_log(ss);
#else
        ret = sslave_recv_log(ss);
#endif
        if (ss->isrunning == FALSE)
            sleep(1);
    }

    return NULL;
}

void
sslave_thread(SSlave *slave)
{
    /*
    while (slave->threadid != 0) {
        DINFO("wait slave thread exit ...\n");
        usleep(10);
    }
    sleep(1);
    */
    memset(slave, 0, sizeof(SSlave));

    slave->timeout = g_cf->timeout;
    //slave->is_backup_do = FALSE;

    pthread_t threadid;
    pthread_attr_t attr;
    int ret;

    ret = pthread_attr_init(&attr);
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("pthread_attr_init error: %s\n",  errbuf);
        MEMLINK_EXIT;
    }

    ret = pthread_create(&threadid, &attr, sslave_run, slave);
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("pthread_create error: %s\n",  errbuf);
        MEMLINK_EXIT;
    }
    slave->threadid = threadid;
    ret = pthread_detach(threadid);
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("pthread_detach error; %s\n",  errbuf);
        MEMLINK_EXIT;
    }

    DINFO("create sync slave thread ok!\n");
}

SSlave*
sslave_create() 
{
    SSlave *ss;

    ss = (SSlave*)zz_malloc(sizeof(SSlave));
    if (ss == NULL) {
        DERROR("sslave malloc error.\n");
        MEMLINK_EXIT;
    }
    memset(ss, 0, sizeof(SSlave));
    sslave_thread(ss);
    return ss;
}

void
sslave_go(SSlave *ss)
{
    ss->isrunning = TRUE;
}

void
sslave_stop(SSlave *ss)
{
    ss->isrunning = FALSE;
}

void
sslave_close(SSlave *ss)
{
    if (ss->sock > 0) {
        close(ss->sock);
        ss->sock = -1;
    }
    
}

void 
sslave_destroy(SSlave *ss) 
{
    if (ss->sock > 0) {
        close(ss->sock);
        ss->sock = -1;
    }
    zz_free(ss);
}

/**
 * @}
 */
