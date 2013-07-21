/**
 * 连接处理
 * @file conn.c
 * @author zhaowei
 * @ingroup memlink
 * @{
 */
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#ifdef __linux
#include <linux/if.h>
#endif
#include <sys/un.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include "conn.h"
#include "logfile.h"
#include "zzmalloc.h"
//#include "serial.h"
#include "network.h"
#include "utils.h"
#include "defines.h"
//#include "runtime.h"
//#include "common.h"

//#define MEMLINK_EXIT abort()

/**
 * @param conn
 * @param newflag EV_READ or EV_WRITE
 * @param isnew 1 if the connection event has not been added before. 0 if not 
 * added.
 * @param timeout if positive, the event is added with a timeout.
 */
int
change_event(Conn *conn, int newflag, int timeout, int isnew)
{
    struct event      *event = &conn->evt;

    conn->iotimeout = timeout;
    /*   
     * For old event, do nothing and return if the new flag is the same as the 
     * old flag.  Otherwise, unregistered the event.
     */
    if (isnew == FALSE) { 
        if (event->ev_flags == newflag)
            return 0;

        if (event_del(event) == -1) {
            DERROR("event del error.\n");
            //return -1;
        }
    }

    if (newflag & EV_READ) {
        event_set(event, conn->sock, newflag, conn->read, (void *)conn);
    }else if (newflag & EV_WRITE) {
        event_set(event, conn->sock, newflag, conn->write, (void *)conn);
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

int        
change_sock_event(struct event_base *base, int fd, int newflag, int timeout, int isnew, 
            struct event *event, void (*func)(int fd, short event, void *args), void *arg)
{
    //WThread *wt = g_runtime->wthread;

    if (isnew == 0) {
        if (event->ev_flags == newflag)
            return 0;
        if (event_del(event) == -1) {
            DERROR("event del error.\n");
            return -1;
        }
    }
    event_set(event, fd, newflag, func, (void *)arg);
    event_base_set(base, event);

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

/**
 * Read client request, execute the command and send response. 
 *
 * @param fd
 * @param event
 * @param arg connection
 */
void
conn_event_read(int fd, short event, void *arg)
{
    Conn *conn = (Conn*)arg;
    int  ret;
    unsigned int datalen = 0;

    zz_check(conn);
    
    if (conn->is_destroy)
        return;

    if (event & EV_TIMEOUT) {
        DWARNING("read timeout:%d, close %s(%d)\n", fd, conn->client_ip, conn->client_port);
        //conn->destroy(conn);
        conn->timeout(conn);
        return;
    }
    /*
     * Called more than one time for the same command and aready receive the 
     * 2-byte command length.
     */
    if (conn->rlen >= conn->headsize) {
        memcpy(&datalen, conn->rbuf, conn->headsize);
    }
    DINFO("client read datalen: %d, fd: %d, event:%x\n", datalen, fd, event);
    DINFO("conn rlen: %d\n", conn->rlen);

    while (1) {
        int rlen = datalen;
        // If command length is unavailable, use max length.
        if (rlen == 0) {
            rlen = CONN_MAX_READ_LEN - conn->rlen;
        }
        DINFO("try read len: %d\n", rlen);
        if (conn->rsize - conn->rlen < rlen) {

            DINFO("conn->rsize: %d, conn->rlen: %d, malloc new rbuf\n", conn->rsize, conn->rlen);
            char *newbuf = (char *)zz_malloc(rlen + conn->rsize);
            memcpy(newbuf, conn->rbuf, conn->rlen);
            conn->rsize += rlen;
            zz_free(conn->rbuf);
            conn->rbuf = newbuf;
        }
        ret = read(fd, &conn->rbuf[conn->rlen], rlen);
        DINFO("read return: %d\n", ret);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }else if (errno != EAGAIN) {
                char errbuf[1024];
                strerror_r(errno, errbuf, 1024);
                DINFO("%d read error: %s, close conn.\n", fd,  errbuf);
                conn->destroy(conn);
                return;
            }else{
                char errbuf[1024];
                strerror_r(errno, errbuf, 1024);
                DERROR("%d read EAGAIN, error %d: %s\n", fd, errno,  errbuf);
            }
        }else if (ret == 0) {
            DINFO("read 0, close conn %d.\n", fd);
            conn->destroy(conn);
            return;
        }else{
            conn->rlen += ret;
            DINFO("2 conn rlen: %d\n", conn->rlen);
        }

        break;
    }

    zz_check(conn);

    DINFO("conn rbuf len: %d\n", conn->rlen);
    while (conn->rlen >= sizeof(int)) {
        //memcpy(&datalen, conn->rbuf, sizeof(short));
        memcpy(&datalen, conn->rbuf, sizeof(int));
        DINFO("check datalen: %d, rlen: %d\n", datalen, conn->rlen);
        //int mlen = datalen + sizeof(short);
        int mlen = datalen + sizeof(int);

        if (conn->rlen >= mlen) {
            conn->ready(conn, conn->rbuf, mlen);
            memmove(conn->rbuf, conn->rbuf + mlen, conn->rlen - mlen);
            conn->rlen -= mlen;

            zz_check(conn->rbuf);
        }else{
            break;
        }
    }
}

/**
 * Send data inside the connection to client. If all the data has been sent, 
 * register read event for this connection.
 *
 * @param fd
 * @param event
 * @param arg connection
 */
void
conn_event_write(int fd, short event, void *arg)
{
    Conn  *conn = (Conn*)arg;

    if (conn->is_destroy)
        return;

    if (event & EV_TIMEOUT) {
        DWARNING("write timeout:%d, close\n", fd);
        //conn->destroy(conn);
        conn->timeout(conn);
        return;
    }

    if (conn->wpos == conn->wlen) {
        DINFO("client write ok!\n");
        conn->wlen = conn->wpos = 0;
        conn->wrote(conn);
        return;
    }
    conn_write(conn);
}

/**
 * conn_create - return the accepted connection.
 */
Conn*   
conn_create(int svrfd, int connsize)
{
    Conn    *conn;
    int     newfd;
    struct sockaddr_in  clientaddr;
    socklen_t           slen = sizeof(clientaddr);

    while (1) {
        newfd = accept(svrfd, (struct sockaddr*)&clientaddr, &slen);
        if (newfd == -1) {
            if (errno == EINTR) {
                continue;
            }else{
                char errbuf[1024];
                strerror_r(errno, errbuf, 1024);
                DERROR("accept error: %s\n",  errbuf);
                return NULL;
            }
        }
        break;
    }
    tcp_server_setopt(newfd);

    conn = (Conn*)zz_malloc(connsize);
    if (conn == NULL) {
        DERROR("conn malloc error.\n");
    }
    
    memset(conn, 0, connsize); 
    conn->rbuf    = (char *)zz_malloc(CONN_MAX_READ_LEN);
    conn->rsize   = CONN_MAX_READ_LEN;
    conn->sock    = newfd;
    conn->destroy = conn_destroy;
    conn->wrote   = conn_wrote;    
    conn->timeout = conn_timeout;
    conn->read    = conn_event_read;
    conn->write   = conn_event_write;
    conn->headsize = 4;
    conn->is_destroy = FALSE;

    inet_ntop(AF_INET, &(clientaddr.sin_addr), conn->client_ip, slen);    
    conn->client_port = ntohs((short)clientaddr.sin_port);

    gettimeofday(&conn->ctime, NULL);
    //atom_inc(&g_runtime->conn_num);
    DINFO("accept newfd: %d, %s:%d\n", newfd, conn->client_ip, conn->client_port);
    
    zz_check(conn);

    return conn;
}

Conn*
conn_client_create(char *svrip, int svrport, int connsize)
{
    Conn *conn;
    int  cltfd;

    cltfd = tcp_socket_connect(svrip, svrport, 0, 0);
    if (cltfd < 0)
        return NULL;

    conn = (Conn *)zz_malloc(connsize);
    if (NULL == conn) {
        DERROR("conn malloc error.\n");
    }
    
    memset(conn, 0x0, connsize);
    conn->rbuf     = (char *)zz_malloc(CONN_MAX_READ_LEN);
    conn->rsize    = CONN_MAX_READ_LEN;
    conn->sock     = cltfd;
    conn->destroy  = conn_destroy;
    conn->wrote    = conn_wrote;
    conn->read     = conn_event_read;
    conn->write    = conn_event_write;
    conn->timeout  = conn_timeout;
    conn->headsize = 4;
    conn->is_destroy = FALSE;

    gettimeofday(&conn->ctime, NULL);
    DINFO("connect cltfd: %d, %s:%d\n", cltfd, svrip, svrport);

    zz_check(conn);

    return conn;
}

void
conn_destroy(Conn *conn)
{
/*
    int i;
    ThreadServer *ts;
    WThread *wt;
    SThread *st;
    ConnInfo *conninfo = NULL; //SyncConnInfo *sconninfo;
*/

    zz_check(conn);

    if (conn->wbuf) {
        zz_free(conn->wbuf);
    }
    if (conn->rbuf) {
        zz_free(conn->rbuf);
    }
    event_del(&conn->evt);
    
    //atom_dec(&g_runtime->conn_num);
/*
    int maxconn = 0;
    if (conn->port == g_cf->read_port) {
        ts = (ThreadServer *)conn->thread;
        if (ts) {
            conninfo = (ConnInfo *)ts->rw_conn_info;
            ts->conns--;
            maxconn = g_cf->max_read_conn;
        }
    } else if (conn->port == g_cf->write_port) {
        wt = (WThread *)conn->thread;
        conninfo = (ConnInfo *)wt->rw_conn_info;
        wt->conns--;
        maxconn = g_cf->max_write_conn;
    } else if (conn->port == g_cf->sync_port) {
        st = (SThread *)conn->thread;
        conninfo = (ConnInfo *)st->sync_conn_info;
        st->conns--;
        maxconn = g_cf->max_sync_conn;
    }

    if (conninfo) {
        for (i = 0; i < maxconn; i++) {
            if (conn->sock == conninfo[i].fd) {
                conninfo[i].fd = 0;
                break;
            }
        }

    }
*/
    close(conn->sock);
    zz_free(conn);
}

void
conn_destroy_udp(Conn *conn)
{
    if (conn->wbuf) {
        zz_free(conn->wbuf);
    }
    if (conn->rbuf) {
        zz_free(conn->rbuf);
    }
    event_del(&conn->evt);
     close(conn->sock);
    zz_free(conn);
}

int
conn_send_buffer(Conn *conn)
{
    zz_check(conn);
    zz_check(conn->wbuf);
    
    if (conn->wlen <= 0)
        return FAILED;

/*#ifdef DEBUG
    char buf[10240] = {0};
    DINFO("reply %s\n", formath(conn->wbuf, conn->wlen, buf, 10240));
#endif*/
 
    DINFO("change event to write.\n");
    int ret = change_event(conn, EV_WRITE|EV_PERSIST, conn->iotimeout, FALSE);
    if (ret < 0) { 
        DERROR("change_event error: %d, close conn.\n", ret);
        conn->destroy(conn);
        return FAILED;
    }    
    return OK;
}

char*
conn_write_buffer(Conn *conn, int size)
{
    if (conn->wsize >= size) {
        conn->wlen = conn->wpos = 0;
        return conn->wbuf;
    }

    if (conn->wbuf != NULL)
        zz_free(conn->wbuf);

    conn->wbuf  = zz_malloc(size);
    conn->wsize = size;
    conn->wlen  = conn->wpos = 0;

    return conn->wbuf;
}

inline char*
conn_write_buffer_append(Conn *conn, void *data, int datalen)
{
    if (conn->wlen + datalen > conn->wsize) {
        int   newsize = (conn->wlen + datalen) * 2;
        char *newdata = zz_malloc(newsize);
    
        if (conn->wlen > 0) {
            memcpy(newdata, conn->wbuf, conn->wlen);
        }
        zz_free(conn->wbuf);
        conn->wsize = newsize;
        conn->wbuf  = newdata;
    }
    
    memcpy(conn->wbuf + conn->wlen, data, datalen);
    conn->wlen += datalen;

    return conn->wbuf;
}

// put datalen and return code to header
int
conn_write_buffer_head(Conn *conn, int retcode, int len)
{
    if (retcode == OK) { // 注意要先调用 conn_write_buffer 确定buffer空间
        conn->wlen = len; 
        len -= sizeof(int);
    }else{
        int headlen = conn->headsize + sizeof(short);
        conn_write_buffer(conn, headlen);
        conn->wlen = headlen;
        len = headlen - conn->headsize;
    }    
    //char bufx[1024];
    //DINFO("wlen:%d, size:%d ret:%d %s\n", conn->wlen, idx, ret, formath(conn->wbuf, conn->wlen, bufx, 1024)); 
    memcpy(conn->wbuf, &len, sizeof(int));
    short retv = retcode; 
    memcpy(conn->wbuf + sizeof(int), &retv, sizeof(short));

    return OK;
}

int
conn_write_buffer_reply(Conn *conn, int retcode, char *retdata, int retlen)
{
    int  datalen = conn->headsize + sizeof(short) + retlen;

    conn_write_buffer(conn, datalen);

    int packlen = datalen - sizeof(int);
    //conn_write_buffer_head(conn, retcode, datalen);
    memcpy(conn->wbuf, &packlen, sizeof(int));
    conn->wlen += sizeof(int);
    short rcode = retcode;
    memcpy(conn->wbuf+conn->wlen, &rcode, sizeof(short));
    conn->wlen += sizeof(short);
    if (retdata != NULL && retlen > 0)
        conn_write_buffer_append(conn, retdata, retlen);
   
    zz_check(conn);
    zz_check(conn->wbuf);
 
/*#ifdef DEBUG
    char buf[10240] = {0};
    DINFO("reply %s\n", formath(conn->wbuf, conn->wlen, buf, 10240));
#endif*/
 
    return OK; 
}

int
conn_send_buffer_reply(Conn *conn, int retcode, char *retdata, int retlen)
{
    conn_write_buffer_reply(conn, retcode, retdata, retlen);
    return conn_send_buffer(conn);
}
int
conn_wrote(Conn *conn)
{
    DINFO("write complete! change event to read.\n");
    int ret = change_event(conn, EV_READ|EV_PERSIST, conn->iotimeout, 0);
    if (ret < 0) {
        DERROR("change event error:%d close socket\n", ret);
        conn->destroy(conn);
    }
    return 0;
}

int 
conn_timeout(Conn *conn)
{
    conn->destroy(conn);
    return 0;
}

void 
conn_write(Conn *conn)
{
    int ret;

    while (1) {
        DINFO("conn write len:%d\n", conn->wlen - conn->wpos);
        ret = write(conn->sock, &conn->wbuf[conn->wpos], conn->wlen - conn->wpos);
        DINFO("write: %d\n", ret);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }else if (errno != EAGAIN) {
                // maybe close conn?
                char errbuf[1024];
                strerror_r(errno, errbuf, 1024);
                DWARNING("write error! %s\n",  errbuf);
                conn->destroy(conn);
                break;
            }
        }else{
            /*char buf[512];
            DINFO("conn write, ret:%d, wlen:%d, wpos:%d, %x, wbuf:%s\n", ret, 
                    conn->wlen, conn->wpos, conn->wbuf[conn->wpos], 
                    formath(&conn->wbuf[conn->wpos], ret, buf, 512));
            */
            conn->wpos += ret;
        }
        break;
    } 
}

/*
int
conn_check_max(Conn *conn)
{
    int rconn = 0;
    int i;
    for (i = 0; i < g_cf->thread_num; i++) {
        rconn += g_runtime->server->threads[i].conns;
    }
    rconn++; // add self
   
    int allconn = rconn + g_runtime->wthread->conns + g_runtime->sthread->conns;
    DINFO("check conn: %d, %d\n", allconn, g_cf->max_conn);

    if (allconn > g_cf->max_conn) {
        return MEMLINK_ERR_CONN_TOO_MANY;
    }
    
    if (conn->port == g_cf->read_port) {
        DINFO("check read conn: %d, %d\n", rconn, g_cf->max_read_conn);
        if (g_cf->max_read_conn > 0 && rconn > g_cf->max_read_conn) {
            return MEMLINK_ERR_CONN_TOO_MANY;
        }
    }else if (conn->port == g_cf->write_port) {
        DINFO("check write conn: %d, %d\n", g_runtime->wthread->conns, g_cf->max_write_conn);
        if (g_cf->max_write_conn > 0 && g_runtime->wthread->conns > g_cf->max_write_conn) {
            return MEMLINK_ERR_CONN_TOO_MANY;
        }
    }else if (conn->port == g_cf->sync_port) {
        DINFO("check sync conn: %d, %d\n", g_runtime->sthread->conns, g_cf->max_write_conn);
        if (g_cf->max_sync_conn > 0 && g_runtime->sthread->conns > g_cf->max_sync_conn) {
            return MEMLINK_ERR_CONN_TOO_MANY;
        }
    }

    return MEMLINK_OK;
}
*/

Conn*
conn_create_udp(int sock, int connsize)
{
    Conn    *conn;

    conn = (Conn*)zz_malloc(connsize);
    if (conn == NULL) {
        DERROR("conn malloc error.\n");
    }
    
    memset(conn, 0, connsize); 
    conn->rbuf    = (char *)zz_malloc(CONN_MAX_READ_LEN);
    conn->rsize   = CONN_MAX_READ_LEN;
    conn->sock    = sock;
    conn->destroy = conn_destroy;
    
    gettimeofday(&conn->ctime, NULL);

    zz_check(conn);

    return conn;

}

void conn_destroy_real(int fd, short event, void *arg)
{
    Conn *conn = (Conn *)arg;

    conn->destroy(conn);
    return;
}

void
conn_destroy_delay(Conn *conn)
{
    if (conn->is_destroy)
        return;
    conn->is_destroy = TRUE;
    event_del(&conn->evt);

    struct timeval tm;
    evtimer_set(&conn->evt, conn_destroy_real, conn);
    evutil_timerclear(&tm);
    tm.tv_sec = 0;
    event_base_set(conn->base, &conn->evt);
    event_add(&conn->evt, &tm);

    return;
}

int
conn_destroy_check(Conn *conn)
{
    return conn->is_destroy;
}

/**
 * @}
 */
