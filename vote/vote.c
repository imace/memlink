/**
 * 仲裁模块
 * @file vote.c
 * @author zwyao
 * @ingroup vote
 * @{
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <arpa/inet.h>
#include <event.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <libgen.h>
#include <sys/resource.h>
#include <ctype.h>

#include "vote.h"
#include "../common.h"
#include "daemon.h"
#include "../base/zzmalloc.h"
#include "logfile.h"
#include "confparse.h"
#include "conn.h"
#include "network.h"

MainServer *ms = NULL;
//VoteReplyThread *vs = NULL;
Config *g_cf;
extern LogFile *g_log;

int pack_master(char *buf);
int pack_backup(char *buf);
void vconn_event_read(int fd, short event, void *arg);
void vconn_event_write(int fd, short event, void *arg);
int rvote_ready(Conn *conn, char *data, int len);
int vote_confirm(Conn *conn, char *data, int len);
void update_hosts(int notifyfd, short event, void *arg);

/*
*
*sizeof(int) + cmd(1 bytes)
*/
int
pack_cmd(char *buf, uint8_t cmd)
{
    int count = sizeof(int);
    memcpy(buf+count, &cmd, sizeof(cmd));
    count += sizeof(cmd);
    return count;
}

/*
*
*/
int
cmd_vote_unpack(char *data, Vote *vote)
{
    int count = 0;

    memcpy(&vote->id, data, sizeof(uint64_t));
    count += sizeof(uint64_t);
    memcpy(&vote->id_flag, data+count, sizeof(uint8_t));
    count += sizeof(uint8_t);
    memcpy(&vote->vote_id, data+count, sizeof(uint64_t));
    count += sizeof(uint64_t);
    memcpy(&vote->port, data+count, sizeof(uint16_t));

    //DINFO("*** id: %llu, idflag: %d, vote_id: %llu, port: %u\n", vote->id, vote->id_flag, vote->vote_id, vote->port);
    if (vote->id_flag != COMMITED && vote->id_flag != ROLLBACKED) {
        DERROR("*** id flag error: %d\n", vote->id_flag);
        return -1;
    }

    return 0;
}
/*
int 
change_event(Conn *conn, int newflag, int timeout, int isnew)
{
    struct event *event = &conn->evt;

    if (isnew == 0) {
        if (event->ev_flags == newflag)
            return 0;
        if (event_del(event) == -1) {
            DERROR("event del error.\n");
            return -1;
        }
    }

    if (newflag & EV_READ) {
        event_set(event, conn->sock, newflag, client_read, (void *)conn);
    }else if (newflag & EV_WRITE) {
        event_set(event, conn->sock, newflag, client_write, (void *)conn);
    }
    event_base_set(conn->base, event);
  
    if (timeout > 0) {
        struct timeval  tm;
        evutil_timerclear(&tm);
        tm.tv_sec = timeout;
        if (event_add(event, &tm) == -1) {
            DERROR("event add error.\n");
            return -3;
        }
    }else{
        if (event_add(event, 0) == -1) {
            DERROR("event add error.\n");
            return -3;
        }
    }
    return 0;
}
*/

void
release_vote(Vote *vote)
{
    Vote **tmp = &ms->votes;
    while (*tmp) {
        if (*tmp == vote) {
            *tmp = vote->next;
            ms->vote_num--;
            zz_free(vote);
            break;
        }

        tmp = &(*tmp)->next;
    }
}

int
conn_dummy(Conn *conn, char *data, int datalen)
{
    DERROR("*** conn_dummy call, ignore the data\n");
    return 0;
}


int
vconn_wrote(Conn *conn)
{
    int ret = change_event(conn, EV_READ|EV_PERSIST, conn->iotimeout, 0);
    if (ret < 0) {
        DERROR("*** change event error: %d close socket\n", ret);
        conn->destroy(conn);
    }
    conn->rlen = 0;
    return 0;
}

void
vconn_destroy(Conn *conn)
{
    VoteConn *vconn = (VoteConn *)conn;

    if (vconn->vote)
        vconn->vote->conn = NULL;
//    DINFO("ok\n");

    DINFO("*** CLOSE CONN %s(%d)\n", conn->client_ip, conn->port);
    conn_destroy(conn);
//    DINFO("ok\n");
/*
    if (conn->wbuf)
        zz_free(conn->wbuf);
    if (conn->rbuf)
        zz_free(conn->rbuf);
    event_del(&conn->evt);
    close(conn->sock);
    zz_free(conn);
*/
}

void
vconn_event_read(int fd, short event, void *arg)
{
    VoteConn *conn = (VoteConn *)arg;
    int ret;
    unsigned int datalen = 0;

    if (event & EV_TIMEOUT) {
        if (conn->status == VOTE_SENDED) {
            DERROR("*** timeout: %s(%d) receive \"vote\" confirm\n", conn->client_ip, conn->port);
            if (conn->trytimes > 0 && change_event((Conn *)conn, EV_WRITE|EV_PERSIST, conn->iotimeout, 0) == 0) {
                DERROR("*** timeout: %s(%d) try (%d) to send \"votes\" again...\n",
                       conn->client_ip, conn->port, g_cf->trytimes-conn->trytimes+1);
                conn->status = VOTE_SENDING;
                conn->wpos = 0;
                conn->trytimes--;
                return;
            } else if (conn->trytimes == 0) {
                DERROR("*** timeout: %s(%d) after try (%d) times\n", conn->client_ip, conn->port, g_cf->trytimes);
            }
        } else if (conn->status == VOTE_UPDATE_SENDED) {
            DERROR("*** timeout: %s(%d) receive \"update-hosts\" confirm\n", conn->client_ip, conn->port);
            if (conn->trytimes > 0 && change_event((Conn *)conn, EV_WRITE|EV_PERSIST, conn->iotimeout, 0) == 0) {
                DERROR("*** timeout: %s(%d) try (%d) to send \"update-hosts\" again...\n",
                       conn->client_ip, conn->port, g_cf->trytimes-conn->trytimes+1);
                conn->status = VOTE_UPDATE_SENDING;
                conn->wpos = 0;
                conn->trytimes--;
                return;
            } else if (conn->trytimes == 0) {
                DERROR("*** timeout: after try (%d) times\n", g_cf->trytimes);
            }
        } else if (conn->status == VOTE_DETECT_SENDED) {
            DERROR("*** timeout: %s(%d) received \"detect-hosts\" reply\n", conn->client_ip, conn->port);
/*
            int *active_host = (int *)conn->arg;
            *active_host -= 1;
            if (*active_host == 0)
                event_base_loopbreak(conn->base);
*/
        } else {
            DERROR("read %s timeout, close\n", conn->client_ip);
        }
        conn->destroy((Conn *)conn);
        return;
    }
    if (conn->rlen >= sizeof(int)) {
        memcpy(&datalen, conn->rbuf, sizeof(int));
        datalen -= conn->rlen - sizeof(int);
    } else {
        datalen = conn->rsize - conn->rlen;
    }

    //DINFO("bufsize: %d readed: %d, left: %d\n", conn->rsize, conn->rlen, datalen);
    while (1) {
        ret = read(fd, conn->rbuf + conn->rlen, datalen);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            } else if (errno != EAGAIN) {
                if (conn->status == VOTE_SENDED) {
                    DERROR("*** read error: receive %s(%d)'s \"vote\" confirm package: %s\n",
                            conn->client_ip, conn->port, strerror(errno));
                } else if (conn->status == VOTE_UPDATE_SENDED) {
                    DERROR("*** read error: receive %s(%d)'s \"update-hosts\" confirm package: %s\n",
                            conn->client_ip, conn->port, strerror(errno));
                } else if (conn->status == VOTE_DETECT_SENDED) {
                    DERROR("*** read error: %s(%d) when \"detect-hosts\": %s\n", conn->client_ip, conn->port, strerror(errno));
/*
                    int *active_host = (int *)conn->arg;
                    *active_host -= 1;
                    if (*active_host == 0)
                        event_base_loopbreak(conn->base);
*/
                } else if (conn->status == VOTE_WAITING){
                    DERROR("*** read error: %s(%d) error, close(\"vote-wait\"): %s\n",
                            conn->client_ip, conn->port, strerror(errno));
                } else {
                    DERROR("*** read error: %s(%d), close conn: %s\n", conn->client_ip, conn->port, strerror(errno));
                }
                conn->destroy((Conn *)conn);
                return;
            }else{
                DERROR("*** %d read EAGAIN, error %d: %s\n", fd, errno, strerror(errno));
            }

        } else if (ret == 0) {
            if (conn->status == VOTE_SENDED) {
                DERROR("*** %s(%d) abnormal closed, do not receive \"vote\" confirm package\n",
                        conn->client_ip, conn->port);
            } else if (conn->status == VOTE_UPDATE_SENDED) {
                DERROR("*** %s(%d) abnormal closed, do not receive \"update-hosts\" confirm package\n",
                        conn->client_ip, conn->port);
            } else if (conn->status == VOTE_DETECT_SENDED) {
                DERROR("*** %s(%d) abnormal closed, when \"detect-hosts\"\n", conn->client_ip, conn->port);
/*
                int *active_host = (int *)conn->arg;
                *active_host -= 1;
                if (*active_host == 0)
                    event_base_loopbreak(conn->base);
*/
            } else if (conn->status == VOTE_WAITING){
                DERROR("*** %s(%d) abnormal closed, close(\"vote-wait\")\n", conn->client_ip, conn->port);
            } else {
                DERROR("*** read 0, close conn %s(%d).\n", conn->client_ip, conn->port);
            }
            conn->destroy((Conn *)conn);
            return;
        }else{
            conn->rlen += ret;
        }
        break;
    }

    if (conn->rlen >= sizeof(int)) {
        memcpy(&datalen, conn->rbuf, sizeof(int));
        unsigned int mlen = datalen + sizeof(int);

        if (conn->rlen >= mlen) {
            conn->ready((Conn *)conn, conn->rbuf, mlen);
        }
    }
}

void
vconn_event_write(int fd, short event, void *arg)
{
    VoteConn *conn = (VoteConn *)arg;
    if (event & EV_TIMEOUT) {
        if (conn->status == VOTE_SENDING) {
            DERROR("*** timeout: send \"vote\" to %s(%d)\n", conn->client_ip, conn->port);
        } else if (conn->status == VOTE_WAITING) {
            DERROR("*** timeout: send \"vote-wait\" to %s(%d)\n", conn->client_ip, conn->port);
        } else if (conn->status == VOTE_NONEED) {
            DERROR("*** timeout: send \"vote-noneed\" to %s(%d)\n", conn->client_ip, conn->port);
        } else if (conn->status == VOTE_CONNECTING) {
            DERROR("*** timeout: %s(%d) connecting to send \"vote\"\n", conn->client_ip, conn->port);
        } else if (conn->status == VOTE_PARAM_ERR) {
            DERROR("*** timeout: %s(%d) send \"vote-param-err\"\n", conn->client_ip, conn->port);
        } else if (conn->status == VOTE_UPDATE_CONNECTING) {
            DERROR("*** timeout: %s(%d) connecting to send \"update-hosts\"\n", conn->client_ip, conn->port);
        } else if (conn->status == VOTE_DETECT_CONNECTING) {
            DERROR("*** timeout: %s(%d) connecting to send \"detect-hosts\"\n", conn->client_ip, conn->port);
/*
            int *active_host = (int *)conn->arg;
            *active_host -= 1;
            if (*active_host == 0)
                event_base_loopbreak(conn->base);
*/
        }
        conn->destroy((Conn *)conn);
        return;
    }

    if (conn->status == VOTE_CONNECTING) {
        conn->status = VOTE_SENDING;
    } else if (conn->status == VOTE_UPDATE_CONNECTING) {
        conn->status = VOTE_UPDATE_SENDING;
    } else if (conn->status == VOTE_DETECT_CONNECTING) {
        conn->status = VOTE_DETECT_SENDING;
    }

    int ret;

    while (1) {
        ret = write(conn->sock, conn->wbuf + conn->wpos, conn->wlen - conn->wpos);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }else if (errno != EAGAIN) {
                if (conn->status == VOTE_SENDING) {
                    DERROR("*** send \"vote\" to %s(%d) error: %s\n", conn->client_ip, conn->port, strerror(errno));
                } else if (conn->status == VOTE_UPDATE_SENDING) {
                    DERROR("*** send \"update-hosts\" to %s(%d) error: %s\n", conn->client_ip, conn->port, strerror(errno));
                } else if (conn->status == VOTE_DETECT_SENDING) {
                    DERROR("*** send \"detect-hosts\" to %s(%d) error: %s\n", conn->client_ip, conn->port, strerror(errno));
/*
                    int *active_host = (int *)conn->arg;
                    *active_host -= 1;
                    if (*active_host == 0)
                        event_base_loopbreak(conn->base);
*/
                } else if (conn->status == VOTE_WAITING) {
                    DERROR("*** send \"vote-wait\" to %s(%d) error: %s\n", conn->client_ip, conn->port, strerror(errno));
                } else if (conn->status == VOTE_NONEED) {
                    DERROR("*** send \"vote-noneed\" to %s(%d) error: %s\n", conn->client_ip, conn->port, strerror(errno));
                } else if (conn->status == VOTE_PARAM_ERR) {
                    DERROR("*** send \"vote-param_err\" to %s(%d) error: %s\n", conn->client_ip, conn->port, strerror(errno));
                }
                conn->destroy((Conn *)conn);
                return;
            }
        }else{
            conn->wpos += ret;
        }
        break;
    } 

    if (conn->wpos == conn->wlen) {
        if (conn->status == VOTE_WAITING) {
            DINFO("*** \"vote-waiting\" on the way to %s(%d)\n", conn->client_ip, conn->port);
/*
            conn->destroy(conn);
            return;
*/
            change_event((Conn *)conn, EV_READ|EV_PERSIST, -1, 0);
            return;
        } else if (conn->status == VOTE_SENDING) {
            DINFO("*** \"vote\" on the way to %s(%d), wait confirm.\n", conn->client_ip, conn->port);
            conn->status = VOTE_SENDED;
        } else if (conn->status == VOTE_UPDATE_SENDING) {
            DINFO("*** \"update-hosts\" on the way to %s(%d), wait confirm.\n", conn->client_ip, conn->port);
            conn->status = VOTE_UPDATE_SENDED;
        } else if (conn->status == VOTE_DETECT_SENDING) {
            DINFO("*** \"detect-hosts\" on the way to %s(%d), wait reply.\n", conn->client_ip, conn->port);
            conn->status = VOTE_DETECT_SENDED;
        } else if (conn->status == VOTE_NONEED) {
            DINFO("*** \"vote-noneed\" on the way to %s(%d)\n", conn->client_ip, conn->port);
            conn->destroy((Conn *)conn);
            return;
        } else if (conn->status == VOTE_PARAM_ERR) {
            DINFO("*** \"vote-param-err\" on the way to %s(%d)\n", conn->client_ip, conn->port);
            conn->destroy((Conn *)conn);
            return;
        }
        //conn->wlen = conn->wpos = 0;
        conn->wrote((Conn *)conn);
    }
}

int
pack_string(char *s, char *v, unsigned char vlen)
{
    if (vlen <= 0) {
        vlen = strlen(v);
    }
    if (vlen > 0) {
        memcpy(s, v, vlen);
    }
    s[vlen] = '\0';

    return vlen + 1;
}

int check_ip_valid(uint32_t ip)
{
    //return 1;
    Host *h = g_cf->hosts;

    while (h) {
        if (h->ip == ip) {
            return 1;
        }
        h = h->next;
    }

    return 0;
}

int
check_conn_valid(uint32_t ip, uint16_t port)
{
    //return 1;
    Host *h = g_cf->hosts;

    while (h) {
        if (h->ip == ip && h->port == port) {
            return 1;
        }
        h = h->next;
    }

    return 0;
}

int
set_linger(int fd)
{
    struct linger ling = {1, 0};
    return setsockopt(fd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
}

int
set_nonblock(int fd)
{
    int flag;

    flag = fcntl(fd, F_GETFL, 0);
    if (flag == -1) {
        DERROR("fcntl F_GETFL error: %s\n", strerror(errno));
        return -1;
    }
    if (fcntl(fd, F_SETFL, flag | O_NONBLOCK) == -1) {
        DERROR("fcntl F_SETFL error: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int
tcp_server(uint16_t port)
{
    int fd, ret, flag;
    struct sockaddr_in s;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        DERROR("socket: %s\n", strerror(errno));
        return -1;
    }
    bzero(&s, sizeof(s));
    s.sin_family = AF_INET;
    s.sin_port = htons(port);
    s.sin_addr.s_addr = htonl(INADDR_ANY);

    flag = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    ret = bind(fd, (struct sockaddr *)&s, sizeof(s));
    if (ret < 0) {
        DERROR("bind: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    ret = listen(fd, 128);
    if (ret < 0) {
        DERROR("listen: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

void
set_roles(uint32_t master_host, uint16_t master_port)
{
    Host *host = g_cf->hosts;

    while (host) {
        if (host->ip == master_host && host->port == master_port) {
            host->role = ROLE_MASTER;
        } else {
            host->role = ROLE_BACKUP;
        }
        host = host->next;
    }
}

void
set_role(uint32_t ip, uint16_t port, uint8_t role)
{
    Host *host = g_cf->hosts;
    while (host) {
        if (host->ip == ip && host->port == port) {
            host->role = role;
            break;
        }
        host = host->next;
    }
    return;
}

VoteConn *
get_conn()
{
    VoteConn *conn = (VoteConn *)zz_malloc(sizeof(VoteConn));
    if (conn == NULL) {
        DERROR("zz_malloc error\n");
        MEMLINK_EXIT;
    }

    memset(conn, 0, sizeof(VoteConn));
    conn->wbuf = (char *)zz_malloc(1024);
    conn->rbuf = (char *)zz_malloc(1024);
    if (conn->wbuf == NULL || conn->rbuf == NULL) {
        DERROR("zz_malloc error\n");
        MEMLINK_EXIT;
    }
    conn->wsize = 1024;
    conn->rsize = 1024;
    conn->read = vconn_event_read;
    conn->write = vconn_event_write;
    conn->destroy = vconn_destroy;
    conn->wrote = vconn_wrote;
    conn->status = VOTE_NONE;
    conn->trytimes = g_cf->trytimes;
    conn->sock = -1;
    return conn;
}

VoteConn *
find_conn(uint32_t ip, uint16_t port)
{
    Vote *vote = ms->votes;
    while (vote) {
        if (vote->host == ip && vote->port == port)
            return vote->conn;
        vote = vote->next;
    }
    return NULL;
}

void
reply_votes()
{
    int fd, ret;
    char mbuf[1024];
    char bbuf[1024];
    struct sockaddr_in s;
    int newconn;

    int mlen = pack_master(mbuf);
    //DINFO("mlen: %d\n", mlen);
    if (mlen == -1)
        return;
    int blen = pack_backup(bbuf);
    //DINFO("blen: %d\n", blen);

    Host *host = g_cf->hosts;
    while (host) {
        newconn = 0;
        VoteConn *conn = find_conn(host->ip, host->port);
        if (conn == NULL) {
            //DINFO("*** get new conn\n");
            newconn = 1;
            conn = get_conn();
            fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd == -1) {
                DERROR("*** <reply votes> socket error: %s\n", strerror(errno));
                host = host->next;
                conn->destroy((Conn *)conn);
                continue;
            }
            if (set_nonblock(fd) < 0) {
                DERROR("*** <reply votes> set nonblock error: %s\n", strerror(errno));
                close(fd);
                host = host->next;
                conn->destroy((Conn *)conn);
                continue;
            }

            conn->sock = fd;
            conn->base = ms->base;

            bzero(&s, sizeof(s));
            s.sin_family = AF_INET;
            s.sin_addr.s_addr = host->ip;
            s.sin_port = htons(host->port);

            conn->port = host->port;
            inet_ntop(AF_INET, &host->ip, conn->client_ip, INET_ADDRSTRLEN);

            conn->status = VOTE_CONNECTING;
            ret = connect(conn->sock, (struct sockaddr *)&s, sizeof(s));
            if (ret == -1)
                if (errno != EINPROGRESS && errno != EINTR) {
                    char ip[INET_ADDRSTRLEN];
                    DERROR("*** <reply votes> connect to <%s(%d)> error: %s\n", 
                            inet_ntop(AF_INET, &host->ip, ip, INET_ADDRSTRLEN), host->port, strerror(errno));
                    conn->destroy((Conn *)conn);
                    host = host ->next;
                    continue;
                }

        } else {
            conn->port = host->port;
            inet_ntop(AF_INET, &host->ip, conn->client_ip, INET_ADDRSTRLEN);
            conn->status = VOTE_SENDING;
            DINFO("*** %s(%d) reuse conn(vote)\n", conn->client_ip, conn->port);
        }
        
        if (host->role == ROLE_BACKUP) {
            memcpy(conn->wbuf, mbuf, mlen);
            conn->wlen = mlen;
            conn->wpos = 0;
        } else if (host->role == ROLE_MASTER){
            memcpy(conn->wbuf, bbuf, blen);
            conn->wlen = blen;
            conn->wpos = 0;
        }
        conn->ready = vote_confirm;

        if (change_event((Conn *)conn, EV_WRITE|EV_PERSIST, g_cf->timeout, newconn) < 0) {
            DERROR("*** <reply votes> <%s(%d)> change event error\n", conn->client_ip, conn->port);
            conn->destroy((Conn *)conn);
            host = host ->next;
            continue;
        }

        host = host->next;
    }
}

void
do_vote()
{
    Vote *vote = ms->votes;
    Vote *v = vote->next;

    DINFO("*** start voting...\n");
    while (v) {
        if (vote->id < v->id) {
            vote = v;
        } else if (vote->id == v->id) {
            if (vote->id_flag == ROLLBACKED && v->id_flag == COMMITED)
                vote = v;
        }
        v = v->next;
    }

    set_roles(vote->host, vote->port);
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &vote->host, ip, INET_ADDRSTRLEN);
    DINFO("*** %s(%d) IS THE NEW MASTER\n", ip, vote->port);

    ms->vote_id++;

    //write(vs->notify_send_fd, "v", 1);
    reply_votes();

    return;
}

void
clear_votes()
{
    Vote *vote, *tmp;
    vote = ms->votes;
    while (vote) {
        tmp = vote;
        if (vote->conn)
            vote->conn->vote = NULL;
        vote = vote->next;
        zz_free(tmp);
    }
    ms->votes = NULL;
    ms->vote_num = 0;
    ms->vote_flag = 0;

    return;
}

int
pack_noneed(char *buf)
{
    int count = pack_cmd(buf, CMD_VOTE);
    short retcode = CMD_VOTE_NONEED;
    memcpy(buf+count, &retcode, sizeof(retcode));
    count += sizeof(retcode);
    int len = count - sizeof(int);
    memcpy(buf, &len, sizeof(len));

    return count;
}

void
reply_noneed()
{
    Vote *vote = ms->votes;
    char buf[1024];

    int count = pack_noneed(buf);
    while (vote) { 
        VoteConn *conn = vote->conn;
        if (conn != NULL) {
            conn->port = vote->port;
            inet_ntop(AF_INET, &vote->host, conn->client_ip, INET_ADDRSTRLEN);

            conn->status = VOTE_NONEED;
            DINFO("*** %s(%d) reuse conn(vote-noneed)\n", conn->client_ip, conn->port);

            memcpy(conn->wbuf, buf, count);
            conn->wlen = count;
            conn->wpos = 0;
            conn->ready = conn_dummy;

            if (change_event((Conn *)conn, EV_WRITE|EV_PERSIST, conn->iotimeout, 0) < 0) {
                DERROR("*** <send vote-noneed> <%s(%d)> change event error\n", conn->client_ip, conn->port);
                conn->destroy((Conn *)conn);
                vote = vote->next;
                continue;
            }
        }
        vote = vote->next;
    }
    return;
}

void
vote_time_window(int fd, short event, void *arg)
{
    if (ms->vote_num >= CANVOTE) {
        do_vote();
        clear_votes();
    } else {
        if (ms->timeout <= 0) {
            reply_noneed();
            clear_votes();
        } else {
            struct timeval tm;
            evutil_timerclear(&tm);
            tm.tv_sec = g_cf->time_window_interval;
            event_add(&ms->time_window, &tm);
            ms->timeout--;
        }
    }
    return;
}

int
reply_vote_direct(VoteConn *conn)
{
    conn->wlen = pack_master(conn->wbuf);
    if (conn->wlen == -1) {
        conn->destroy((Conn *)conn);
        return -1;
    }
    conn->wpos = 0;

    if (change_event((Conn *)conn, EV_WRITE|EV_PERSIST, conn->iotimeout, 0) < 0) {
        DERROR("change event error.\n");
        conn->destroy((Conn *)conn);
    }
    
    conn->status = VOTE_SENDING;
    conn->ready = vote_confirm;
    return 0;
}

int
reply_wait(VoteConn *conn, Vote *vote)
{
    int ret;
    int count;
    short retcode = CMD_VOTE_WAIT;

    count = pack_cmd(conn->wbuf, CMD_VOTE);
    memcpy(conn->wbuf+count, &retcode, sizeof(retcode));
    count += sizeof(retcode);
    ret = count - sizeof(int);
    memcpy(conn->wbuf, &ret, sizeof(ret));
    conn->wlen = count;
    conn->wpos = 0;
    conn->status = VOTE_WAITING;

    ret = change_event((Conn *)conn, EV_WRITE|EV_PERSIST, conn->iotimeout, 0);
    if (ret < 0) {
        DERROR("*** <reply wait> change event error.\n");
        conn->destroy((Conn *)conn);
        return -1;
    }
    vote->conn = conn;
    conn->vote = vote;
    conn->ready = conn_dummy;
    return 0;
}
/*
*sizeof(int) + CMD_VOTE(2 bytes) + MEMLINK_ERR_VOTE_PARAM(2 bytes)
*/
int
pack_err(char *buf)
{
    short retcode = MEMLINK_ERR_VOTE_PARAM;
    int count;

    count = pack_cmd(buf, CMD_VOTE);

    memcpy(buf+count, &retcode, sizeof(retcode));
    count += sizeof(retcode);

    int len = count - sizeof(int);
    memcpy(buf, &len, sizeof(len));

    return count;
}

int
reply_err(VoteConn *conn)
{
    conn->wlen = pack_err(conn->wbuf);
    conn->wpos = 0;
    if (change_event((Conn *)conn, EV_WRITE|EV_PERSIST, conn->iotimeout, 0) < 0) {
        DERROR("*** <reply err> change event error.\n");
        conn->destroy((Conn *)conn);
    }
    conn->status = VOTE_PARAM_ERR;
    return 0;
}

int
vote_exist(Vote *vote)
{
    Vote *v = ms->votes;
    while (v) {
        if (v->host == vote->host && v->port == vote->port)
            return 1;
        v = v->next;
    }
    return 0;
}

int
vote_confirm(Conn *conn, char *data, int len)
{
    VoteConn *vconn = (VoteConn *)conn;
    short retcode;

    memcpy(&retcode, conn->rbuf+sizeof(int), sizeof(retcode));

    if (retcode == MEMLINK_OK) {
        if (vconn->status == VOTE_SENDED) {
            DINFO("*** VOTE: %s(%d) has received \"votes\"\n", conn->client_ip, conn->port);
        } else if (vconn->status == VOTE_UPDATE_SENDED) {
            DINFO("*** UPDATE HOSTS: %s(%d) has received update-hosts info\n", conn->client_ip, conn->port);
        }
    } else if (retcode == MEMLINK_ERR_NOT_MASTER){
        if (vconn->status == VOTE_UPDATE_SENDED)
            DWARNING("*** UPDATE HOSTS: %s(%d) is not master, ignore \"update-hosts\"\n", conn->client_ip, conn->port);
    } else if (retcode == MEMLINK_ERR_SLAVE) {
        DWARNING("*** VOTE: %s(%d) is in sync_slave mode\n", conn->client_ip, conn->port);
    } else {
        DWARNING("*** %s(%d) unknown error\n", conn->client_ip, conn->port);
    }
    conn->destroy((Conn *)conn);

    return 0;
}

int
rvote_ready(Conn *conn, char *data, int len)
{
    uint8_t cmd;
    int ret;
    int datalen;
    uint32_t host;
    Vote *vote;
    VoteConn *vconn = (VoteConn *)conn;

    memcpy(&datalen, conn->rbuf, sizeof(int));
    memcpy(&cmd, conn->rbuf+sizeof(int), sizeof(char));

    switch (cmd) {
        case CMD_VOTE:
            //DINFO("<<<cmd VOTE>>>\n");

            vote = (Vote *)zz_malloc(sizeof(Vote));
            if (vote == NULL) {
                DERROR("malloc error.\n");
                MEMLINK_EXIT;
            }

            memset(vote, 0, sizeof(Vote));

            ret = cmd_vote_unpack(conn->rbuf+CMD_REQ_HEAD_LEN, vote);

            inet_pton(AF_INET, conn->client_ip, (struct in_addr *)&host);
            if (check_conn_valid(host, vote->port) == 0) {
                DERROR("*** %s(%d) invalid\n", conn->client_ip, vote->port);
                if (set_linger(conn->sock) < 0)
                    DERROR("*** set linger error: %s\n", strerror(errno));
                conn->destroy(conn);
                zz_free(vote);
                return -1;
            }

            DINFO("*** %s(%d) vote comes\n", conn->client_ip, vote->port);
            conn->port = vote->port;

            if (ret < 0) {
                DERROR("*** %s(%d) vote parameter error\n", conn->client_ip, vote->port);
                reply_err(vconn);
                zz_free(vote);
                break;
            }

            vote->host = host;
            vconn->status = VOTE_RECVED;

            if (vote_exist(vote) == 1) {
                //reply_wait(conn, vote);
                DINFO("*** %s(%d) vote already in, ignore this one\n", conn->client_ip, vote->port);
                conn->destroy(conn);
                zz_free(vote);
                break;
            }

            DINFO("*** VOTE voteid: %llu %s(%d)\n", (unsigned long long)vote->vote_id, conn->client_ip, vote->port);
            if (vote->vote_id <= ms->vote_id) {
            //需要判断
                if (ms->vote_flag == 1) {
                    //可能当前vote是重启后进来的，如果这时系统正在做仲裁，
                    //清零id，确保不会选择当前vote为新的主，但是会让它参与仲裁.
                    vote->id = 0;
                } else {
                    //直接返回已有仲裁结果。
                    reply_vote_direct(vconn);
                    zz_free(vote);
                    break;
                }
            }
            vote->next = ms->votes;
            ms->votes = vote;
            ms->vote_num++;
            if (ms->vote_flag == 0) {
                struct timeval tm;

                evutil_timerclear(&tm);
                tm.tv_sec = g_cf->time_window_interval;

                ms->vote_flag = 1;
                evtimer_set(&ms->time_window, vote_time_window, NULL);
                event_add(&ms->time_window, &tm);
                ms->timeout = g_cf->time_window_count;
            }

            reply_wait(vconn, vote);

            break;
        default:
            DINFO("Unknow cmd: %d\n", cmd);
            conn->destroy(conn);
            return -1;
    }

    return 0;

//error_ret:
//    return 0;
}

void
vote_read(int fd, short event, void *arg)
{
    HostEvent *he= (HostEvent *)arg;
    int datalen;
    int ret;

    if (event & EV_TIMEOUT) {
        struct in_addr ip = {he->host->ip};
        DERROR("%s timeout\n", inet_ntoa(ip));
        event_del(&he->ev);
        close(he->fd);
        return;
    }

    if (he->len >= MEMLINK_VOTE_CMD_HEAD_LEN) {
        memcpy(&datalen, he->buf, MEMLINK_VOTE_CMD_HEAD_LEN);
        datalen -= he->len - MEMLINK_VOTE_CMD_HEAD_LEN;
    } else {
        datalen = he->size - he->len;
    }
    if (datalen > he->size - he->len) {
        DERROR("received data too large\n");
        event_del(&he->ev);
        close(he->fd);
        return;
    }

    while (1) {
        ret = read(he->fd, he->buf + he->len, datalen);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EAGAIN) {
                return;
            } else {
                DERROR("read error: %s\n", strerror(errno));
                event_del(&he->ev);
                close(he->fd);
                return;
            }
        } else if (ret == 0) {
            DINFO("conn close\n");
            event_del(&he->ev);
            close(he->fd);
            return;
        } else {
            he->len += ret;
        }
        break;
    }

    if (he->len > MEMLINK_VOTE_CMD_HEAD_LEN) {
        int memlen;
        memcpy(&datalen, he->buf, MEMLINK_VOTE_CMD_HEAD_LEN);
        memlen = datalen + MEMLINK_VOTE_CMD_HEAD_LEN;
        if (he->len >= memlen) {
        //
        }
    }
    return;
}

/*
Conn *
conn_create(int fd)
{
    Conn    *conn;
    int     newfd;
    struct sockaddr_in  clientaddr;
    socklen_t slen = sizeof(clientaddr);
    int flag = 1;

    while (1) {
        newfd = accept(fd, (struct sockaddr*)&clientaddr, &slen);
        if (newfd == -1) {
            if (errno == EINTR) {
                continue;
            }else{
                DERROR("accept error: %s\n", strerror(errno));
                return NULL;
            }
        }
        break;
    }

    set_nonblock(newfd);
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    conn = (Conn*)zz_malloc(sizeof(Conn));
    if (conn == NULL) {
        DERROR("conn malloc error.\n");
        MEMLINK_EXIT;
    }
	
    memset(conn, 0, sizeof(Conn)); 
    conn->rbuf    = (char *)zz_malloc(BUF_MAX_LEN);
    conn->rsize   = BUF_MAX_LEN;
    conn->wbuf = (char *)zz_malloc(BUF_MAX_LEN);
    conn->wsize = BUF_MAX_LEN;
    conn->status = VOTE_NONE;

    if (conn->rbuf == NULL || conn->wbuf == NULL) {
        DERROR("zz_malloc error.\n");
        MEMLINK_EXIT;
    }
    conn->sock    = newfd;
    conn->destroy = conn_destroy;
    conn->wrote   = vconn_wrote;
    conn->ready = rvote_ready;

    inet_ntop(AF_INET, &(clientaddr.sin_addr), conn->client_ip, INET_ADDRSTRLEN);	
    conn->port = ntohs(clientaddr.sin_port);

    //DINFO("accept newfd: %s\n", conn->client_ip);
	
    return conn;
}
*/

void
conn_read(int fd, short event, void *arg)
{
    MainServer *ms = (MainServer *)arg;
    uint32_t ip;
    int ret;
    VoteConn *conn = NULL;

    conn = (VoteConn *)conn_create(fd, sizeof(VoteConn));
    if (conn == NULL) {
        return;
    }
    conn->base = ms->base;
    conn->ready = rvote_ready;
    conn->destroy = vconn_destroy;
    conn->read = vconn_event_read;
    conn->write = vconn_event_write;
    conn->wrote = vconn_wrote;
    conn->status = VOTE_NONE;
    conn->wbuf = (char *)zz_malloc(BUF_MAX_LEN);
    conn->wsize = BUF_MAX_LEN;
    conn->port = conn->client_port;

    if (inet_pton(AF_INET, conn->client_ip, (struct in_addr *)&ip) <= 0) {
        DERROR("inet_pton error: %s\n", strerror(errno));
        conn->destroy((Conn *)conn);
        return;
    }

    if (check_ip_valid(ip) == 0) {
        //DERROR("%s invalid, denied\n", conn->client_ip);
        set_linger(conn->sock);
        conn->destroy((Conn *)conn);
        return;
    }

    ret = change_event((Conn *)conn, EV_READ|EV_PERSIST, g_cf->timeout, 1);
    if (ret < 0) {
        DERROR("change event error.\n");
        conn->destroy((Conn *)conn);
        return;
    }
    //conn->rlen = 0;
    //DINFO("%s comes: ready to read\n", conn->client_ip);
    return;
}


/*
*return info:
*sizeof(int) + CMD_VOTE + CMD_VOTE_BACKUP(2 bytes) + vote_id(64 bits) + ip + port
*/
int
pack_master(char *buf)
{
    Host *host = g_cf->hosts;
    while (host) {
        if (host->role == ROLE_MASTER)
            break;
        host = host->next;
    }

    if (host == NULL) {
        DERROR("*** no master, ignore the vote request\n");
        return -1;
    }

    char ip[INET_ADDRSTRLEN];
    int count = 0;
    short retcode = CMD_VOTE_BACKUP;
    uint16_t port = host->port;
    int len;

    count = pack_cmd(buf, CMD_VOTE);

    memcpy(buf+count, &retcode, sizeof(retcode));
    count += sizeof(retcode);

    memcpy(buf+count, &ms->vote_id, sizeof(uint64_t));
    count += sizeof(uint64_t);

    inet_ntop(AF_INET, (struct in_addr *)&host->ip, ip, INET_ADDRSTRLEN);
    count += pack_string(buf+count, ip, 0);
    memcpy(buf+count, &port, sizeof(port));
    count += sizeof(port);

    len = count - sizeof(int);
    memcpy(buf, &len, sizeof(len));

    return count;
}

/*
*return info:
*sizeof(int) + retcode(2 bytes) + vote_id + ip + port + ... + ip + port
*/
int
pack_backup(char *buf)
{
    Host *host = g_cf->hosts;
    int count;
    char ip[INET_ADDRSTRLEN];
    uint16_t port;
    int len;
    short retcode = CMD_VOTE_MASTER;

    count = pack_cmd(buf, CMD_VOTE);

    memcpy(buf+count, &retcode, sizeof(retcode));
    count += sizeof(retcode);

    memcpy(buf+count, &ms->vote_id, sizeof(uint64_t));
    count += sizeof(uint64_t);

    while (host) {
        if (host->role == ROLE_BACKUP) {
            inet_ntop(AF_INET, &host->ip, ip, INET_ADDRSTRLEN);
            count += pack_string(buf+count, ip, 0);
            port = host->port;
            memcpy(buf+count, &port, sizeof(port));
            count += sizeof(port);
        }
        host = host->next;
    }
    len = count - sizeof(int);
    memcpy(buf, &len, sizeof(len));

    return count;
}
/*
void
vote_write(int fd, short event, void *arg)
{
    HostEvent *he = (HostEvent *)arg;
    int ret;

    if (event & EV_TIMEOUT) {
        struct in_addr ip = {he->host->ip};
        DERROR("write %s timeout\n", inet_ntoa(ip));
        event_del(&he->ev);
        close(fd);
        return;
    }

    if (he->pos == he->len) {
        DINFO("client write ok!\n");
        he->len = he->pos = 0;
        event_del(&he->ev);

        event_set(&he->ev, he->fd, EV_READ|EV_PERSIST, vote_read, he);
        event_base_set(vs->base, &he->ev);
        if (g_cf->timeout > 0) {
            struct timeval tm;
            evutil_timerclear(&tm);
            tm.tv_sec = g_cf->timeout;

            event_add(&he->ev, &tm);
        } else {
            event_add(&he->ev, 0);
        }
        return;
    }

    while (1) {
        ret = write(he->fd, he->buf + he->pos, he->len - he->pos);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EAGAIN) {
                return;
            } else {
                DERROR("write error: %s.\n", strerror(errno));
                event_del(&he->ev);
                close(he->fd);
                return;
            }
        } else {
            he->pos += ret;
        }
        break;
    }

    return;
}

int
conn_and_send(HostEvent *he)
{
    struct sockaddr_in s;
    fd_set wfd;
    int maxfd = -1;
    int ret;
    int error;
    socklen_t len = sizeof(error);

    while (he->host) {
        he->fd = socket(AF_INET, SOCK_STREAM, 0);
        if (he->fd < 0) {
            DERROR("socket error: %s\n", strerror(errno));
            continue;
        }
        if (maxfd < he->fd)
            maxfd = he->fd;
        set_nonblock(he->fd);
        bzero(&s, sizeof(s));
        s.sin_family = AF_INET;
        s.sin_addr.s_addr = he->host->ip;
        s.sin_port = htons(he->host->port);

        ret = connect(he->fd, (struct sockaddr *)&s, sizeof(s));
        if (ret < 0) {
            if (errno != EINPROGRESS && errno != EINTR) {
                DERROR("connect error: %s\n", strerror(errno));
                close(he->fd);
                continue;
            }

            FD_ZERO(&wfd);
            FD_SET(he->fd, &wfd);

            struct timeval tm;
            tm.tv_sec = 10;
            tm.tv_usec = 0;
            ret = select(he->fd + 1, NULL, &wfd, NULL, &tm);
            if (ret < 0) {
                DERROR("select error: %s\n", strerror(errno));
                close(he->fd);
                continue;
            } else if (ret == 0) {
                struct in_addr ip = {he->host->ip};
                DERROR("connect to <%s> timeout\n", inet_ntoa(ip));
                close(he->fd);
                he++;
                continue;
            }
            getsockopt(he->fd, SOL_SOCKET, SO_ERROR, &error, &len);
            if (error) {
                DERROR("connect error: %s\n", strerror(error));
                close(he->fd);
                he++;
                continue;
            }
        }
        event_set(&he->ev, he->fd, EV_WRITE|EV_PERSIST, vote_write, he);
        event_base_set(vs->base, &he->ev);
        if (g_cf->timeout > 0) {
            struct timeval tm;
            evutil_timerclear(&tm);
            tm.tv_sec = g_cf->timeout;

            event_add(&he->ev, &tm);
        } else {
            event_add(&he->ev, 0);
        }

        he++;
    }

    return 0;
}

void
vote_reply(int fd, short event, void *arg)
{
    char buf;
    size_t buflen = 1024;

    if (read(fd, &buf, 1) != 1)
        return;

    HostEvent *he = vs->hevent;
    Host *host = g_cf->hosts;

    char *mbuf = (char *)zz_malloc(buflen);
    char *bbuf = (char *)zz_malloc(buflen);

    if (mbuf == NULL || bbuf == NULL) {
        DERROR("zz_malloc error.\n");
        return;
    }

    int mlen = pack_master(mbuf);
    int blen = pack_backup(bbuf);

    while (host) {
        if (host->role == ROLE_MASTER) {
            he->buf = bbuf;
            he->len = blen;
        } else {
            he->buf = mbuf;
            he->len = mlen;
        }
        he->host = host;
        he->pos = 0;
        he->size = buflen;

        he++;
        host = host->next;
    }
    he->host = NULL;

    conn_and_send(vs->hevent);
}

int
voteserver_init()
{
    vs = (VoteReplyThread *)zz_malloc(sizeof(VoteReplyThread));
    if (vs == NULL) {
        DERROR("zz_malloc error.\n");
        return -1;
    }
    memset(vs, 0, sizeof(*vs));

    vs->base = event_base_new();

    int fd[2];

    if (pipe(fd) == -1) {
        DERROR("pipe error: %s\n", strerror(errno));
        return -1;
    }

    set_nonblock(fd[0]);
    set_nonblock(fd[1]);

    vs->notify_recv_fd = fd[0];
    vs->notify_send_fd = fd[1];

    event_set(&vs->notify_event, vs->notify_recv_fd, EV_READ|EV_PERSIST, vote_reply, NULL);
    event_base_set(vs->base, &vs->notify_event);
    event_add(&vs->notify_event, 0);

    pthread_t thread_id;
    pthread_attr_t attr;
    int ret;

    ret = pthread_attr_init(&attr);
    if (ret != 0) {
        DERROR("pthread_attr_init error: %s\n", strerror(ret));
        return -1;
    }

    ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (ret != 0) {
        DERROR("pthread_attr_setdetachstate error: %s\n", strerror(ret));
        return -1;
    }

    ret = pthread_create(&thread_id, &attr, voteserver_loop, NULL);
    if (ret != 0) {
        DERROR("pthread_create error: %s\n", strerror(ret));
        return -1;
    }

    return 0;
}

void *
voteserver_loop(void *arg)
{
    event_base_loop(vs->base, 0);
    return NULL;
}
*/

int
mainserver_init()
{
    ms = (MainServer *)zz_malloc(sizeof(MainServer));
    if (ms == NULL) {
        DERROR("malloc: %s\n", strerror(errno));
        return -1;
    }
    memset(ms, 0, sizeof(*ms));

    ms->fd = tcp_server(g_cf->server_port);
    if (ms->fd < 0) {
        DERROR("mainserver error\n");
        return -1;
    }
    if (set_nonblock(ms->fd) == -1) {
        DERROR("*** set_nonblock error\n");
        return -1;
    }

    int fd[2];
    if (pipe(fd) == -1) {
        DERROR("*** pipe error: %s\n", strerror(errno));
        return -1;
    }
    ms->notify_update_recv_fd = fd[0];
    ms->notify_update_send_fd = fd[1];

    if (set_nonblock(fd[0]) == -1) {
        DERROR("*** set_nonblock error\n");
        return -1;
    }
    if (set_nonblock(fd[1]) == -1) {
        DERROR("*** set_nonblock error\n");
        return -1;
    }

    ms->timeout = g_cf->time_window_count;

    ms->base = event_init();
    event_set(&ms->ev, ms->fd, EV_READ|EV_PERSIST, conn_read, ms);
    event_add(&ms->ev, 0);

    event_set(&ms->upev, ms->notify_update_recv_fd, EV_READ|EV_PERSIST, update_hosts, NULL);
    event_add(&ms->upev, 0);

    return 0;
}

void
server_loop()
{
    event_base_loop(ms->base, 0);

    return;
}
/*
Config *
read_config(char *file)
{
    FILE *fp = fopen(file, "r");
    char buf[1024];
    char key[10];
    char *p, *s, *b;
    int lineno = 0;

    Config *cf = (Config *)zz_malloc(sizeof(Config));
    if (cf == NULL) {
        DERROR("zz_malloc error\n");
        return NULL;
    }
    
    memset(cf, 0, sizeof(*cf));
    strcpy(cf->cf, realpath(file, buf));
    DINFO("config file: %s\n", cf->cf);

    *key = '\0';
    while (fgets(buf, 1024, fp) != NULL) {
        lineno++;
        p = buf;
        while (*p && (*p == ' ' || *p == '\t'))
            p++;
        if (*p == '#' || *p == '\r' || *p == '\n')
            continue;
        if (*p == ':') {
            DERROR("confile file error at line %d\n", lineno);
            return NULL;
        }
        char *colon;
        colon = strchr(p, ':');
        if (colon == NULL) {
            if (*key == '\0') {
                DERROR("confilg file error at line %d\n", lineno);
                return NULL;
            }
        } else {
            *colon = '\0';
            s = colon - 1;
            while (*s == ' ' || *s == '\t') {
                *s-- = '\0';
            }
            strcpy(key, p);
            p = colon + 1;
            while (*p == ' ' || *p == '\t') {
                p++;
            }
            if (*p == '\r' || *p == '\n') {
                DERROR("config file error at line %d\n", lineno);
                return NULL;
            }
        }
        s = p + strlen(p) - 1;
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
            *s-- = '\0';
        //到这里,p指向有效的字符(非空格，非冒号，非井号), 尾部的空字符和换行符也去掉了。
        if (strcmp(key, "host") == 0) {
            b = p;
            uint32_t ip;
            uint16_t port;

            while (*b != ' ' && *b != '\t')
                b++;
            *b++ = '\0';

            if (inet_pton(AF_INET, p, (struct in_addr *)&ip) < 0) {
                DERROR("config file error at line %d\n", lineno);
                return NULL;
            }
            while (*b == ' ' || *b == '\t')
                b++;

            port = atoi(b);
            Host *host = (Host *)zz_malloc(sizeof(Host));
            if (host == NULL) {
                DERROR("zz_malloc error.\n");
                return NULL;
            }
            memset(host, 0, sizeof(Host));
            host->ip = ip;
            host->port = port;
            host->role = ROLE_NONE;
            host->next = cf->hosts;
            cf->hosts = host;
            cf->host_num++;
            struct in_addr in = {host->ip};
            DINFO("host: %s : %d\n", inet_ntoa(in), host->port);
        } else if (strcmp(key, "iotimeout") == 0) {
            cf->timeout = atoi(p);
            DINFO("iotimeout: %d\n", cf->timeout);
        } else if (strcmp(key, "time_window") == 0) {
            b = p;
            while (*b != ' ' && *b != '\t')
                b++;
            *b++ = '\0';
            while (*b == ' ' || *b == '\t')
                b++;
            if (*b == '\0') {
                DERROR("config file error at line %d\n", lineno);
                return NULL;
            }
            if (strcmp(p, "interval") == 0) {
                cf->time_window_interval = atoi(b);
                if (cf->time_window_interval < 0)
                    cf->time_window_interval = 3;
                DINFO("time_window_interval: %d\n",cf->time_window_interval);
            } else if (strcmp(p, "count") == 0) {
                cf->time_window_count = atoi(b);
                if (cf->time_window_count < 0)
                    cf->time_window_count = 60;
                DINFO("time_window_count: %d\n", cf->time_window_count);
            }
        } else if(strcmp(key, "log") == 0) {
            b = p;
            while (*b != ' ' && *b != '\t')
                b++;
            *b++ = '\0';
            while (*b == ' ' || *b == '\t')
                b++;
            if (strcmp(p, "name") == 0) {
                if (*b == '\r' || *b == '\n') {
                    DERROR("config file error at line %d\n", lineno);
                    return NULL;
                }
                strcpy(cf->log_name, b);
                DINFO("log_name: %s\n", cf->log_name);
            } else if (strcmp(p, "level") == 0) {
                if (*b == '\r' || *b == '\n') {
                    DERROR("config file error at line %d\n", lineno);
                    return NULL;
                }
                cf->log_level = atoi(b);
                DINFO("log_level: %d\n", cf->log_level);
            }
        } else if(strcmp(key, "bindport") == 0) {
            cf->server_port = atoi(p);
            DINFO("server_port: %d\n", cf->server_port);
        } else if (strcmp(key, "trytimes") == 0) {
            cf->trytimes =  atoi(p);
            if (cf->trytimes < 0)
                cf->trytimes = 3;
            DINFO("trytimes: %d\n", cf->trytimes);
        } else if (strcmp(key, "daemon") == 0) {
            if (strcasecmp(p, "true") == 0) {
                cf->daemon = 1;
            } else {
                cf->daemon = 0;
            }
            DINFO("daemon: %s\n", p);
        } else if (strcmp(key, "detect_time") == 0) {
            cf->detect_time = atoi(p);
            DINFO("detect_time: %d\n", cf->detect_time);
        }
    }

    fclose(fp);
    return cf;
}
*/

/*
@param v: the value is already stripped blanks
*/
int
parser_host_port(void *cf, char *v, int i)
{
    Config *m_cf = (Config *)cf;
    char *b = strchr(v, ' ');
    if (b == NULL) {
        b = strchr(v, '\t');
        if (b == NULL) {
            DERROR("config error key<host>\n");
            return FALSE;
        }
    }
    *b = '\0';
    b++;
    while (isblank(*b)) b++;

    Host *host = (Host *)zz_malloc(sizeof(Host));
    memset(host, 0, sizeof(Host));

    inet_pton(AF_INET, v, &host->ip);
    host->port = atoi(b);

    host->role = ROLE_NONE;
    host->next = m_cf->hosts;
    m_cf->hosts = host;
    m_cf->host_num++;
    struct in_addr in = {host->ip};
    DINFO("host: %s : %d\n", inet_ntoa(in), host->port);

    return TRUE;
}

Config *
read_config(char *file)
{
    char buf[1024];
    int core = 0;

    Config *cf = (Config *)zz_malloc(sizeof(Config));
    if (cf == NULL) {
        DERROR("zz_malloc error\n");
        return NULL;
    }
    
    memset(cf, 0, sizeof(*cf));
    strcpy(cf->cf, realpath(file, buf));
    DINFO("config file: %s\n", cf->cf);
/*
    char *hosts[100] = {0};
    int i;
    for (i = 0; i < 100; i++) {
        hosts[i] = (char *)zz_malloc(100);
        memset(hosts[i], 0, 100);
    }
*/

    ConfParser * cf_p = confparser_create(file);

    //confparser_add_param(cf_p, (void *)hosts, "host", CONF_STRING, 100, NULL);
    confparser_add_param(cf_p, cf, "host", CONF_USER, 0, parser_host_port);
    confparser_add_param(cf_p, &cf->log_name, "log_name", CONF_STRING, 0, NULL);
    confparser_add_param(cf_p, &cf->log_level, "log_level", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &cf->time_window_interval, "time_window_interval", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &cf->time_window_count, "time_window_count", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &cf->timeout, "iotimeout", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &cf->server_port, "bindport", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &cf->trytimes, "trytimes", CONF_INT, 0, NULL);
    confparser_add_param(cf_p, &cf->daemon, "daemon", CONF_BOOL, 0, NULL);
    confparser_add_param(cf_p, &core, "dumpcore", CONF_BOOL, 0, NULL);

    if (confparser_parse(cf_p) < 0) {
        zz_free(cf);
        return NULL;
    }

    if (core > 0) {
        struct rlimit rlim_new;
        struct rlimit rlim;
        if (getrlimit(RLIMIT_CORE, &rlim) == 0) {
            DINFO("rlim_max %u\n", (unsigned int)rlim.rlim_max);
            rlim_new.rlim_cur = rlim_new.rlim_max = RLIM_INFINITY;
            if (setrlimit(RLIMIT_CORE, &rlim_new)!= 0) {
                /* failed. try raising just to the old max */
                rlim_new.rlim_cur = rlim_new.rlim_max = rlim.rlim_max;
                (void)setrlimit(RLIMIT_CORE, &rlim_new);
            }
        }
        if ((getrlimit(RLIMIT_CORE, &rlim) != 0) || rlim.rlim_cur == 0) {
            DERROR("failed to ensure corefile creation\n");
            MEMLINK_EXIT;
        }
    }

/*
    for (i = 0; i < 100; i++) {
        if (hosts[i]) {
            if (*(hosts[i])) {
                char *p = hosts[i];
                char *b;
                uint32_t ip;
                uint16_t port;

                b = p;
                while (*b != ' ' && *b != '\t')
                    b++;
                *b++ = '\0';
                while (*b == ' ' && *b == '\t')
                    b++;
                if (*b == '\0') {
                    DERROR("config file error, \"host\"");
                    return NULL;
                }

                if (inet_pton(AF_INET, p, (struct in_addr *)&ip) < 0) {
                    DERROR("config file error \"host\"\n");
                    return NULL;
                }

                port = atoi(b);
                Host *host = (Host *)zz_malloc(sizeof(Host));
                if (host == NULL) {
                    DERROR("zz_malloc error.\n");
                    return NULL;
                }
                memset(host, 0, sizeof(Host));
                host->ip = ip;
                host->port = port;
                host->role = ROLE_NONE;
                host->next = cf->hosts;
                cf->hosts = host;
                cf->host_num++;
                struct in_addr in = {host->ip};
                DINFO("host: %s : %d\n", inet_ntoa(in), host->port);
            }

            zz_free(hosts[i]);
        }
    }
*/

    DINFO("log_name: %s\n", cf->log_name);
    DINFO("log_level: %d\n", cf->log_level);
    DINFO("time_window_interval: %d\n", cf->time_window_interval);
    DINFO("time_window_count: %d\n", cf->time_window_count);
    DINFO("trytimes: %d\n", cf->trytimes);
    DINFO("timeout: %d\n", cf->timeout);
    DINFO("server_port: %d\n", cf->server_port);
    DINFO("daemon: %d\n", cf->daemon);
    DINFO("dumpcore: %d\n", core);

    confparser_destroy(cf_p);
    return cf;
}

int
is_host_change(Host *newhost)
{
    Host *tmp = newhost;
    Host *oldhost = g_cf->hosts;
    int update = 0;

    while (oldhost) {
        while (newhost) {
            if (newhost->ip == oldhost->ip && newhost->port == oldhost->port)
                break;
            newhost = newhost->next;
        }
        if (newhost == NULL) {
            if (oldhost->role == ROLE_MASTER) {
                DERROR("*** You can't modify MASTER's ip/port by SIGHUP, ignore the modify\n");
                return -1;
            }
            update = 1;
        }
        newhost = tmp;
        oldhost = oldhost->next;
    }

    if (update == 1)
        DINFO("*** Host ip/port modified, notify the master\n");
    return update;
}

void
sig_handler_int(int sig)
{
    DINFO("*** SIGINT handled\n");

    exit(EXIT_SUCCESS);
}

void
sig_handler_segv(int sig)
{
    DINFO("*** SIGSEGV handled\n");

    abort();
}

Host *
which_master()
{
    Host *host = g_cf->hosts;
    while (host) {
        if (host->role == ROLE_MASTER)
            return host;
        host = host->next;
    }

    return NULL;
}

void
release_config(Config *cf)
{
    Host *host, *tmp;

    host = cf->hosts;
    while (host) {
        DINFO("*** RELEASE CONFIGURE HOSTS\n");
        tmp = host->next;
        zz_free(host);
        host = tmp;
    }

    zz_free(cf);
    return;
}

void
notify_update_hosts()
{
    write(ms->notify_update_send_fd, "", 1);
    return;
}

int
pack_update(char *buf)
{
    Host *host = g_cf->hosts;
    int count;
    char ip[INET_ADDRSTRLEN];
    uint16_t port;
    int len;

    count = pack_cmd(buf, CMD_VOTE_UPDATE);

    memcpy(buf+count, &ms->vote_id, sizeof(uint64_t));
    count += sizeof(uint64_t);

    while (host) {
        if (host->role == ROLE_BACKUP) {
            inet_ntop(AF_INET, &host->ip, ip, INET_ADDRSTRLEN);
            count += pack_string(buf+count, ip, 0);
            port = host->port;
            memcpy(buf+count, &port, sizeof(port));
            count += sizeof(port);
        }
        host = host->next;
    }
    len = count - sizeof(int);
    memcpy(buf, &len, sizeof(len));

    return count;
}

void
update_hosts(int notifyfd, short event, void *arg)
{

    if (ms->vote_flag == 1) {
        DINFO("*** be voting, update host delay\n");
        return;
    }

    char buf;
    int update;

    read(notifyfd, &buf, 1);

    Config *cf =  read_config(g_cf->cf);

    if (cf == NULL) {
        DERROR("*** zz_malloc error\n");
        MEMLINK_EXIT;
    }

    update = is_host_change(cf->hosts);

    if (update == -1) {
        release_config(cf);
        return;
    } else if (update == 0) {
        if (cf->host_num > g_cf->host_num)
            update = 1;
    }

    Host *host = which_master();
    if (host == NULL) {
        DINFO("*** master unknown, ignore the modify\n");
        release_config(cf);
        return;
    }

    Config *tmp = g_cf;
    g_cf = cf;
    set_roles(host->ip, host->port);

    release_config(tmp);

    if (update == 1) {
        DINFO("*** UPDATE THE HOSTS\n");
    } else {
        DINFO("*** DO NOT UPDATE HOSTS\n");
    }

    logfile_destroy(g_log);
    logfile_create(g_cf->log_name, g_cf->log_level);

    //下面发送update信息
    struct sockaddr_in s;
    host = which_master();
    if (host == NULL) {
        DERROR("*** UPDATE HOSTS IGNORE: master unknown, something wrooooong\n");
        return;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        DERROR("*** UPDATE HOSTS IGNORE: create socket error: %s\n", strerror(errno));
        return;;
    }

    if (set_nonblock(fd) == -1) {
        DERROR("*** UPDATE HOSTS IGNORE: set_nonblok error");
        return;
    }

    VoteConn *conn = get_conn();
    conn->ready = vote_confirm;
    conn->sock = fd;
    conn->port = host->port;
    inet_ntop(AF_INET, &host->ip, conn->client_ip, INET_ADDRSTRLEN);
    conn->base = ms->base;

    conn->wlen = pack_update(conn->wbuf);

    bzero(&s, sizeof(s));
    s.sin_family = AF_INET;
    s.sin_addr.s_addr = host->ip;
    s.sin_port = htons(host->port);

    int ret = connect(fd, (struct sockaddr *)&s, sizeof(s));
    if (ret == -1) {
        if (errno != EINPROGRESS && errno != EINTR) {
            DERROR("*** UPDATE HOSTS IGNORE: connect error: %s\n", strerror(errno));
            conn->destroy((Conn *)conn);
            return;
        }
    }

    conn->status = VOTE_UPDATE_CONNECTING;
    ret = change_event((Conn *)conn, EV_WRITE|EV_PERSIST, g_cf->timeout, 1);
    if (ret == -1) {
        DERROR("*** UPDATE HOSTS change event to write error\n");
        conn->destroy((Conn *)conn);
    }

    return;
}

void
sig_handler_hup(int sig)
{
    DINFO("*** SIGHUP handled\n");
    notify_update_hosts();

    
    return;
}

int
signal_install()
{
    struct sigaction sigact;

    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;

    sigact.sa_handler = sig_handler_int;
    sigaction(SIGINT, &sigact, NULL);

    sigact.sa_handler = sig_handler_hup;
    sigaction(SIGHUP, &sigact, NULL);

    sigact.sa_handler = sig_handler_segv;
    sigaction(SIGSEGV, &sigact, NULL);

    sigact.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sigact, NULL);

    return 0;
}

int
get_opts(int argc, char **argv, char *buf, uint8_t *init)
{
    if (argc < 3)
        return -1;

    struct option opts[] = {   {"conf", 1, NULL, 'f'},
                               {"init", 0, NULL, 'i'},
                               {NULL, 0, NULL, 0},
                           };

    int optindex;
    int ret;
    int flag = 0;

    *init = 0;
    opterr = 0;
    while (1) {
        ret = getopt_long(argc, argv, "f:i", opts, &optindex);
        if (ret == -1)
            break;
        switch (ret) {
            case 'f':
                flag = 1;
                realpath(optarg, buf);
                break;
            case 'i':
                *init = 1;
                break;
            default:
                return -1;
        }
    }
    if (flag == 0)
        return -1;
    return 0;
}

int
detect_ready(Conn *conn, char *data, int len)
{
    int count = sizeof(int);
    short retcode;
    uint32_t ip;
    uint64_t voteid;
    Host *host;
//    int *active_host = (int *)conn->arg;

    inet_pton(AF_INET, conn->client_ip, (struct in_addr *)&ip);

    memcpy(&retcode, conn->rbuf+count, sizeof(retcode));
    count += sizeof(retcode);
    switch (retcode) {
        case CMD_VOTE_MASTER:
            memcpy(&voteid, conn->rbuf+count, sizeof(voteid));
            host = which_master();
            if (host) {
                DERROR("*** DETECT HOSTS fatal: multiple master\n");
                if (voteid > ms->vote_id) {
                    host->role = ROLE_BACKUP;
                } else {
                    set_role(ip, conn->port, ROLE_BACKUP);
                    break;
                }
            }
            if (ms->vote_id < voteid)
                ms->vote_id = voteid;
            set_role(ip, conn->port, ROLE_MASTER);
            DINFO("*** voteid: %llu %s(%d)\n", (unsigned long long)ms->vote_id, conn->client_ip, conn->port);
            break;
        case CMD_VOTE_BACKUP:
            memcpy(&voteid, conn->rbuf+count, sizeof(voteid));
            if (ms->vote_id < voteid)
                ms->vote_id = voteid;
            set_role(ip, conn->port, ROLE_BACKUP);
            DINFO("*** voteid: %llu %s(%d)\n", (unsigned long long)ms->vote_id, conn->client_ip, conn->port);
            break;
        case MEMLINK_ERR_NO_ROLE:
            DWARNING("*** DETECT HOSTS: %s(%d) has no role\n", conn->client_ip, conn->port);
            break;
        case MEMLINK_ERR_SLAVE:
            DWARNING("*** DETECT HOSTS: %s(%d) is in sync_slave mode\n", conn->client_ip, conn->port);
            break;
    }
/*
    *active_host -= 1;
    if (*active_host == 0)
        event_base_loopbreak(conn->base);
*/
    conn->destroy(conn);
    return 0;
}

int
detect_hosts()
{
    DINFO("*** DETECTING HOSTS ...\n");
    char buf[BUF_MAX_LEN];
    int ret;
//    int active_host = g_cf->host_num;

    int count = pack_cmd(buf, CMD_VOTE_DETECT);
    int len = count-sizeof(int);
    memcpy(buf, &len, sizeof(len));
    struct event_base *base = event_base_new();
    Host *host = g_cf->hosts;

    while (host) {
        struct sockaddr_in s;
        int fd;

        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1) {
            DERROR("*** DETECTING HOSTS: socket error: %s\n", strerror(errno));
            return -1;
        }

        if (set_nonblock(fd) < 0) {
            DERROR("*** DETECTING HOSTS: set_nonblock error\n");
            return -1;
        }

        VoteConn *conn = get_conn();
//        conn->arg = &active_host;
        conn->sock = fd;
        memcpy(conn->wbuf, buf, count);
        conn->wlen = count;
        conn->status = VOTE_DETECT_CONNECTING;
        conn->ready = detect_ready;
        inet_ntop(AF_INET, &host->ip, conn->client_ip, INET_ADDRSTRLEN);
        conn->port = host->port;
        conn->base = base;

        bzero(&s, sizeof(s));
        s.sin_family = AF_INET;
        s.sin_addr.s_addr = host->ip;
        s.sin_port = htons(host->port);

        if (connect(fd, (struct sockaddr *)&s, sizeof(s)) == -1) {
            if (errno != EINPROGRESS && errno != EINTR) {
                DERROR("*** DETECTING HOSTS: connect error: %s\n", strerror(errno));
                conn->destroy((Conn *)conn);
//                active_host--;
                host = host->next;
                continue;
            }
        }

        ret = change_event((Conn *)conn, EV_WRITE|EV_PERSIST, g_cf->timeout, 1);
        if (ret == -1) {
            DERROR("*** DETECTING HOSTS change event to write error\n");
            conn->destroy((Conn *)conn);
        }

        host = host->next;
    }

    event_base_loop(base, 0);
    event_base_free(base);

//    active_host = 0;
    host = g_cf->hosts;
    
    int master = 0;
    char ip[INET_ADDRSTRLEN];
    while (host) {
        inet_ntop(AF_INET, &host->ip, ip, INET_ADDRSTRLEN);
        if (host->role == ROLE_MASTER) {
            DINFO("*** DETECT HOSTS %s(%d) is master\n", ip, host->port);
            master = 1;
        } else if (host->role == ROLE_BACKUP) {
            DINFO("*** DETECT HOSTS %s(%d) is backup\n", ip, host->port);
        } else {
            if (master != 0)
                host->role = ROLE_BACKUP;
            DINFO("*** DETECT HOSTS %s(%d) role unknown\n", ip, host->port);
        }
        host = host->next;
    }

    if (master == 0) {
        DERROR("*** DETECT HOSTS error: no master\n");
        ms->vote_id = 0;
    }

    return 0;
}

int
main(int argc, char **argv)
{
    char cfile[PATH_MAX];
    //uint8_t init;

/*
    if (get_opts(argc, argv, cfile, &init) < 0) {
        DERROR("Usage: %s <--conf|-f config file> [--init|-i]\n", basename(argv[0]));
        MEMLINK_EXIT;
    }
*/

    if (argc != 2) {
        printf("Usage: vote configure\n");
        MEMLINK_EXIT;
    }
    strcpy(cfile, argv[1]);

    g_cf = read_config(cfile);
    if (g_cf == NULL) {
        MEMLINK_EXIT;
    }
//    exit(-1);

    logfile_create(g_cf->log_name, g_cf->log_level);

    if (mainserver_init() == -1) {
        MEMLINK_EXIT;
    }

/*
    if (voteserver_init() == -1) {
        MEMLINK_EXIT;
    }
*/
    signal_install();
    if (g_cf->daemon) {
        daemonize(1, 1);
    }

/*
    if (init == 0 && detect_hosts() < 0) {
            MEMLINK_EXIT;
    }
*/

    if (detect_hosts() < 0) {
            MEMLINK_EXIT;
    }
    server_loop();

    return 0;
}

/**
 * @}
 */
