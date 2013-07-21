#include <stdlib.h>
#include <unistd.h>
#include "backup.h"
#include "common.h"
#include "runtime.h"
#include "myconfig.h"
#include "serial.h"
#include "base/zzmalloc.h"
#include "base/pack.h"
#include "base/logfile.h"
#include "base/utils.h"
#include "heartbeat.h"
#include "master.h"

int
switch_backup(int voteid, char *data, int datalen)
{
    WThread *wt = g_runtime->wthread;

    if (g_cf->role == ROLE_MASTER) {
        //释放主节点上保存得备节点得信息
        BackupInfo *binfo = wt->backup_info;
        BackupItem *bitem;

        while(binfo->item) {
            bitem = binfo->item; 
            MBconn *mbconn = bitem->mbconn;

            if (mbconn)
                //mbconn->destroy((Conn *)mbconn);
                mb_conn_destroy_delay((Conn *)mbconn);
            bitem->mbconn = NULL;
            binfo->item = bitem->next;
            zz_free(bitem);
        }
        backup_init(voteid, data, datalen);
    } else if (g_cf->role == ROLE_BACKUP) {
        MasterInfo *minfo = wt->master_info;
        if (minfo == NULL) {
            DERROR("master info is null\n");
            return -2;
        }
        event_del(&minfo->hb_timer_evt);
        event_del(&minfo->hbevt);
        if (minfo->hbsock > 0) {
            close(minfo->hbsock);
            minfo->hbsock = -1;
        }
        /*
        if (minfo->conn)
            minfo->conn->destroy(minfo->conn);
        minfo->conn = NULL;
        */

        zz_free(minfo);
        wt->master_info = NULL;

        backup_init(voteid, data, datalen);
    } else {
        DINFO("role info error: %d\n", g_cf->role);
        return -1;
    }
    return 0;
}

int 
backup_init(int voteid, char *data, int datalen)
{
    //int voteid;
    char      ip[16] = {0};
    uint16_t  port;

    //unpack(data, 0, "ish", &voteid, ip, &port);

    //DINFO("backup init, voteid:%d, ip:%s, port:%d\n", voteid, ip, port);
    //strcpy(ip, "127.0.0.1");
    //port = 31002;
    WThread *wt = g_runtime->wthread;
    g_cf->role = ROLE_BACKUP;
    
    DNOTE("============I am backup\n");
    wt->master_info = zz_malloc(sizeof(MasterInfo));
    if (wt->master_info == NULL) {
        DERROR("malloc MasterInfo struct error\n");
        MEMLINK_EXIT;
    }
    /*
    if (wt->master_info == NULL) {
        wt->master_info = zz_malloc(sizeof(MasterInfo));
        memset(wt->master_info, 0, sizeof(MasterInfo));
        // 添加heartbeat事件
    }
    */

    MasterInfo *mi = wt->master_info;
    memset(mi, 0x0, sizeof(MasterInfo));
    g_runtime->voteid = voteid;
    
    int count;

    count = unpack_votehost(data, ip, &port);

    DINFO("unpack master ip: %s, write_port: %d\n", ip, port);
    strcpy(mi->ip, ip);
    mi->write_port = port;
    //wt->vote_id = voteid;
    //strncpy(mi->ip, ip, 15);
    //mi->write_port = port;
    //mi->hb_port = 30000;
    wt->state = STATE_ALLREADY;
    mi->hb_port = g_cf->heartbeat_port;

    if (wt->clog == NULL) {
        wt->clog = commitlog_create();
        //wt->clog->state = INCOMPLETE;
    }
    
    backup_create_heartbeat(wt);

    return MEMLINK_OK;
}

static int
backup_return_not_master(Conn *conn, MasterInfo *mi)
{
    char *buf = conn_write_buffer(conn, 32);
    int len = pack(buf, 32, "$4hsh", MEMLINK_ERR_NOT_MASTER, mi->ip, mi->write_port);
    conn->wlen = len;
    conn_send_buffer(conn);
    return MEMLINK_REPLIED;
}

int
backup_sync()
{
    WThread     *wt = g_runtime->wthread;
    SSlave      *ss = g_runtime->slave;
    wt->state = STATE_SYNC;
    
    DINFO("start backup sync thread.\n");
    if (g_runtime->slave) {
        //sslave_thread(g_runtime->slave);
        ss->isrunning = TRUE;
    } else {
        g_runtime->slave = sslave_create();
        sslave_go(g_runtime->slave);
    }    
    //g_cf->role = ROLE_SLAVE;

    return 0;
}

int 
backup_sync_close()
{
    DINFO("close backup sync thread.\n");
    SSlave *ss = g_runtime->slave;
    
    ss->isrunning = FALSE;
    return 0;
}

static int
backup_ack(Conn *conn, char cmd, short ret)
{
    int  count;
    char *wbuf = conn_write_buffer(conn, 32); 
    
    count = pack(wbuf, 32, "$4chii", cmd, ret, g_runtime->synclog->version, g_runtime->synclog->index_pos); 
    conn->wlen = count;
    conn_send_buffer(conn);
    return 0;
}

void clean2(void *arg)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)arg;
    pthread_mutex_unlock(mutex);
}

static int
backup_last_do(Conn *conn, char cmd)
{
    WThread     *wt = g_runtime->wthread;
    int ret = 0;
    int cmdlen;
    int lastver, lastline;
    //int count; 
    //char *wbuf = conn_write_buffer(conn, 32);

    commitlog_read(wt->clog, &lastver, &lastline);
    
    //从已经写入binlog
    if (lastver == g_runtime->synclog->version && lastline < g_runtime->synclog->index_pos) {
        DNOTE("SLAVE have write to binlog...\n");
        goto ack_master;
    }

    char *cmddata = commitlog_get_cmd(wt->clog, &cmdlen); // 有可能数据有问题
    DINFO("last do, cmdlen:%d\n", cmdlen);
    DINFO("=====lock\n");
    pthread_cleanup_push(clean2, (void *)&g_runtime->mutex);
    pthread_mutex_lock(&g_runtime->mutex);
    ret = wdata_apply(cmddata, cmdlen, MEMLINK_NO_LOG, NULL);
    DINFO("========ret: %d\n", ret);
    if (ret == 0) {
        int sret = 0;
        
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
    pthread_cleanup_pop(0);
    DINFO("=====unlock\n");
    
    if (ret != MEMLINK_OK) {
        DINFO("restart slave\n");
        backup_sync();
    }
    // backup return 
ack_master:
    /*
    count = pack(wbuf, 32, "$4chii", cmd, ret, g_runtime->synclog->version, g_runtime->synclog->index_pos);
    
    conn->wlen = count;
    conn_send_buffer(conn);*/

    backup_ack(conn, cmd, ret);

    ret = MEMLINK_REPLIED;
    return ret;
}

static int
backup_write_go(Conn *conn, char *data, char cmd, int lastret)
{
    int ret = MEMLINK_OK;
    WThread     *wt = g_runtime->wthread;
    //DNOTE("wt->clog->state: %d\n", wt->clog->state);
    DINFO("wt->clog->state: %d\n", wt->clog->state);
    if (wt->clog->state == INCOMPLETE) { // 未完成
        DINFO("last commitlog incomplete.\n");
        if (lastret == COMMITED) {
            DINFO("do last log.\n"); 
            ret = backup_last_do(conn, cmd);
            //return ret;
        }else{
            DINFO("rollback last log.\n");
            /*
            int count; 
            char *wbuf = conn_write_buffer(conn, 32);
            count = pack(wbuf, 32, "$4chii", cmd, ret, g_runtime->synclog->version, g_runtime->synclog->index_pos);

            conn->wlen = count;
            conn_send_buffer(conn);*/

            backup_ack(conn, cmd, ret);
            ret = MEMLINK_REPLIED;

        }
        wt->clog->state = lastret;
    }
    /*
       int cmdlen = datalen-sizeof(int)*4+sizeof(char)*2;;
       DINFO("write commit log: %d\n", cmdlen);
       commitlog_write_data(wt->clog, data+sizeof(int)*4+sizeof(char)*2, cmdlen);
     */
    int cmdlen = 0;

    DINFO("=======write to commitlog\n");
    unpack(data+sizeof(int)*4+sizeof(char), 0, "i", &cmdlen);
    memcpy(wt->clog->data, data+sizeof(int)*5+sizeof(char), cmdlen);
    wt->clog->len = cmdlen;
    wt->clog->state = INCOMPLETE;

    if (lastret == 0) {
        /*
        int count; 
        char *wbuf = conn_write_buffer(conn, 32);
        count = pack(wbuf, 32, "$4chii", cmd, ret, g_runtime->synclog->version, g_runtime->synclog->index_pos);

        conn->wlen = count;
        conn_send_buffer(conn);*/
        backup_ack(conn, cmd, ret);

        ret = MEMLINK_REPLIED;
    }
    return ret;
}

int
backup_cmd_write(Conn *conn, char cmd, char* data, int datalen)
{
    WThread     *wt = g_runtime->wthread;
    int  logver, logline, lastver, lastline, lastret;

    unpack(data+sizeof(int)+sizeof(char), 0, "iii", 
            &lastver, &lastline, &lastret);

    unpack(data+sizeof(int)*5+sizeof(char), 0, "ii", &logver, &logline);
    //DNOTE("cmd write, lastver:%d, lastline:%d, lastret:%d, logver:%d, logline:%d\n",
            //lastver, lastline, lastret, logver, logline);   
    DINFO("cmd write, lastver:%d, lastline:%d, lastret:%d, logver:%d, logline:%d\n",
            lastver, lastline, lastret, logver, logline);   
    //DNOTE("synclog logver: %d, logline: %d\n", g_runtime->synclog->version, g_runtime->synclog->index_pos);
    DINFO("synclog logver: %d, logline: %d\n", g_runtime->synclog->version, g_runtime->synclog->index_pos);
    // 在SYNC状态可能切换回ALLREADY
    if (wt->state == STATE_SYNC) {
        if (g_runtime->synclog->version == logver && g_runtime->synclog->index_pos == logline) {
            DINFO("change state from SYNC to ALLREADY.\n");
            wt->state = STATE_ALLREADY;
            //g_runtime->slave->is_backup_do = TRUE;
            // 断开主从同步连接
            backup_sync_close();
            int cmdlen;
            unpack(data+sizeof(int)*4+sizeof(char), 0, "i", &cmdlen);
            memcpy(wt->clog->data, data+sizeof(int)*5+sizeof(char), cmdlen);
            wt->clog->len = cmdlen;
            wt->clog->state = INCOMPLETE;
            
            /*int count; 
            char *wbuf = conn_write_buffer(conn, 32);
            count = pack(wbuf, 32, "$4chii", cmd, MEMLINK_OK, g_runtime->synclog->version, g_runtime->synclog->index_pos);

            conn->wlen = count;
            conn_send_buffer(conn);*/
            backup_ack(conn, cmd, MEMLINK_OK);

            return MEMLINK_REPLIED;
        } else {
            /*int count; 
            char *wbuf = conn_write_buffer(conn, 32);
            count = pack(wbuf, 32, "$4chii", cmd, MEMLINK_ERR_SYNC, g_runtime->synclog->version, g_runtime->synclog->index_pos);

            conn->wlen = count;
            conn_send_buffer(conn);*/
            backup_ack(conn, cmd, MEMLINK_ERR_SYNC);
            return MEMLINK_REPLIED;
        }
    }
    
    DINFO("=============backup state: %d\n", wt->state);
    DINFO("=============clog state: %d\n", wt->clog->state);
    int ret = MEMLINK_OK;
    if (wt->state == STATE_ALLREADY) {
        int myver, myline, myret;
        commitlog_read(wt->clog, &myver, &myline);
        myret = wt->clog->state;
        //DNOTE("my ver:%d, line:%d, ret:%d\n", myver, myline, myret);
        DINFO("my ver:%d, line:%d, ret:%d\n", myver, myline, myret);
        if (myver != lastver || myline != lastline) {
            DINFO("logver, logline not equal, try backup sync.\n");
            backup_sync();

            /*int count; 
            char *wbuf = conn_write_buffer(conn, 32);
            count = pack(wbuf, 32, "$4chii", cmd, MEMLINK_ERR_SYNC, g_runtime->synclog->version, g_runtime->synclog->index_pos);

            conn->wlen = count;
            conn_send_buffer(conn);*/

            backup_ack(conn, cmd, MEMLINK_ERR_SYNC);
            return MEMLINK_REPLIED;
 
            //return MEMLINK_ERR_SYNC;
        }
        ret = backup_write_go(conn, data, cmd, lastret);
        return ret;
    }else{
        DINFO("state error:%d\n", wt->state);
        ret = MEMLINK_ERR_STATE;
    }
    return ret;
}

int
backup_cmd_write_result(Conn *conn, char cmd, char* data, int datalen)
{
    WThread     *wt = g_runtime->wthread;
    //MasterInfo  *mi = wt->master_info;

    int lastver, lastline, lastret;
    
    unpack(data+sizeof(int)+sizeof(char), 0, "iii", &lastret, &lastver, &lastline);
    DINFO("cmd write_result, lastver:%d, lastline:%d, lastret:%d\n", lastver, lastline, lastret);    
    if (wt->state == STATE_ALLREADY) {
        int myver, myline, myret;
        commitlog_read(wt->clog, &myver, &myline);
        myret = wt->clog->state;
        DINFO("myver: %d, myline: %d, lastver: %d, lastline: %d\n", myver, myline, lastver, lastline);
        if (myver != lastver || myline != lastline) {
            DINFO("lastver, lastline not equal, try backup sync.\n");
            backup_sync();
            backup_ack(conn, cmd, MEMLINK_ERR_SYNC);
            return MEMLINK_REPLIED;
        }
        int ret = 0;
        if (wt->clog->state == INCOMPLETE) { // 未完成
            if (lastret == COMMITED) {
                DINFO("commit last log.\n");
                ret = backup_last_do(conn, cmd);
                //return ret;
            }else{
                DINFO("rollback last log.\n");
                backup_ack(conn, cmd, MEMLINK_OK);
            }
            wt->clog->state = lastret;
        } else if (wt->clog->state == COMMITED) {
            /*int count;
            char *wbuf = conn_write_buffer(conn, 8);

            count = pack(wbuf, 8, "$4ch", cmd, ret, g_runtime->synclog->version, g_runtime->synclog->index_pos);
            conn->wlen = count;

            conn_send_buffer(conn);*/
            backup_ack(conn, cmd, ret);

            ret = MEMLINK_REPLIED;
        }

        return ret;
    } else if (wt->state == STATE_SYNC) {
       if (g_runtime->synclog->version == lastver && g_runtime->synclog->index_pos - 1 == lastline) {
           DINFO("change state from SYNC to ALLREADY.\n");
           wt->state = STATE_ALLREADY;
           //g_runtime->slave->is_backup_do = TRUE;
           // 断开主从同步连接
           backup_sync_close();
           int cmdlen;
           unpack(data+sizeof(int)*4+sizeof(char), 0, "i", &cmdlen);
           memcpy(wt->clog->data, data+sizeof(int)*5+sizeof(char), cmdlen);
           wt->clog->len = cmdlen;
           wt->clog->state = COMMITED;

           backup_ack(conn, cmd, MEMLINK_OK);

           return MEMLINK_REPLIED;
       } else {
           backup_ack(conn, cmd, MEMLINK_ERR_SYNC);
           return MEMLINK_REPLIED;
       }
    }
    return MEMLINK_OK;
}

int
backup_cmd_getport(Conn *conn, char* data, int datalen)
{
    WThread     *wt = g_runtime->wthread;
    MasterInfo  *mi = wt->master_info;
    
    //mi->conn = conn;
    unpack(data+sizeof(int)+sizeof(char), 0, "iiii", &mi->read_port, &mi->write_port, &mi->sync_port, &mi->hb_port);
    DINFO("read_port:%d, write port:%d, sync_port:%d, hb_port:%d\n", 
            mi->read_port, mi->write_port, mi->sync_port, mi->hb_port);    
    event_del(&conn->evt);
    g_cf->master_sync_port = mi->sync_port;
    strcpy(g_cf->master_sync_host, mi->ip);

    change_event(conn, EV_READ|EV_PERSIST, 0, 1);
    return MEMLINK_REPLIED;
}

int        
backup_ready(Conn *conn, char *data, int datalen)
{
    WThread     *wt = g_runtime->wthread;
    MasterInfo  *mi = wt->master_info;
    
    DINFO("backup ready, state:%d\n", wt->state);
    //mi->conn = conn;
    // 不是主的连接
    /*
    if (strcmp(conn->client_ip, mi->ip) != 0) {
        DERROR("not master ip!, client_ip: %s, maste ip: %s\n", conn->client_ip, mi->ip);
        return backup_return_not_master(conn, mi);
    }*/

    if (wt->state == STATE_NOWRITE) {
        DINFO("backup nowrite.\n");
        return MEMLINK_ERR_NOWRITE;
    }

    char cmd;
    memcpy(&cmd, data+sizeof(int), sizeof(char));

    switch(cmd) {
    case CMD_WRITE:
        DINFO("======CMD_WRITE\n");
        return backup_cmd_write(conn, cmd, data, datalen);
        break;
    case CMD_WRITE_RESULT:
        DINFO("======CMD_WRITE_RESULT\n");
        return backup_cmd_write_result(conn, cmd, data, datalen);
        break;
    case CMD_GETPORT:
        DINFO("=====CMD_GETPORT\n");
        return backup_cmd_getport(conn, data, datalen);
        break;
    default:
        return backup_return_not_master(conn, mi);
    }
        
    return MEMLINK_ERR;
}



