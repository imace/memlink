/**
 * 主同步线程
 * @file sthread.c
 * @author zhaowei
 * @ingroup memlink
 * @{
 */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "network.h"
#include "sthread.h"
#include "wthread.h"
#include "logfile.h"
#include "myconfig.h"
#include "zzmalloc.h"
#include "base/utils.h"
#include "base/md5.h"
#include "base/pack.h"
#include "common.h"
#include "synclog.h"
#include "serial.h"
#include "runtime.h"

#define CMD_HEAD_LEN        sizeof(int) * 2 + sizeof(int)

void
read_dump(int fd, short event, void *arg)
{
    SyncConn *conn =  (SyncConn *)arg;
    char *buffer = conn_write_buffer((Conn *)conn, SYNC_BUF_SIZE);
    int ret;

    DINFO("reading dump...\n");

    ret = readn(conn->dump_fd, buffer, SYNC_BUF_SIZE, 0);
    DINFO("ret: %d\n", ret);
    if (ret > 0) {
        conn->wlen = ret;
        DINFO("conn->wlen: %d, conn->wpos : %d\n", conn->wlen, conn->wpos);
    } else if (ret == 0) {
        DINFO("finished sending dump\n");
        event_del(&conn->sync_write_evt);
        close(conn->dump_fd);
        conn->dump_fd = -1;
        DINFO("change event to read.\n");
        int ret = change_event((Conn *)conn, EV_READ | EV_PERSIST, 0, 1);
        if (ret < 0) {
            DERROR("change_evnet error: %d, close conn\n", ret);
            sync_conn_destroy((Conn *)conn);
        }
    }else{

        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("read dump error! %s",  errbuf);
        MEMLINK_EXIT;
    }
    return;
}

int
cmd_get_dump(SyncConn *conn, char *data, int datalen)
{
    int ret = 0;
    unsigned int dumpver;
    uint64_t transferred_size;
    int retcode;
    uint64_t offset;
    uint64_t file_size;
    uint64_t remaining_size;

    cmd_getdump_unpack(data, &dumpver, &transferred_size);
    char dump_filename[PATH_MAX];
    snprintf(dump_filename, PATH_MAX, "%s/dump.dat", g_cf->datadir);
    
    int fd;
    if ((fd = open(dump_filename, O_RDONLY)) == -1) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("open %s error ! %s\n", dump_filename,  errbuf);
        MEMLINK_EXIT;
    }
    conn->dump_fd = fd;
    file_size = lseek(fd, 0, SEEK_END);
    
    DINFO("g_runtime->dumpver: %d, dumpver: %d\n", g_runtime->dumpver, dumpver);
    if (g_runtime->dumpver == dumpver) {
        if (transferred_size <= file_size) {
            retcode = CMD_GETDUMP_OK;
        } else {
            retcode = CMD_GETDUMP_SIZE_ERR;
        }
    } else {
        retcode = CMD_GETDUMP_CHANGE;
    }

    offset = retcode == CMD_GETDUMP_OK ? transferred_size: 0;
    remaining_size = file_size - offset;
    
    DINFO("remaining_size: %llu\n", (unsigned long long)remaining_size);
    //第一次传输，需要告诉从dump.dat的md5值
    //int retlen = sizeof(int) + sizeof(int64_t);
    //char retrc[128];
    char *retrc = conn_write_buffer((Conn *)conn, 128);
    int count;
    if (remaining_size == file_size) {
        char md5[33] = {0};
        char dumpfilemd5[PATH_MAX];
        
        snprintf(dumpfilemd5, PATH_MAX, "%s/dump.data.md5", g_cf->datadir);
        if (isfile(dumpfilemd5)) {
            FILE *fp = fopen(dumpfilemd5, "rb");
            ffread(md5, sizeof(char), 32, fp); 
            fclose(fp);
        } else {
            ret = md5_file(dump_filename, md5, 32);
        }
        count = pack(retrc, 0, "$4hsil", retcode, md5, g_runtime->dumpver, remaining_size);
        conn->wlen = count;
    } else {
        count = pack(retrc, 0, "$4hiL", retcode, g_runtime->dumpver, remaining_size);
        conn->wlen = count;
    }
    //memcpy(retrc, &g_runtime->dumpver, sizeof(int));
    //memcpy(retrc + sizeof(int), &remaining_size, sizeof(int64_t));

    if (offset < file_size) {
        conn->status = SEND_DUMP;
        lseek(fd, offset, SEEK_SET);
    } else {
        conn->status = NOT_SEND;
    }
    conn_send_buffer((Conn *)conn);    
    //ret = conn_send_buffer_reply((Conn *)conn, retcode, retrc, retlen);
    return ret;
}

int
decode_package(char *buffer)
{
    unsigned int package_len = 0;
    char *ptr;
    unsigned int count = 0;
    
    ptr = buffer + sizeof(int);
    memcpy(&package_len, buffer, sizeof(int));
    DINFO("check package_len: %d\n", package_len);
    while (count < package_len - sizeof(int)) {
        unsigned int logver, logline, cmdlen;
        memcpy(&logver, ptr, sizeof(int));
        memcpy(&logline, ptr + sizeof(int), sizeof(int));
        memcpy(&cmdlen, ptr + sizeof(int) * 2, sizeof(int));


        ptr += sizeof(int) * 3 + cmdlen;
        count += sizeof(int) *3 + cmdlen;

    }
    return 1;
}

int
get_synclog_record(SyncConn *conn)
{
    unsigned int logver, logline;
    unsigned int cmdlen;
    ssize_t ret;
    unsigned int i = conn->synclog->index_pos;
    unsigned int count = 0;
    char *buffer = conn_write_buffer((Conn *)conn, SYNC_BUF_SIZE);
    SyncLog *synclog = conn->synclog;
    int *indxdata = (int *)(synclog->index + SYNCLOG_HEAD_LEN);
    SThread *st;
    SyncConnInfo *conninfo = NULL;
    
    zz_check(conn);
    zz_check(conn->wbuf);
    if ( i == SYNCLOG_INDEXNUM) {
        return 0;
    }
    if (i >  SYNCLOG_INDEXNUM || indxdata[i] == 0) {
        return -1;
    }
        
    st = (SThread *)conn->thread;
    int j;
    for (j = 0; j < g_cf->max_sync_conn; j++) {
        conninfo = &(st->sync_conn_info[j]);
        if (conninfo->fd == conn->sock)
            break;
    }
    
    DINFO("----------------------------i: %d\n", i);
    char *ptr = buffer + sizeof(int);
    while(i < SYNCLOG_INDEXNUM && indxdata[i] != 0) {
        char per_buf[1024] = {0};
            
        lseek(synclog->fd, indxdata[i], SEEK_SET);
        //读取logver
        ret = readn(synclog->fd, &logver, sizeof(int), 0);
        memcpy(per_buf, &logver, sizeof(int));
        conninfo->logver = logver;
        //读取logline
        ret = readn(synclog->fd, &logline, sizeof(int), 0);
        memcpy(per_buf + sizeof(int), &logline, sizeof(int));
            
        conninfo->logline = logline;
        
        /*
        if (logver == g_runtime->synclog->version) {
            DINFO("logine: %d, g_runtime->synclog->index_pos: %d\n", logline, g_runtime->synclog->index_pos);
            conninfo->delay = g_runtime->synclog->index_pos - logline - 1;
        } else if (g_runtime->synclog->version - logver == 1) {    
            conninfo->delay = (SYNCLOG_INDEXNUM - logline) + g_runtime->synclog->index_pos - 1;
        } else {
            conninfo->delay = (SYNCLOG_INDEXNUM - logline) + 
                SYNCLOG_INDEXNUM * (g_runtime->synclog->version - logver - 1) + g_runtime->synclog->index_pos - 1;
        }*/
        conninfo->delay = (g_runtime->synclog->version - conninfo->logver) * SYNCLOG_INDEXNUM
            + g_runtime->synclog->index_pos -1 - conninfo->logline;

            
        //命令长度
        ret = readn(synclog->fd, &cmdlen, sizeof(int), 0);
        memcpy(per_buf + sizeof(int) + sizeof(int), &cmdlen, sizeof(int));
        //实际命令
        if (cmdlen + CMD_HEAD_LEN > SYNC_BUF_SIZE && count == 0) {//处理第一条命令，如果命令大于要写入的缓冲区，需要调整缓冲区大小
            //改变缓冲区大小
            char *buffer = conn_write_buffer((Conn *)conn, cmdlen + CMD_HEAD_LEN);
            int package_len = CMD_HEAD_LEN + cmdlen + sizeof(int);
            
            memcpy(buffer + sizeof(int), per_buf,  CMD_HEAD_LEN);
            ret = readn(synclog->fd, (buffer + sizeof(int) * 4), cmdlen, 0);
            memcpy(buffer, &package_len, sizeof(int));
            synclog->index_pos = i + 1;
            conn->wlen = package_len;
            conn->blogver = logver;
            conn->blogline = logline;
            return 0;    
        }
        //缓冲区不大小不够了
        if (count + CMD_HEAD_LEN + cmdlen > SYNC_BUF_SIZE - sizeof(int)) {
            //count += sizeof(int);
            //memcpy(buffer, &count, sizeof(int));
            //decode_package(buffer);
            //conn->wlen = count;
            break;
        }
        //读取命令，copy到缓冲区
        DINFO("pakcage logver: %d, logline: %d, cmdlen: %d\n", logver, logline, cmdlen);
        memcpy(ptr, per_buf, CMD_HEAD_LEN + cmdlen);//copy命令头部， 包括logver, logline, 命令长度
        ret = readn(synclog->fd, (ptr + (sizeof(int) * 3)), cmdlen, 0); 
        count += CMD_HEAD_LEN + cmdlen;
        ptr = ptr + CMD_HEAD_LEN + cmdlen;    
        i++;
        conn->synclog->index_pos = i;
        conn->blogver = logver;
        conn->blogline = logline;
        //DINFO("----------------------------------------i: %d\n", i);
    }
    zz_check(conn);
    zz_check(conn->wbuf);
    if (count > 0) {
        //count += sizeof(int);
        if (conn->need_skip_one == FALSE)
            conn->need_skip_one = TRUE;

        memcpy(buffer, &count, sizeof(int));
        conn->wlen = count + sizeof(int);
        return 0;
    }
    return -1;
}

void
read_synclog(int fd, short event, void *arg)
{
    int newlogline, newlogver;
    int ret;
    SyncConn *conn = (SyncConn *)arg;
    char *buffer = conn_write_buffer((Conn *)conn, SYNC_BUF_SIZE);
    SyncConnInfo *conninfo = NULL;

    if (event == EV_TIMEOUT) {
        DINFO("time out event\n");
    } else {
        DINFO("write event\n");
    }
    
    SThread *st;
    st = (SThread *)conn->thread;
    int j;
    for (j = 0; j < g_cf->max_sync_conn; j++) {
        conninfo = &(st->sync_conn_info[j]);
        if (conninfo->fd == conn->sock)
            break;
    }
    zz_check(conn); 
    zz_check(conn->wbuf);
    //先从缓冲区中读对应的logver, logline
    ret = syncmem_read(g_runtime->syncmem, conn->blogver, conn->blogline,
        &newlogver, &newlogline, buffer, SYNC_BUF_SIZE, conn->need_skip_one);
    zz_check(conn);
    zz_check(conn->wbuf);
    if (ret == 0) {//从buffer中读取到数据
        conn->blogver = newlogver;
        conn->blogline = newlogline;
        conninfo->logver = newlogver;
        conninfo->logline = newlogline;
        conninfo->delay = (g_runtime->synclog->version - conninfo->logver) * SYNCLOG_INDEXNUM
            + g_runtime->synclog->index_pos -1 - conninfo->logline;
        int size;
        memcpy(&size, buffer, sizeof(int));
        conn->wlen = size + sizeof(int);
        DINFO("conn->wlen: %d\n", conn->wlen);
        if (conn->need_skip_one == FALSE)
            conn->need_skip_one = TRUE;
        if (event == EV_TIMEOUT) {
            DINFO("add write evnet\n");
            event_add(&conn->sync_write_evt, NULL);
        }
        return;
    } else if (ret == 1) {//buffer中没有产生新的数据
        if (event == EV_WRITE) {
            DINFO("remove write event\n");
            event_del(&conn->sync_write_evt);
        }
        DINFO("add interval event\n");
        evtimer_add(&conn->sync_check_interval_evt, &conn->interval);
        return;
    } else {//在buffer中找不到对应的数据
        if (conn->synclog != NULL) {
            DINFO("-------can't find data in syncbuffer\n");
            DINFO("conn->blogver: %d, conn->blogline: %d\n", conn->blogver, conn->blogline);
            DINFO("conn->synclog->version: %d, conn->synclog->blogline: %d\n", conn->synclog->version, conn->synclog->index_pos);
            if (conn->blogver > conn->synclog->version || (conn->blogver == conn->synclog->version && conn->blogline > conn->synclog->index_pos)) {
                DINFO("need check_binlog_local\n");
                if (conn->synclog != NULL) {
                    synclog_destroy(conn->synclog);
                    conn->synclog = NULL;
                }
                check_binlog_local(conn, conn->blogver, conn->blogline);

            } else if (conn->blogver <= conn->synclog->version || (conn->blogver == conn->synclog->version && conn->blogline <= conn->synclog->index_pos)) {
                DINFO("read binlog on disk\n");
            } else {
                DERROR("synclog data error!\n");
                MEMLINK_EXIT;
            }
        }
    }
    //read and check bin.log
    if (get_synclog_record(conn) < 0) {

        if (event == EV_WRITE) {
            DNOTE("remove write event\n");
            event_del(&conn->sync_write_evt);
        }
        DNOTE("add interval event\n");
        evtimer_add(&conn->sync_check_interval_evt, &conn->interval);
        return;
    }
    SyncLog *synclog = conn->synclog;
    if (synclog->index_pos >= SYNCLOG_INDEXNUM) {
        if (synclog->version < g_runtime->logver) {
            synclog_destroy(conn->synclog);
            conn->synclog = NULL;
            DINFO("need check_binlog_local\n");
            if (check_binlog_local(conn, synclog->version + 1, 0) < 0) {
                MEMLINK_EXIT;
            }
        } else if (synclog->version == g_runtime->logver && conn->wpos == 0 && conn->wlen == 0) {
            if (event == EV_WRITE) {
                DINFO("remove write event\n");
                event_del(&conn->sync_write_evt);
            }
            DNOTE("add interval event\n");
            evtimer_add(&conn->sync_check_interval_evt, &conn->interval);
            return;
        }
    }
    if (event == EV_TIMEOUT) {
        DNOTE("add write evnet\n");
        event_add(&conn->sync_write_evt, NULL);
    }
    return;
}

void sync_read(int fd, short event, void *arg)
{
    char buf[256];
    int ret;
    SyncConn *conn = (SyncConn *)arg;

    ret = read(fd, buf, 1);
    if (ret == 0) {
        DINFO("read 0, close conn %d.\n", fd);
        conn->destroy((Conn *)conn);
    } else if (ret < 0) {
        DINFO("read < 0, close conn %d.\n", fd);
        conn->destroy((Conn *)conn);
    } else {
        DINFO ("ignore receive data\n");
    }
}

void
sync_write(int fd, short event, void *arg)
{
    SThread *st = g_runtime->sthread;
    SyncConn *conn = (SyncConn *)arg;
    SyncConnInfo *conninfo = NULL;
    //从线程要做日志调整， 停止一切推送行为 
    int i;
    if (st->stop == TRUE) {
        if (st->sock >0) {
            event_del(&st->event);
            close(st->sock);
            st->sock = -1;
        }
        for (i = 0; i < g_cf->max_sync_conn; i++) {
            conninfo = &(st->sync_conn_info[i]);
            if (conninfo->fd == fd && conninfo->push_log_stop == FALSE) {
                event_del(&conn->sync_write_evt);
                event_del(&conn->sync_check_interval_evt);
                event_del(&conn->sync_read_evt);
                conninfo->push_log_stop = TRUE;
                st->push_stop_nums++;
            }
        }
        return;
    }

    if (event & EV_TIMEOUT) {
        DNOTE("write timeout: %d, close\n", fd);
        //conn->destroy((Conn *)conn);
        conn->timeout((Conn*)conn);
        return;
    }

    if (conn->wpos == conn->wlen) {
        if (conn->wlen == 0) {
            DINFO("no data in wbuf\n");
        } else {
            DINFO("finished sending %d bytes of data in wbuf\n", conn->wlen);
        }

        if (conn->cmd == CMD_SYNC) {
            zz_check(conn);
            zz_check(conn->wbuf);
            read_synclog(0, EV_WRITE, conn);
            zz_check(conn);
            zz_check(conn->wbuf);
        } else if (conn->cmd == CMD_GETDUMP) {
            read_dump(0, EV_WRITE, conn);
        }
    }
    if (conn->wlen - conn->wpos > 0) {
        DINFO("write to socket\n");
        conn_write((Conn *)conn);
    }
    return ;
}
//设置超时
static void
set_timeval(struct timeval *tm_ptr, unsigned int seconds, unsigned int msec)
{
    evutil_timerclear(tm_ptr);
    tm_ptr->tv_sec = seconds;
    //tm_ptr->tv_usec = msec * 1000;
    tm_ptr->tv_usec = msec;  
}

//设置主读写事件
void
set_rw_init(SyncConn *conn)
{
    event_del(&conn->evt);
    event_set(&conn->sync_write_evt, conn->sock, EV_WRITE | EV_PERSIST, sync_write, conn);
    event_base_set(conn->base, &conn->sync_write_evt);
    event_add(&conn->sync_write_evt, NULL);

    event_set(&conn->sync_read_evt, conn->sock, EV_READ | EV_PERSIST, sync_read, conn);
    event_base_set(conn->base, &conn->sync_read_evt);
    event_add(&conn->sync_read_evt, 0);

    return;
}

int
sync_conn_wrote(Conn *c)
{
    SyncConn *conn = (SyncConn *)c;
    DINFO("response finished with status %d\n", conn->status);
    switch (conn->status) {
        case NOT_SEND:
            conn_wrote(c);
            break;
        case SEND_LOG:
            //设置定时检测是否有新日志的事件
            evtimer_set(&conn->sync_check_interval_evt, read_synclog, conn);
            event_base_set(conn->base, &conn->sync_check_interval_evt);
            set_timeval(&conn->interval, 0, g_cf->sync_check_interval);
            //主的读写事件
            set_rw_init(conn);
            break;
        case SEND_DUMP:
            set_rw_init(conn);
            break;

        default:
            DERROR("illegal sync connect status %d\n", conn->status);
            MEMLINK_EXIT;
    }
    return 0;

}

//根据从传递的logver logline， 找到相应的bin.log
int
check_binlog_local(SyncConn *conn, unsigned int log_ver, unsigned int log_line)
{
    int ret = 0;
    char binlog[PATH_MAX];
    
    if (log_line == 0 && g_runtime->logver == log_ver) {
        snprintf(binlog, PATH_MAX, "%s/bin.log", g_cf->datadir);
        conn->synclog = synclog_open(binlog);
        conn->synclog->index_pos = log_line;
        return 0;
    }
    if (log_line > SYNCLOG_INDEXNUM) {
        DERROR("log no %u\n", log_line);
        return -1;
    }
    //根据logver, logline校验本地的bin.log
    if (log_ver <= 0) {
        DINFO("Invalid log version: %u\n", log_ver);
        return -2;    
    }
    
    //确定要同步的bin.log，没找到返回负值
    if (log_ver < g_runtime->synclog->version) {
        snprintf(binlog, PATH_MAX, "%s/bin.log.%u", g_cf->datadir, log_ver);
        if (!isfile(binlog)) {//当前bin.log不存在
            ret = -3;
        }
    } else if (log_ver == g_runtime->synclog->version) {//要同步bin.log
        snprintf(binlog, PATH_MAX, "%s/bin.log.%u", g_cf->datadir, log_ver);
        if (!isfile(binlog)) {
            snprintf(binlog, PATH_MAX, "%s/bin.log", g_cf->datadir);
            if (!isfile(binlog)) {
                ret = -4;
            }
        }
    } else {
        ret = -5;
    }
    //如果对应的bin.log存在
    if (ret == 0) {
        conn->synclog = synclog_open(binlog);
        conn->synclog->index_pos = log_line;
        int *indxdata = (int *)(conn->synclog->index + SYNCLOG_HEAD_LEN);

        //for (i = 0; i <= log_line; i++) {
            //DINFO("indxdata[%d] = %d\n",i,  indxdata[i]);
        //}        
        DINFO("find binlog name: %s, log_ver: %d, log_line: %d\n", binlog, log_ver, log_line);
            
        //DINFO("indxdata[log_line]=%d\n", indxdata[log_line]);
        if (log_line < SYNCLOG_INDEXNUM) {
            if (indxdata[log_line] == 0) {
                if (indxdata[log_line - 1] != 0) {
                    //conn->synclog->index_pos = log_line - 1;
                    return 0;
                } else {
                    synclog_destroy(conn->synclog);
                    conn->synclog = NULL;
                    return -6;
                }
            } else {
                return 0;
            }
        } else if (log_line == SYNCLOG_INDEXNUM) {
            if (indxdata[log_line - 1] != 0) {
                return 0;    
            } else {
                synclog_destroy(conn->synclog);
                conn->synclog = NULL;
                return -7;
            }
        }
    }
    DINFO("===ret: %d\n", ret);
    return ret;
}

//对应命令sync log_ver, log_line
int
cmd_sync(SyncConn *conn, char *data, int datalen)
{
    int ret;
    unsigned int log_ver =0, log_line = 0;
    int bcount = 0;
    char md5[33] = {0};
    char md5local[33] = {0};
    char binlog[PATH_MAX];
    
    cmd_sync_unpack(data, &log_ver, &log_line, &bcount, md5);
    DINFO("log version: %u, log line: %u, bcount: %d, md5: %s\n", log_ver, log_line, bcount, md5);

    snprintf(binlog, PATH_MAX, "%s/bin.log.%d", g_cf->datadir, log_ver);
    if (!isfile(binlog)) {
        snprintf(binlog, PATH_MAX, "%s/bin.log", g_cf->datadir);
    }
    zz_check(conn);
    if (check_binlog_local(conn, log_ver, log_line) == 0) {
        if (bcount != 0)
            ret = synclog_read_data(binlog, log_line - bcount, log_line, md5local);
        DINFO("md5: %s, md5local: %s\n", md5, md5local);
        if (bcount == 0 || strcmp(md5local, md5) == 0) { 
            conn->status = SEND_LOG;
            conn->blogver = log_ver;
            conn->blogline = log_line;
            //g_runtime->syncmem->need_skip_one = FALSE;
            conn->need_skip_one = FALSE;
            DINFO("Found sync log file (version = %u)\n", log_ver);
            ret = conn_send_buffer_reply((Conn *)conn, CMD_SYNC_OK, NULL, 0);
            zz_check(conn->rbuf);
            zz_check(conn);
            zz_check(conn->wbuf);
        } else {
            conn->status = NOT_SEND;
            ret = conn_send_buffer_reply((Conn *)conn, CMD_SYNC_MD5_ERROR, NULL, 0);
            DINFO("Not found syn log file (version %u) having log record %d\n", log_ver, log_line);
        }
    } else {
        conn->status = NOT_SEND;
        ret = conn_send_buffer_reply((Conn *)conn, CMD_SYNC_FAILED, NULL, 0);
        DINFO("Not found syn log file (version %u) having log record %d\n", log_ver, log_line);
    }
    return ret;
}

//获取从传递上来的命令
int
sdata_ready(Conn *c, char *data, int datalen)
{
    int ret;
    unsigned char cmd;
    SyncConn *conn = (SyncConn *)c;
    SThread *st;
    SyncConnInfo *conninfo = NULL;

    st = (SThread *)conn->thread;
    
    int i;
    for (i = 0; i < g_cf->max_sync_conn; i++) {
        conninfo = &(st->sync_conn_info[i]);
        if (conninfo->fd == conn->sock)
            break;
    }
    
    
    memcpy(&cmd, data + sizeof(int), sizeof(char));
    conn->cmd = cmd;
    char buf[256] = {0};
    DINFO("data ready cmd: %d, data: %s\n", cmd, formath(data, datalen, buf, 256));
    switch (cmd) {
        //同步bin.log
        case CMD_SYNC:
            ret = cmd_sync(conn, data, datalen);
            if (conninfo) {
                conninfo->status = CMD_SYNC;
                conninfo->cmd_count++;
            }
            break;
        //同步dump.dat
        case CMD_GETDUMP:
            ret = cmd_get_dump(conn, data, datalen);
            if (conninfo) {
                conninfo->status = CMD_GETDUMP;
                conninfo->cmd_count++;
            }
            break;

        default:
            ret = MEMLINK_ERR_CLIENT_CMD;
            break;
    }
    DINFO("conn_send_buffer_reply return: %d\n", ret);

    return ret;
}

void
sync_conn_destroy(Conn *c)
{
    SThread * st;
    SyncConnInfo *sconninfo = NULL;

    DINFO("destroy sync connection, fd: %d\n", c->sock);
    SyncConn *conn = (SyncConn*) c;
    event_del(&conn->sync_check_interval_evt);
    event_del(&conn->sync_write_evt);
    event_del(&conn->sync_read_evt);
    //event_del(&conn->evt);
    //close(conn->sock);
    if (conn->dump_fd > 0) {
        close(conn->dump_fd);
    }
    if (conn->synclog != NULL && conn->synclog->fd > 0) {
        synclog_destroy(conn->synclog);
        conn->synclog = NULL;
    }
    st = (SThread *)conn->thread;
    if (st) {
        sconninfo = st->sync_conn_info;
        st->conns--;
    }
    int i;
    if (sconninfo) {
        for (i = 0; i < g_cf->max_sync_conn; i++) {
            if (conn->sock == sconninfo[i].fd) {
                memset(&sconninfo[i], 0x0, sizeof(SyncConnInfo));
                break;
            }
        }
    }
    

    conn_destroy((Conn*)conn);
}


static void
sthread_read(int fd, short event, void *arg)
{
    SThread *st = (SThread*) arg;
    SyncConn *conn;

    DINFO("sthread_read...\n");
    conn = (SyncConn *)conn_create(fd, sizeof(SyncConn));

    if (conn) {
        conn->port = g_cf->sync_port;
        conn->base = st->base;
        conn->ready = sdata_ready;
        conn->destroy = sync_conn_destroy;
        conn->wrote = sync_conn_wrote;

        if (conn_check_max((Conn*)conn) == MEMLINK_ERR_CONN_TOO_MANY) {
            DERROR("too many sync conn.\n");
            conn->destroy((Conn*)conn);
            return;
        }
        
        int i;
        SyncConnInfo *sconninfo = st->sync_conn_info;

        st->conns++;
        
        for (i = 0; i < g_cf->max_sync_conn; i++) {
            sconninfo = &(st->sync_conn_info[i]);
            if (sconninfo->fd == 0) {
                sconninfo->fd = conn->sock;
                strcpy(sconninfo->client_ip, conn->client_ip);
                sconninfo->port = conn->client_port;
                memcpy(&sconninfo->start, &conn->ctime, sizeof(struct timeval));
                break;
            }
        }
        conn->thread = st;
        
        DINFO("new conn: %d\n", conn->sock);
        zz_check(conn);
        int ret = change_event((Conn *)conn, EV_READ | EV_PERSIST, 0, 1);
        zz_check(conn);
        if (ret < 0) {
            DERROR("change_evnet error: %d, close conn.\n", ret);
            sync_conn_destroy((Conn *)conn);
        }
    }
    return ;
}

void
sig_master_handler()
{
    DINFO("this is master\n");
}

void *
sthread_run(void *arg)
{
    struct sigaction sigact;

    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;

    sigact.sa_handler = sig_master_handler;
    sigaction(SIGUSR1, &sigact, NULL);

    SThread *st = (SThread *)arg;
    DINFO("sthread_loop...\n");
    event_base_loop(st->base, 0);
    return NULL;
}

//创建主线程
SThread *
sthread_create()
{
    SThread *st = (SThread *)zz_malloc(sizeof(SThread));
    if (st == NULL) {
        DERROR("memlink malloc sthread error.\n");
        MEMLINK_EXIT;
    }
    memset(st, 0x0, sizeof(SThread));

    st->sync_conn_info = (SyncConnInfo *)zz_malloc(sizeof(SyncConnInfo) * g_cf->max_sync_conn);
    if (st->sync_conn_info == NULL) {
        DERROR("memlink malloc stread connect info error. \n");
        MEMLINK_EXIT;
    }
    memset(st->sync_conn_info, 0x0, sizeof(SyncConnInfo) * g_cf->max_sync_conn);

    st->sock = tcp_socket_server(g_cf->host, g_cf->sync_port);
    if (st->sock == -1)
        MEMLINK_EXIT;
    DINFO("sync thread socket creation ok!\n");

    st->base = event_base_new();
    event_set(&st->event, st->sock, EV_READ | EV_PERSIST, sthread_read, st);
    event_base_set(st->base, &st->event);
    event_add(&st->event, 0);

    g_runtime->sthread = st;

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
    ret = pthread_create(&threadid, &attr, sthread_run, st);
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("pthread_create error: %s\n",  errbuf);
        MEMLINK_EXIT;
    }


    ret = pthread_detach(threadid);
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("pthread_detach error; %s\n",  errbuf);
        MEMLINK_EXIT;
    }

    DINFO("create sync thread ok\n");
    
    return st;
}
/**
 * @}
 */
