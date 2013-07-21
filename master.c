#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef __linux
#include <linux/if.h>
#endif
#include <sys/un.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/time.h>
#include <netdb.h>
#include <signal.h>

#include "master.h"
#include "wthread.h"
#include "serial.h"
#include "network.h"
#include "myconfig.h"
#include "logfile.h"
#include "dumpfile.h"
#include "rthread.h"
#include "sthread.h"
#include "base/zzmalloc.h"
#include "base/utils.h"
#include "common.h"
#include "sslave.h"
#include "runtime.h"
#include "commitlog.h"
#include "heartbeat.h"
#include "serial.h"
#include "base/pack.h"


void
mb_conn_destroy_delay(Conn *conn)
{
    if (conn->is_destroy)
        return;
    conn->is_destroy = TRUE;
    event_del(&conn->evt);

    struct timeval tm;
    evtimer_set(&conn->evt, mb_conn_destroy, conn);
    evutil_timerclear(&tm);
    tm.tv_sec = 0;
    event_base_set(conn->base, &conn->evt);
    event_add(&conn->evt, &tm);

    return;
}

int
mb_data_ready(Conn *conn, char *data, int datalen)
{
    short  ret;
    unsigned char   cmd;
    WThread *wt = (WThread *)conn->thread;
    BackupInfo *binfo = wt->backup_info;
    Conn *ctlconn = binfo->ctlconn;
    char *cmddata;
    int  cmdlen = 0;
    int logver, logline;

    cmd_backup_ack_unpack(data, &cmd, &ret, &logver, &logline);

    if (cmd == CMD_WRITE_RESULT) {
        return 0;
    }
    
    DINFO("=====logver: %d, logline: %d\n", logver, logline);
    DINFO("=====g_runtime syclog: logver: %d, logline: %d\n", g_runtime->synclog->version, g_runtime->synclog->index_pos);
    if (ret == MEMLINK_OK && (logver != g_runtime->synclog->version || logline != g_runtime->synclog->index_pos))
        return 0;

    if (binfo->succ + 1 == g_runtime->servernums / 2 + 1)
        return 0;

    binfo->succ++;
    
    cmddata = binfo->cltcmd + sizeof(int)*2;
    cmdlen = binfo->cmdlen - sizeof(int)*2;
    DINFO("-------------------cmdlen: %d\n", cmdlen);
    //DINFO("done: %d, succ: %d\n", binfo->done, binfo->succ);
    DINFO("======succ: %d, servernuns: %d\n", binfo->succ, g_runtime->servernums);
    //备应答的个数满足要求
    if ((binfo->succ + 1 == g_runtime->servernums / 2 + 1)) {
        //if ((binfo->succ + 1 == g_runtime->servernums)) {
        //if (binfo->timer_send == FALSE) {
        pthread_mutex_lock(&g_runtime->mutex);
        ret = wdata_apply(cmddata, cmdlen, MEMLINK_WRITE_LOG, ctlconn);
        pthread_mutex_unlock(&g_runtime->mutex);
        DINFO("g_runtime->index_pos: %d\n", g_runtime->synclog->index_pos);
        DINFO("g_runtime->pos: %d\n", g_runtime->synclog->pos);

        binfo->last_cmd_time = time(NULL);
        //DINFO("write to commitlog\n");
        //commitlog_write(wt->clog, g_runtime->synclog->version, g_runtime->synclog->index_pos - 1, data, datalen);
        memcpy(wt->clog->data, binfo->cltcmd, binfo->cmdlen);
        wt->clog->len = binfo->cmdlen;

        if (ret == MEMLINK_REPLIED || ret >= 0) {
            wt->clog->state = COMMITED;
        } else {
            wt->clog->state = ROLLBACKED;
        }
        if (ret != MEMLINK_REPLIED) {
            conn_send_buffer_reply(ctlconn, ret, NULL, 0);
        }
        event_del(&binfo->timer_check_evt);
        //} else {

        //event_del(&binfo->timer_check_evt2);
        //}
    } 
    /*
    else if (binfo->done == g_runtime->servernums - 1) {
        //不满足条件
        wt->state = STATE_NOCONN;
        conn_send_buffer_reply(ctlconn, MEMLINK_ERR_NOWRITE, NULL, 0);
    }
    */
    return 0;
}

void
mb_data_timeout(int fd, short event, void *arg)
{
    WThread *wt = g_runtime->wthread;
    BackupInfo *binfo = wt->backup_info;
    int alive = 0;

    DNOTE("-------------------------------------------------binfo->succ: %d\n", binfo->succ);
    if (binfo->succ  + 1 < g_runtime->servernums / 2 + 1) {
        BackupItem *bitem = binfo->item;
        while (bitem) {
            if (bitem->mbconn)
                alive++;
            bitem = bitem->next;
        }
        if (alive + 1 < g_runtime->servernums / 2 + 1) {
            Conn *conn = (Conn *)arg;
            conn_send_buffer_reply(conn, MEMLINK_ERR_NOWRITE, NULL, 0);
            wt->state = STATE_NOCONN;
        }
    }
    return;
}

int
mb_conn_wrote(Conn *conn)
{
    WThread *wt = g_runtime->wthread;
    MBconn *mbconn = (MBconn *)conn;
    BackupItem *bitem = (BackupItem *)mbconn->item;
    BackupInfo *binfo = wt->backup_info;
    
    
    DINFO("master write to backup complete! then do nothing.\n");
    event_del(&conn->evt);
    
    bitem->state = STATE_ALLREADY;
    bitem->time = time(NULL);
    binfo->succ_conns++;

    mbconn->wrote = conn_wrote;

    if (binfo->succ_conns + 1 >= g_runtime->servernums / 2 + 1) {
        wt->state = STATE_ALLREADY;
    }
    /*
    int ret = change_event(conn, EV_READ|EV_PERSIST, g_cf->backup_timeout, 0);
    if (ret < 0) {
        DERROR("change event error:%d close socket\n", ret);
        conn->destroy(conn);
    }
    */
    return 0;
}

//主连接备的时候结构的销毁
void
mb_conn_destroy(int fd, short event, void *arg)
{
    WThread *wt;
    BackupInfo *binfo;
    BackupItem *bitem;
    MBconn *mbconn = (MBconn *)arg;
    
    zz_check(mbconn);
    
    /*
    if (mbconn->wbuf) {
        zz_free(mbconn->wbuf);
        mbconn->wbuf = NULL;
    }

    if (mbconn->rbuf) {
        zz_free(mbconn->rbuf);
        mbconn->rbuf = NULL;
    }
    */
    
    /*
    event_del(&conn->evt);
    if (mbconn->sock > 0)
        close(mbconn->sock);
    */
    wt = (WThread *)mbconn->thread;
    binfo = wt->backup_info;
    bitem = (BackupItem *)mbconn->item;
     
    DINFO("====================destroy in connect: %s:%d\n", bitem->ip, bitem->write_port);
    bitem->state = STATE_NOWRITE;
    //zz_free(mbconn);
    bitem->mbconn = NULL;
    //bitem->is_conn_destroy = TRUE;
    conn_destroy((Conn *)mbconn);

    return;
}

int
master_ready(Conn *conn, char *data, int datalen)
{
    WThread *wt = g_runtime->wthread;
    //int ret;
    int lastlogver, lastlogline;
    //int lastret;

    commitlog_read(wt->clog, &lastlogver, &lastlogline);
    DINFO("=======lastlogver: %d, lastlogline: %d\n", lastlogver, lastlogline);
    //DNOTE("=======lastlogver: %d, lastlogline: %d\n", lastlogver, lastlogline);
    //写本地commitlog
    //commitlog_write(wt->clog, g_runtime->synclog->version, g_runtime->synclog->index_pos, 
        //data, datalen);

    int newlogver, newlogline;

    //commitlog_read(wt->clog, &newlogver, &newlogline);
    newlogver = g_runtime->synclog->version;
    newlogline = g_runtime->synclog->index_pos;

    //DNOTE("=======newlogver: %d, newlogline: %d\n", newlogver, newlogline);
    DINFO("=======newlogver: %d, newlogline: %d\n", newlogver, newlogline);
    //DNOTE("in synclog version: %d, logline: %d\n", g_runtime->synclog->version,   g_runtime->synclog->index_pos);
    DINFO("in synclog version: %d, logline: %d\n", g_runtime->synclog->version,   g_runtime->synclog->index_pos);
    /* 
    int dlen = wt->clog->len + sizeof(char);
    char *buffer = conn_write_buffer(conn, dlen); 
    
    ret = pack(buffer, 0, "$4cC", dlen, CMD_WRITE, wt->clog->len, wt->clog->data); 
    */
    BackupInfo *binfo = wt->backup_info;
    BackupItem *bitem  = binfo->item;
    
    //保存客户端命令
    pack(binfo->cltcmd, 0, "ii", newlogver, newlogline);
    memcpy(binfo->cltcmd+sizeof(int)*2, data, datalen);
    binfo->cmdlen = datalen + sizeof(int)*2;

    //binfo->done = 0;
    binfo->succ = 0;
    
    DINFO("state: %d, succ: %d\n", wt->state, binfo->succ);
    //系统不可用
    if (wt->state == STATE_NOCONN) {
        conn_send_buffer_reply(conn, MEMLINK_ERR_NOWRITE, NULL, 0); 
        return 0;
    }
    
    //while (binfo->timer_send == TRUE) {
    //}

    binfo->ctlconn = conn;
    DINFO("need send comand to all backup\n");
    while (bitem) {
        if(bitem->state == STATE_ALLREADY) {
            Conn *conn = (Conn *)bitem->mbconn;
            //DNOTE("-------------------wlen: %d, wpos: %d\n", conn->wlen, conn->wpos);
            DINFO("-------------------wlen: %d, wpos: %d\n", conn->wlen, conn->wpos);
            char *buffer = conn_write_buffer(conn, 2048);
            int  count;
            int  ret;
            
            DINFO("clog->len: %d\n", wt->clog->len);
            //count = pack(buffer, 0, "$4ciiiC:4", CMD_WRITE, lastlogver, lastlogline, wt->clog->state, wt->clog->len , wt->clog->data);
            count = pack(buffer, 0, "$4ciiiC:4", CMD_WRITE, lastlogver, lastlogline, wt->clog->state, binfo->cmdlen, binfo->cltcmd);
            int lastver, lastline, lastret, logver, logline;
            
            unpack(buffer+sizeof(int)+sizeof(char), 0, "iii", &lastver, &lastline, &lastret);
            DINFO("lastver: %d, lastline: %d, lastret: %d\n", lastver, lastline,
                lastret);

            unpack(buffer+sizeof(int)*5+sizeof(char), 0, "ii", &logver, &logline);
            DINFO("unpack logver: %d, logline: %d\n", logver, logline);
            conn->wlen = count;
            
            DINFO("change event to write\n");
            //DNOTE("change event to write\n");
            ret = change_event((Conn *)bitem->mbconn, EV_WRITE | EV_PERSIST, 0, 0);
            /*
            if (ret < 0) {
                DERROR("change_event error: %d, %s:%d\n", ret, bitem->ip, bitem->write_port);
                Conn *conn = (Conn *)bitem->mbconn;
                conn->destroy((Conn *)bitem->mbconn);
                binfo->succ_conns--;
            }
            */
        }
        bitem = bitem->next;
    }
    
    DINFO("=======add timer check event\n");
    struct timeval tm;
    evtimer_set(&binfo->timer_check_evt, mb_data_timeout, conn);
    evutil_timerclear(&tm);
    tm.tv_sec = 1;
    event_base_set(wt->base, &binfo->timer_check_evt);
    event_add(&binfo->timer_check_evt, &tm);

    return 0;
}

MBconn*
master_connect_backup(BackupItem *bitem)
{
    char    *ip;
    int     write_port;
    MBconn  *mbconn;
    WThread *wt = g_runtime->wthread;
    //int     error = 0;
    
    ip = bitem->ip;
    write_port = bitem->write_port;
    if (bitem->mbconn && bitem->mbconn->sock > 0) {
        DINFO("=====================destroy mbconn\n");
        //bitem->mbconn->destroy((Conn *)bitem->mbconn);
        mb_conn_destroy_delay((Conn *)bitem->mbconn);
        bitem->state = STATE_NOWRITE;
        bitem->mbconn = NULL;
    }

    mbconn = (MBconn *)conn_client_create(ip, write_port, sizeof(MBconn));
    if (mbconn == NULL) {
        DERROR("can not create mbconn struct\n");
        return NULL;
    }
    DINFO("mbconn->rbuf: %p\n", mbconn->rbuf);
    
    mbconn->item    = bitem; 
    mbconn->thread  = wt;
    mbconn->destroy = mb_conn_destroy_delay;
    mbconn->base    = wt->base;
    bitem->state    = STATE_NOWRITE; 
    mbconn->ready   = mb_data_ready;
    mbconn->wrote   = mb_conn_wrote;
    
    Conn *conn = (Conn *)mbconn;
    char *buffer = conn_write_buffer(conn, 1024);
    int count;

    g_cf->heartbeat_port = 30000;
    count = pack(buffer, 0, "$4ciiii", CMD_GETPORT, g_cf->write_port, g_cf->read_port, g_cf->sync_port, g_cf->heartbeat_port);
    //DINFO("--------------------count:%d\n", count);
    conn->wlen = count;
    change_event((Conn *)mbconn, EV_WRITE|EV_PERSIST, 0, 1); 

    return mbconn;
}

//主启动后驻都连备
int
master_connect_backup_all(WThread *wt)
{
    BackupInfo *binfo = NULL;
    BackupItem *bitem;
    int noconn = 0;
    
    DINFO("------master connect backup\n");
    wt->state = STATE_NOCONN;
    binfo = wt->backup_info;
    bitem  = binfo->item;
    while (NULL != bitem) {
        DINFO("backup info: %s:%d\n", bitem->ip, bitem->write_port);
        //bitem->mbconn = (MBconn *)conn_client_create(bitem->ip, bitem->write_port, sizeof(MBconn));
        bitem->mbconn = master_connect_backup(bitem);
        bitem->is_conn_destroy = FALSE;
        if (!bitem->mbconn) {
            noconn++;
            
            DINFO("can not conenct %s: %d\n", bitem->ip, bitem->write_port);
        }
        bitem = bitem->next;
    }

    //DNOTE("can not conenct %s: %d\n", bitem->ip, bitem->write_port);
    //注册定时检测事件， 检测在没有客户端连接的上来的情况下，发送上次执行的结果给备
    struct timeval tm;
    evtimer_set(&wt->backup_info->m_send_evt, mb_send_timeout, &wt->backup_info->m_send_evt);
    evutil_timerclear(&tm);
    tm.tv_sec = 3;
    event_base_set(wt->base, &wt->backup_info->m_send_evt);
    event_add(&wt->backup_info->m_send_evt, &tm);
    //DNOTE("there is %d server can't connected\n", noconn);
    return 0;
}

void
mb_send_timeout(int fd, short event, void *arg)
{
    WThread *wt = g_runtime->wthread;
    BackupInfo *binfo = wt->backup_info;
    BackupItem *bitem = binfo->item;
    int lastlogver, lastlogline;
    
    DINFO("=========================in mb_send_timeout\n"); 

    struct timeval tm;
    evutil_timerclear(&tm);
    tm.tv_sec = 3;
    event_add(&wt->backup_info->m_send_evt, &tm);
 
    if (wt->state == STATE_NOCONN) {
        return;
    }

    if (wt->clog->state != COMMITED)
        return;
    
    uint64_t now;
    now = time(NULL);
    if (now - binfo->last_cmd_time > 3) {
        commitlog_read(wt->clog, &lastlogver, &lastlogline);
        DINFO("=========lastlogver: %d, lastlogline: %d\n", lastlogver, lastlogline);
        //binfo->succ = 0;
        
        DINFO("===================send result to backup\n");
        while (bitem) {
            if(bitem->state == STATE_ALLREADY) {
                Conn *conn = (Conn *)bitem->mbconn;
                char *buffer = conn_write_buffer(conn, 4098);
                int  count;

                count = pack(buffer, 0, "$4ciii", CMD_WRITE_RESULT, wt->clog->state, lastlogver, lastlogline);
                conn->wlen = count;
                
                //if (binfo->timer_send == FALSE)
                    //binfo->timer_send = TRUE;

                conn_send_buffer(conn);
            }
            bitem = bitem->next;
        }
        
        /*
        struct timeval tm;
        evtimer_set(&binfo->timer_check_evt2, mb_data_timeout, NULL);
        evutil_timerclear(&tm);
        tm.tv_sec = 1;
        event_base_set(wt->base, &binfo->timer_check_evt2);
        event_add(&binfo->timer_check_evt2, &tm);*/
    }

    return;   
}

int
master_init(int voteid, char *data, int datalen)
{
    int ret;
    WThread *wt = g_runtime->wthread;
    
    wt->clog = commitlog_create();
    g_cf->role = ROLE_MASTER;
    
    wt->backup_info = (BackupInfo *)zz_malloc(sizeof(BackupInfo));

    
    DNOTE("============I am the master\n");
    if (wt->backup_info == NULL) {
        DERROR("malloc BackupInfo struct error\n");
        MEMLINK_EXIT;
    }

    memset(wt->backup_info, 0x0, sizeof(BackupInfo));

    BackupInfo *binfo = wt->backup_info;
    g_runtime->servernums = 1;
    g_runtime->voteid = voteid;
    binfo->timer_send = FALSE;

    int  count = 0;
    char ip[16] = {0};
    uint16_t port;

    DINFO("datalen : %d\n", datalen);
    while (count < datalen) {
        ret = unpack_votehost(data + count, ip, &port);
        DINFO("ip: %s port: %d\n", ip, port);
        count += ret;
        BackupItem *bitem = (BackupItem *)zz_malloc(sizeof(BackupItem));
        if (bitem == NULL) {
            DERROR("malloc BackupItem struct error\n");
            MEMLINK_EXIT;
        }
        memset(bitem, 0x0, sizeof(BackupItem));
        strcpy(bitem->ip, ip);
        bitem->write_port = port;
        DINFO("backup ip, port: %s:%d\n", ip, port);
        bitem->next = binfo->item;
        binfo->item = bitem;
        g_runtime->servernums++;
    }

    master_create_heartbeat(wt);
    //初始化backupinfo结构
    ret = master_connect_backup_all(wt);
    return 0;
}

//备切换成主
int 
switch_master(int voteid, char *data, int datalen)
{
    WThread    *wt = g_runtime->wthread;
    MasterInfo *minfo = wt->master_info;

    if (g_cf->role == ROLE_MASTER) {
        DERROR("master can not switch to master\n");
        return -1;
    } else if (g_cf->role == ROLE_BACKUP) {
        DINFO("backup switch to master\n");
        if (minfo != NULL) {
            //销毁备上面以前保存得主节点得信息
            event_del(&minfo->hb_timer_evt);
            event_del(&minfo->hbevt);
            if (minfo->hbsock > 0)
                close(minfo->hbsock);
            minfo->hbsock = -1;
            wt->master_info = NULL;
            
            zz_free(minfo);
            //构建主节点上得备节点信息
            master_init(voteid, data, datalen);
            if (wt->clog->state == INCOMPLETE) {
                int ret;
                int cmdlen;
                char *cmd = commitlog_get_cmd(wt->clog, &cmdlen);

                pthread_mutex_lock(&g_runtime->mutex);
                ret = wdata_apply(cmd, cmdlen, MEMLINK_NO_LOG, NULL);
                DINFO("===wdata_apply ret: %d\n", ret);
                if ( ret == 0) {
                    int sret;

                    sret = synclog_write(g_runtime->synclog, wt->clog->data, wt->clog->len);
                    if (sret < 0) {
                        DERROR("synclog_write error: %d\n", sret);
                        MEMLINK_EXIT;
                    }   
                    char *cmd;
                    int  cmdlen;

                    cmd = commitlog_get_cmd(wt->clog, &cmdlen);
                    sret = syncmem_write(g_runtime->syncmem, cmd, cmdlen, 
                        g_runtime->synclog->version, g_runtime->synclog->index_pos - 1); 
                    if (sret < 0) {
                        DERROR("syncmem_write error: %d\n", sret);
                        MEMLINK_EXIT;
                    }   
                }
                pthread_mutex_unlock(&g_runtime->mutex);
            }
        } else {
            DERROR("master info is null, can not switch\n");
            return -3;
        }
    }
    return 0; 
}

//主上收到仲裁备节点信息变动
int
mb_binfo_update(int voteid, char *data, int datalen)
{
    WThread *wt = g_runtime->wthread;
    BackupInfo *binfo = wt->backup_info;
    BackupItem *bitem;

    //释放以前所有备节点得信息
    while (binfo->item) {
        bitem = binfo->item;
        binfo->item = bitem->next;

        if (bitem->mbconn && bitem->mbconn->sock > 0)
            //bitem->mbconn->destroy((Conn *)bitem->mbconn);
            DNOTE("destroy mbconn\n");
            mb_conn_destroy_delay((Conn *)bitem->mbconn);
        bitem->mbconn = NULL;
        bitem->state  = STATE_NOWRITE;
        zz_free(bitem);
    }
    
    close(wt->backup_info->hbsock);
    event_del(&wt->backup_info->hbevt);
    event_del(&wt->backup_info->timer_check_evt);
    zz_free(wt->backup_info);
    master_init(voteid, data, datalen);
    return 0; 
}
