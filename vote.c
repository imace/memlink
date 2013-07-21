#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <event.h>

#include "vote.h"
#include "serial.h"
#include "conn.h"
#include "base/logfile.h"
#include "base/zzmalloc.h"
#include "base/pack.h"
#include "runtime.h"
#include "wthread.h"
#include "common.h"
#include "serial.h"
#include "backup.h"
#include "master.h"
#include "heartbeat.h"

//for test
//#define VOTE_HOST "127.0.0.1"
//#define VOTE_PORT 11000
#define ERRBUFLEN 1024

int
set_linger(int fd)
{
    struct linger ling = {1, 0};
    setsockopt(AF_INET, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
    return 0;
}

int
set_nonblock(int fd)
{
    int flag;
    char err[ERRBUFLEN];

    flag = fcntl(fd, F_GETFL, 0);
    if (flag == -1) {
        strerror_r(errno, err, ERRBUFLEN);
        DERROR("set O_NONBLOCK: fcntl F_GETFL error: %s\n", err);
        return -1;
    }
    if (fcntl(fd, F_SETFL, flag | O_NONBLOCK) == -1) {
        strerror_r(errno, err, ERRBUFLEN);
        DERROR("set O_NONBLOCK: fcntl F_GETFL error: %s\n", err);
        return -1;
    }
    return 0;
}

int
vote_timeout(Conn *conn)
{
    DINFO("*** VOTE TIMEOUT to %s(%d)\n", conn->client_ip, conn->port);
    //*((int *)conn->thread) = VOTE_TIMEOUT;
    conn->vote_status = VOTE_TIMEOUT;
    conn->destroy(conn);
    return 0;
}

void
vote_destroy(Conn *conn)
{
    VoteConn *vconn = (VoteConn *)conn;

    if (conn->rbuf)
        zz_free(conn->rbuf);
    event_del(&conn->evt);
    set_linger(conn->sock);
    close(conn->sock);



    if (vconn->vote_status == VOTE_TIMEOUT) {
        DERROR("*** VOTE TIMEOUT, TRY AGAIN\n");
        vconn->vote_status = VOTE_CONTINUE;
    } else if (vconn->vote_status == VOTE_REQUEST || vconn->vote_status == VOTE_WAITING) {
/*
        if (g_cf->role == ROLE_NONE) {
            DERROR("*** VOTE REQUEST TRY AGAIN\n");
            vconn->status = VOTE_CONTINUE;  
        } else {
*/
        DERROR("*** VOTE ABNORMAL ERROR/HOST INVALID\n");
        vconn->vote_status = VOTE_CONTINUE;
/*
            g_cf->role = ROLE_NONE;
            g_runtime->voteid = 0;
            zz_free(vconn);
            return;
        }
*/
    } else if (vconn->vote_status == VOTE_ERR_PARAM){
        DERROR("*** VOTE PARAM ERROR\n");
        g_cf->role = ROLE_NONE;
        g_runtime->voteid = 0;
        zz_free(vconn);
        return;
    }

    if (vconn->vote_status == VOTE_CONTINUE) {
        DINFO("*** VOTE CONTINUE\n");
        request_vote(vconn->id, vconn->idflag, vconn->voteid, vconn->wport);
        zz_free(vconn);
        return;
    }
    DINFO("*** VOTE OK\n");

    zz_free(vconn);
}

int
pack_reply(char *buf, uint16_t retcode)
{
    return pack(buf, 0, "$4h", retcode);
}

void
release_hosts(Host *host)
{
    Host *tmp;
    while (host) {
        tmp = host->next;
        zz_free(host);
        host = tmp;
    }
}

int
vote_ready(Conn *conn, char *buf, int datalen)
{
    short retcode;
    uint8_t cmd;
    int count = sizeof(int);
    char ip[INET_ADDRSTRLEN];
    uint16_t port;
    uint64_t voteid;

    memcpy(&cmd, buf+count, sizeof(cmd));
    count += sizeof(cmd);
    if (g_cf->sync_mode == MODE_MASTER_SLAVE) {
        conn_send_buffer_reply(conn, MEMLINK_ERR_SLAVE, NULL, 0);
        return 0;
    }
    switch (cmd) {
        case CMD_VOTE:
            memcpy(&retcode, buf+count, sizeof(retcode));
            count += sizeof(retcode);
            switch (retcode) {
                case CMD_VOTE_WAIT:
                    DINFO("*** VOTE WAITING\n");

                    int timeout = 10*g_cf->timeout;
                    if (timeout < 60) {
                        timeout = 60;
                    } else if (timeout > 300){
                        timeout = 300;
                    }
                    //*((int *)conn->thread) = VOTE_WAITING;
                    conn->vote_status = VOTE_WAITING;
                    event_del(&conn->evt);
                    change_event(conn, EV_READ|EV_PERSIST, timeout, 1);

                    break;
                case CMD_VOTE_MASTER:
                    DINFO("*** VOTE MASTER\n");

/*
                    if (conn->thread != g_runtime->wthread)
                        *((int *)conn->thread) = VOTE_OK;
*/
                    conn->vote_status = VOTE_OK;
                    count += unpack_voteid(buf+count, &voteid);

                    if (g_cf->role == ROLE_NONE) {
                        DINFO("*** master init\n");
                        master_init(voteid, buf+count, datalen-count);
                        g_cf->role = ROLE_MASTER;
                        g_runtime->voteid = voteid;
                    } else {
                        DINFO("*** switch to master\n");
                        if (switch_master(voteid, buf+count, datalen-count) < 0) {
                            DERROR("*** FATAL ERROR : switch to master error\n");
                            g_cf->role = ROLE_NONE;
                            g_runtime->voteid = 0;
                        } else {
                            g_cf->role = ROLE_MASTER;
                            g_runtime->voteid = voteid;
                        }
                    }

                    DINFO("*** voteid: %llu\n", (unsigned long long)g_runtime->voteid);
                    DINFO("*** VOTE: I am master\n");
                    while (count < datalen) {
                        count += unpack_votehost(buf+count, ip, &port);
                        DINFO("*** VOTE backup: %s(%d)\n", ip, port);
                    }
/*
                    Host **hostp = &g_runtime->hosts;
                    while (count < datalen) {
                        count += unpack_votehost(buf+count, ip, &port);
                        DINFO("*** VOTE backup: %s(%d)\n", ip, port);
                        if (*hostp == NULL) {
                            *hostp = (Host *)zz_malloc(sizeof(Host));
                            strcpy((*hostp)->ip, ip);
                            (*hostp)->port = port;
                            (*hostp)->next = NULL;
                            hostp = &(*hostp)->next;
                        } else {
                            strcpy((*hostp)->ip, ip);
                            (*hostp)->port = port;
                            hostp = &(*hostp)->next;
                        }
                    }

                    if (*hostp != NULL) {
                        release_hosts(*hostp);
                        *hostp = NULL;
                    }
*/
                    conn_send_buffer_reply(conn, MEMLINK_OK, NULL, 0);

                    break;
                case CMD_VOTE_BACKUP:
                    DINFO("*** VOTE BACKUP\n");
/*
                    if (conn->thread != g_runtime->wthread)
                        *((int *)conn->thread) = VOTE_OK;
*/
                    conn->vote_status = VOTE_OK;

                    count += unpack_voteid(buf+count, &voteid);
                    unpack_votehost(buf+count, ip, &port);


                    if (g_cf->role == ROLE_NONE) {
                        DINFO("*** backup init\n");
                        backup_init(voteid, buf+count, datalen-count);
                        g_cf->role = ROLE_BACKUP;
                        g_runtime->voteid = voteid;
                    } else {
                        DINFO("*** switch to backup\n");
                        if (switch_backup(voteid, buf+count, datalen-count) < 0) {
                            DERROR("*** FATAL ERROR : switch to backup error\n");
                            g_cf->role = ROLE_NONE;
                            g_runtime->voteid = 0;
                        } else {
                            g_cf->role = ROLE_BACKUP;
                            g_runtime->voteid = voteid;
                        }
                    }
                    DINFO("*** VOTE: I am backup, master is %s(%d)\n", ip, port);
                    DINFO("*** voteid: %llu\n", (unsigned long long)g_runtime->voteid);

/*
                    Host *host;
                    if (g_runtime->hosts == NULL) {
                        host = (Host *)zz_malloc(sizeof(Host));
                        if (host == NULL) {
                            DERROR("*** VOTE: zz_malloc error\n");
                            MEMLINK_EXIT;
                        }
                        host->next = NULL;
                        strcpy(host->ip, ip);
                        host->port = port;
                        g_runtime->hosts = host;
                    } else {
                        host = g_runtime->hosts;
                        strcpy(host->ip, ip);
                        host->port = port;
                        release_hosts(host->next);
                        host->next = NULL;
                    }
*/
                    conn_send_buffer_reply(conn, MEMLINK_OK, NULL, 0);

                    break;
                case CMD_VOTE_NONEED:
                    DINFO("*** VOTE NONEED\n");

                    if (g_cf->role == ROLE_NONE) {
                        //*((int *)conn->thread) = VOTE_CONTINUE;
                        conn->vote_status = VOTE_CONTINUE;
                        break;
                    }

                    //*((int *)conn->thread) = VOTE_OK;
                    conn->vote_status = VOTE_OK;
                    backup_create_heartbeat(g_runtime->wthread);

                    break;
                case MEMLINK_ERR_VOTE_PARAM:
                    DERROR("*** VOTE PARAM ERROR\n");

                    //*((int *)conn->thread) = VOTE_ERR_PARAM;
                    conn->vote_status = VOTE_ERR_PARAM;

                    break;
                default:
                    DERROR("*** VOTE UNKNOWN ERROR\n");
                    //if (conn->thread != g_runtime->wthread)
                    //    *((int *)conn->thread) = VOTE_OK;
                    conn->vote_status = VOTE_OK;

                    break;
            }
            break;
        case CMD_VOTE_UPDATE:
            DINFO("*** VOTE UPDATING\n");
            if (g_cf->role != ROLE_MASTER) {
                conn_send_buffer_reply(conn, MEMLINK_ERR_NOT_MASTER, NULL, 0);
                break;
            }
            //DINFO("*** VOTE MASTER\n");
/*
            if (conn->thread != g_runtime->wthread)
                *((int *)conn->thread) = VOTE_OK;
*/
            //conn->vote_status = VOTE_OK;

            count += unpack_voteid(buf+count, &voteid);
            DINFO("*** voteid: %llu\n", (unsigned long long)voteid);
            DINFO("*** VOTE: I am master\n");

            g_cf->role = ROLE_MASTER;
            g_runtime->voteid = voteid;

            mb_binfo_update(voteid, buf+count, datalen-count);

            while (count < datalen) {
                count += unpack_votehost(buf+count, ip, &port);
                DINFO("*** VOTE backup: %s(%d)\n", ip, port);
            }
/*
            Host **hostp = &g_runtime->hosts;
            while (count < datalen) {
                count += unpack_votehost(buf+count, ip, &port);
                DINFO("*** backup: %s(%d)\n", ip, port);
                if (*hostp == NULL) {
                    *hostp = (Host *)zz_malloc(sizeof(Host));
                    strcpy((*hostp)->ip, ip);
                    (*hostp)->port = port;
                    (*hostp)->next = NULL;
                    hostp = &(*hostp)->next;
                } else {
                    strcpy((*hostp)->ip, ip);
                    (*hostp)->port = port;
                    hostp = &(*hostp)->next;
                }
            }

            if (*hostp != NULL) {
                release_hosts(*hostp);
                *hostp = NULL;
            }
*/
            conn_send_buffer_reply(conn, MEMLINK_OK, NULL, 0);

            break;
        case CMD_VOTE_DETECT:
            DINFO("*** VOTE DETECT\n");
            if (g_cf->role == ROLE_MASTER) {
                retcode = CMD_VOTE_MASTER;
            } else if (g_cf->role == ROLE_BACKUP){
                retcode = CMD_VOTE_BACKUP;
            } else {
                retcode = MEMLINK_ERR_NO_ROLE;
            }
            conn_send_buffer_reply(conn, retcode, (char *)&g_runtime->voteid, sizeof(g_runtime->voteid));

            break;
        default:
            break;
    }
    return 0;
}

int
request_vote(uint64_t id, unsigned char idflag, uint64_t voteid, unsigned short port)
{
    struct sockaddr_in s;
    int fd;
    int ret;
    char err[ERRBUFLEN];

    DINFO("*** VOTE REQUEST: id: %llu, idflag: %d, voteid: %llu, port: %d\n",
          (unsigned long long)id, idflag, (unsigned long long)voteid, port);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        strerror_r(errno, err, ERRBUFLEN);
        DERROR("*** VOTE: create socket error: %s\n", err);
        return -1;
    }

    if (set_nonblock(fd) == -1) {
        close(fd);
        return -1;
    }

    VoteConn *conn = (VoteConn *)zz_malloc(sizeof(VoteConn));
    if (conn == NULL) {
        DERROR("*** VOTE: conn malloc error.\n");
        MEMLINK_EXIT;
    }

    memset(conn, 0, sizeof(VoteConn));
    conn->rbuf = (char *)zz_malloc(CONN_MAX_READ_LEN);
    if (conn->rbuf == NULL) {
        DERROR("*** VOTE: rbuf malloc error.\n");
        MEMLINK_EXIT;
    }
/*
    conn->wbuf = (char *)zz_malloc(CONN_MAX_READ_LEN);
    if (conn->wbuf == NULL) {
        DERROR("*** VOTE: wbuf malloc error.\n");
        MEMLINK_EXIT;
    }
*/
    conn->wbuf = conn->rbuf;
    conn->rsize = CONN_MAX_READ_LEN;
    conn->wsize = CONN_MAX_READ_LEN;
    conn->sock = fd;
    conn->destroy = vote_destroy;
    conn->wrote = conn_wrote;
    conn->ready = vote_ready;
    conn->timeout = vote_timeout;
    conn->read = conn_event_read;
    conn->write = conn_event_write;
    conn->base = g_runtime->wthread->base;

    conn->vote_status = VOTE_REQUEST;
    //conn->thread = &conn->status;

    strcpy(conn->client_ip, g_cf->vote_host);
    conn->client_port = g_cf->vote_port;

    conn->id = id;
    conn->idflag = idflag;
    conn->voteid = voteid;
    conn->wport = port;
    conn->wlen = cmd_vote_pack(conn->wbuf, id, idflag, voteid, port);

    bzero(&s, sizeof(s));
    s.sin_family = AF_INET;

    //strcpy(g_cf->vote_host, VOTE_HOST);
    //g_cf->vote_port = VOTE_PORT;

    inet_pton(AF_INET, g_cf->vote_host, &s.sin_addr);
    s.sin_port = htons(g_cf->vote_port);

    int flag = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    ret = connect(fd, (struct sockaddr *)&s, sizeof(s));
    if (ret == -1) {
        if (errno != EINPROGRESS && errno != EINTR) {
            strerror_r(errno, err, ERRBUFLEN);
            DERROR("*** VOTE: connect error: %s\n", err);
            conn->destroy((Conn *)conn);
            return -1;
        }
    }

    if (change_event((Conn *)conn, EV_WRITE|EV_PERSIST, g_cf->timeout, 1) < 0) {
        DERROR("*** VOTE: change_event error\n");
        conn->destroy((Conn *)conn);
        return -1;
    }

    return 0;
}
