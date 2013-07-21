#ifndef MEMLINK_WTHREAD_H
#define MEMLINK_WTHREAD_H

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#ifdef __linux
#include <linux/if.h>
#endif
#include <sys/un.h>
#include <sys/types.h>

#include "conn.h"
#include "info.h"
#include "commitlog.h"
#include "backup.h"

#define CONNECTED         500
#define NOCONNECTED       501

typedef struct _master_info
{
    struct event    hb_timer_evt;
    struct event    hbevt; // heartbeat read/write event
    int				hbsock;
    char            ip[16];
	int				read_port;
    int             write_port;
	int			    sync_port;	
    int             hb_port;
    int             id;
	//Conn			*conn;
}MasterInfo;

typedef struct _master_conn
{
    CONN_MEMBER
    //char                connecting;
    void                *item;
}MBconn;

typedef struct _backup_item
{
    MBconn              *mbconn;
    char                is_conn_destroy;
    char                ip[16];
    int                 write_port;
    int                 heartbeat_port;
    char                buffer[256];
    int                 blen;
    uint64_t            time;
    int                 state;
    unsigned char       datalen;
    struct sockaddr_in  from_addr;
    struct _backup_item *next;
}BackupItem;

typedef struct _backup_info
{
    int                 eventid;//事务id
    int                 succ;//有多少次成功
    int                 succ_conns;
    int                 hbsock;
    int                 timer_send;
    char                cltcmd[1024 * 1024];
    char                cmdlen;
    struct event        hbevt; // heartbeat read/write event
    struct event        m_send_evt;
    struct event        timer_check_evt;
	//struct event        timer_check_evt2;
    uint64_t            last_cmd_time;
    Conn                *ctlconn;
    BackupItem          *item;
}BackupInfo;



typedef struct _wthread
{
    int                 sock;
    struct event_base   *base;
    struct event        event; // listen socket event
    struct event        dumpevt; // dump event
    struct event        sync_disk_evt; // sync binlog to disk
    volatile int        indump; // is dumping now
	unsigned short      conns;
	RwConnInfo          *rw_conn_info;
    BackupInfo          *backup_info;//备的相关信息
    int                 state;
    MasterInfo          *master_info;
    CommitLog           *clog;
	int                 conn_write;
    uint64_t            vote_id;
	int					wait_port; // backup waitport
	pthread_mutex_t		rmlock; // lock for remove
}WThread;

typedef void (* CallBackFunc)(int fd, short event, void *arg);


WThread*    wthread_create();
void        wthread_destroy(WThread *wt);
void*       wthread_loop(void *arg);
void        master_hb_write(int fd, short event, void *arg);
void        master_hb_read(int fd, short event, void *arg);
void        backup_hb_write(int fd, short evnet, void *arg);
void        backup_hb_read(int fd, short event, void *arg);

void        client_read(int fd, short event, void *arg);
void        client_write(int fd, short event, void *arg);
int		    data_set_reply(Conn *conn, short retcode, char *retdata, int retlen);
int         data_reply(Conn *conn, short retcode, char *retdata, int retlen);
int         data_reply_direct(Conn *conn);
int         wdata_apply(char *data, int datalen, int writelog, Conn *conn);
int			change_event(Conn *conn, int newflag, int timeout, int isnew);
int         wdata_ready(Conn *conn, char *data, int datalen);

#endif
