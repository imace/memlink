#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef __linux
#include <linux/if.h>
#endif

#include "wthread.h"
#include "logfile.h"
#include "myconfig.h"
#include "runtime.h"
#include "serial.h"
#include "master.h"
#include "base/network.h"
#include "base/conn.h"

int
set_item_state(WThread *wt, int state)
{
    BackupInfo *binfo = wt->backup_info;
    BackupItem *bitem = binfo->item;

    while (bitem != NULL) {
        bitem->state = state;
        bitem = bitem->next;
    }
    return 0;
}

void
master_hb_read(int fd, short event, void *arg)
{
    int ret;
    struct sockaddr_in c_addr;
    socklen_t addrlen = sizeof(c_addr);
    //Conn *conn = (Conn *)arg;
    WThread *wt = g_runtime->wthread;
    char buffer[256] = {0};
    int port;
    char *ip;

    if (event & EV_TIMEOUT) {
        DERROR("=============================no heartbeat found\n");
        wt->state = STATE_NOCONN;
        set_item_state(wt, STATE_NOWRITE);
        change_sock_event(wt->base, wt->backup_info->hbsock, EV_READ | EV_PERSIST, g_cf->heartbeat_timeout, 0, &wt->backup_info->hbevt,  master_hb_read, NULL);
        return;
    }

    DINFO("=================in master_hb_read\n");
    memset(buffer, 0x0, 256);
    memset(&c_addr, 0x0, sizeof(c_addr));
    //读取心跳包
    while (1) {
        ret = recvfrom(fd, buffer, 256, 0, (struct sockaddr *)&c_addr, &addrlen);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                char errbuf[1024];
                strerror_r(errno, errbuf, 1024);
                DERROR("recvfromm error: %d, %s\n", errno,  errbuf);
                //change_sock_event(conn, EV_READ | EV_PERSIST, 1, 0, master_hb_read);
                return;
            }
        } else if (ret == 0) {
            DERROR("recvfrom: %d\n", ret);
        }
        break;
    }
    //conn->rlen = ret;
    
    int hport;
    ip = inet_ntoa(c_addr.sin_addr);
    hport = ntohs(c_addr.sin_port);

    cmd_heartbeat_unpack(buffer, &port);
    DINFO("==================backup ip: %s, write_port: %d, heartbeat_port: %d\n", ip, port, hport);

    int find = 0;
    unsigned int now;
    now = time(NULL);
    BackupInfo *binfo = wt->backup_info;
    BackupItem *bitem = binfo->item;
    BackupItem *tbitem = NULL; 
    while (NULL != bitem) {
        DINFO("back up state: %d\n", bitem->state);
        if (strncmp(bitem->ip, ip, strlen(bitem->ip)) == 0 && port == bitem->write_port) {
            memcpy(&bitem->from_addr, &c_addr, sizeof(struct sockaddr_in));
            if (bitem->state == STATE_NOWRITE || (bitem->state == STATE_ALLREADY && now - bitem->time > g_cf->heartbeat_timeout)) {
                //备已经标示成不可用，再次检测到心跳，回复备为可用
                if (bitem->state == STATE_ALLREADY) {
                    bitem->state = STATE_NOWRITE;
                    binfo->succ_conns--;
                }
                DNOTE("=================need reconnect backup\n");
                bitem->mbconn = master_connect_backup(bitem);
                //mb_reconn(bitem);
            }
            memcpy(bitem->buffer, buffer, ret);
            bitem->blen = ret;
            //char *wbuffer = conn_write_buffer((Conn *)conn, 256);
            //memcpy(wbuffer, buffer, ret);
            find = 1;
            bitem->time = now;
            tbitem = bitem;
            /*
            char *fip;
            int  fport;
            fip = inet_ntoa(bitem->from_addr.sin_addr);
            fport = ntohs(bitem->from_addr.sin_port);
            */
        } else {
            if (bitem->state == STATE_ALLREADY) {
                //检测标记为可用的备， 看是否心跳超时
                if (now - bitem->time > g_cf->heartbeat_timeout) {
                    //某个备心跳检测超时,尝试重连
                    DINFO("===========need reconnect backup\n");
                    bitem->state = STATE_NOWRITE;
                    binfo->succ_conns--;
                    bitem->mbconn = master_connect_backup(bitem);
                    //mb_reconn(bitem);
                    /*
                    if (ret < 0) {
                        bitem->state = 0;
                        binfo->succ_conns--;
                    }
                    */
                }
            }
        }

        bitem = bitem->next;
    }

    if (binfo->succ_conns + 1 < g_runtime->servernums / 2 + 1) {
        //能检测到心跳的备的个数不满足条件，系统不可用
        wt->state = STATE_NOWRITE;
    }
    //设置写事件
    if (find == 1) {
        //strncpy(conn->client_ip, ip, 16);
        //conn->client_port = port;
        change_sock_event(wt->base, fd, EV_WRITE | EV_PERSIST, 0, 0, &wt->backup_info->hbevt,  master_hb_write, tbitem);
    } else {
        DERROR("dest server info is error\n");
        return;
    }
    return;
}

void
master_hb_write(int fd, short event, void *arg)
{
    int ret;
    int i;
    //Conn *conn = (Conn *)arg;
    WThread *wt = g_runtime->wthread;
    BackupItem *bitem = (BackupItem *)arg;
    //int find = 0;
    /*
    while (binfo != NULL) {
        if (strncmp(binfo->ip, conn->client_ip, 16 && binfo->write_port == conn->client_port) == 0) {
            //find == 1;
            break;
        }
        binfo = binfo->next;
    }
    */

    socklen_t addr_len = sizeof(bitem->from_addr);
    //发送三个心跳包
    DINFO("==============sendto backup\n");
    for (i = 0; i < 1; i++) {
        ret = sendto(fd, bitem->buffer, bitem->blen, 0, (struct sockaddr *)&(bitem->from_addr), addr_len);
        DINFO("================send heartbeat: %d\n", ret);
    }

    change_sock_event(wt->base, fd,  EV_READ | EV_PERSIST, g_cf->heartbeat_timeout, 0, &wt->backup_info->hbevt, master_hb_read, NULL);
    return;
}

void
backup_hb_write(int fd, short event, void *arg)
{
    int ret, count;
    WThread *wt = g_runtime->wthread;
    struct sockaddr_in send_addr;
    //socklen_t addr_len = sizeof(send_addr);
    char *host = wt->master_info->ip;
    int port = wt->master_info->hb_port;
    int sock = wt->master_info->hbsock;
    char buffer[256] = {0};

    if (sock == -1) {
        return;
    }
    
    memset(&send_addr, 0, sizeof(send_addr));
    send_addr.sin_family = AF_INET;
    send_addr.sin_port = htons((short)port);
    send_addr.sin_addr.s_addr = inet_addr(host);

    count = cmd_heartbeat_pack(buffer, g_cf->write_port);

    DINFO("=== hb === sendto master %s:%d, sock:%d, count:%d\n", host, port, sock, count);
    do{
        ret = sendto(sock, buffer, count, 0, (struct sockaddr *)&send_addr, sizeof(struct sockaddr_in));
    }while(ret == -1 && errno == EINTR);
    if (ret == -1) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DINFO("=== hb === ret: %d, %s\n", ret,  errbuf);
    }

    struct timeval tv;
    struct event *timeout = arg;
    evutil_timerclear(&tv);
    tv.tv_sec = 1;
    event_add(timeout, &tv);
    //change_sock_event(conn, EV_READ | EV_PERSIST, 5, 0, backup_hb_read);
    return;
}

void
backup_hb_read(int fd, short event, void *arg)
{
    int ret;
    char buffer[256] = {0};
    struct sockaddr_in from_addr;
    socklen_t addr_len;
    WThread *wt = g_runtime->wthread;

    if (event & EV_TIMEOUT) {
        DERROR("no heartbeat found\n");
        event_del(&wt->master_info->hb_timer_evt);
        event_del(&wt->master_info->hbevt);
        close(wt->master_info->hbsock);
        wt->master_info->hbsock = -1;
        uint64_t eid = 0;
        eid |= g_runtime->synclog->version;
        eid = eid << 32;
        eid |= g_runtime->synclog->index_pos;

        request_vote(eid, COMMITED, g_runtime->voteid+1, g_cf->write_port);
        /*
        Conn *conn = wt->master_info->conn; 
        if (conn) {
            conn->destroy(conn);
            wt->master_info->conn = NULL;
        }
        */
        // 仲裁
//        change_sock_event(fd, EV_READ, g_cf->heartbeat_timeout, 0, 
//                &wt->master_info->hbevt, backup_hb_read, NULL);
        return;
    }

    DINFO("=== hb === recv from master\n");
    ret = recvfrom(fd, buffer, 256, 0, (struct sockaddr *)&from_addr, &addr_len);
    if (ret == -1) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DINFO("=== hb === recvfrom: %d, %s\n", ret,  errbuf);
    }
    DINFO("=== hb === recvfrom: %d\n", ret);

    change_sock_event(wt->base, fd, EV_READ, g_cf->heartbeat_timeout, 0, 
            &wt->master_info->hbevt, backup_hb_read, NULL);

    return;
}

void
master_create_heartbeat(WThread *wt)
{
    //建立心跳
    g_cf->heartbeat_port = 30000;
    wt->backup_info->hbsock = udp_sock_server(g_cf->host, g_cf->heartbeat_port);
    if (wt->backup_info->hbsock < 0) {
        DERROR("udp_sock_server: %d\n", wt->backup_info->hbsock);
        MEMLINK_EXIT;
    }  
    DINFO("udp server create succ: %d\n", wt->backup_info->hbsock);
    change_sock_event(wt->base, wt->backup_info->hbsock, EV_READ | EV_PERSIST, g_cf->heartbeat_timeout, 1, &wt->backup_info->hbevt, master_hb_read, NULL);
    return;
}

void
backup_create_heartbeat(WThread *wt)
{
    struct timeval tm;
    
    wt->master_info->hb_port = 30000;
    evtimer_set(&wt->master_info->hb_timer_evt, backup_hb_write, &wt->master_info->hb_timer_evt);
    evutil_timerclear(&tm);
    tm.tv_sec = 1;
    event_base_set(wt->base, &wt->master_info->hb_timer_evt);
    event_add(&wt->master_info->hb_timer_evt, &tm);

    if (wt->master_info->hbsock <= 0) {
        wt->master_info->hbsock = socket(AF_INET, SOCK_DGRAM, 0);
    }
     
    change_sock_event(wt->base, wt->master_info->hbsock, EV_READ, g_cf->heartbeat_timeout, 
                1,  &wt->master_info->hbevt, backup_hb_read, NULL);
    return;
}
