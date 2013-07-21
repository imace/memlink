#ifndef MEMLINK_VOTE_H
#define MEMLINK_VOTE_H

#define MEMLINK_VOTE_HOSTS_MAX (15);

#define MEMLINK_VOTE_CMD_HEAD_LEN (sizeof(int))
#define MEMLINK_VOTE_CMD_LEN (sizeof(char))
#define CMD_VOTE_RETCODE_LEN (sizeof(short))

#define MEMLINK_ERR_PARM (-42)

#define CANVOTE (g_cf->host_num/2+1)

#define CMD_VOTE            200
#define CMD_VOTE_WAIT       201
#define CMD_VOTE_MASTER     202
#define CMD_VOTE_BACKUP     203
#define CMD_VOTE_NONEED     204
#define CMD_VOTE_UPDATE     205
#define CMD_VOTE_DETECT     206

#define BUF_MAX_LEN 1024

#define VOTE_NONE 0
#define VOTE_RECVED 1
#define VOTE_WAITING 2
#define VOTE_SENDING 3
#define VOTE_SENDED 4
#define VOTE_UPDATE_SENDED 5
#define VOTE_DETECT_SENDED 6
#define VOTE_UPDATE_SENDING 7
#define VOTE_DETECT_SENDING 8
#define VOTE_CONFIRMD 9
#define VOTE_UPDATE_CONFIRMD 10
#define VOTE_NONEED 11
//#define NONEEDVOTE_CONNECTING 12
#define VOTE_CONNECTING 13
#define VOTE_UPDATE_CONNECTING 14
#define VOTE_DETECT_CONNECTING 15
#define VOTE_PARAM_ERR 16

#include "conn.h"
#include <limits.h>

struct _vote
{
    uint64_t id;
    uint64_t vote_id;
    uint8_t id_flag;
    uint32_t host;
    uint16_t port;
    struct _vote_conn *conn;
//    struct event ev;
//    int wlen;
//    int wpos;
    struct _vote *next;
};

struct _vote_conn
{
    CONN_MEMBER

    char     trytimes;
    uint8_t    status;
    struct _vote *vote;
    void *arg;
};

typedef struct _vote_conn VoteConn;
typedef struct _vote Vote;

typedef struct _host
{
    uint32_t ip;
    uint16_t port;
    uint8_t role;
    struct _host *next;
}Host;

typedef struct _config
{
    Host *hosts;
    int host_num;
    int timeout;
    int server_port;
    int time_window_count;
    int time_window_interval;
    char log_name[PATH_MAX];
    char cf[PATH_MAX];
    int log_level;
    int trytimes;
    int detect_time;
    int daemon;
}Config;

typedef struct _host_event
{
    int fd;
    Host *host;
    char *buf;
    int len;
    int pos;
    int size;
    struct event ev;
}HostEvent;

typedef struct _vote_reply_thread
{
    struct event notify_event;
    struct event_base *base;
    int notify_recv_fd;
    int notify_send_fd;
    struct _host_event hevent[1024];
}VoteReplyThread;

typedef struct _blacklist
{
    Host *blist;
}BlackList;

typedef struct _mainserver
{
    int fd;
    struct event_base *base;
    int timeout;
    struct event time_window;
    struct event ev;
    struct event upev;
    int notify_update_recv_fd;
    int notify_update_send_fd;

    uint64_t vote_id;
    uint32_t vote_num;
    uint8_t vote_flag;
//the coming vote
    Vote *votes;
    BlackList bl;
//    char *buf;
}MainServer;

#endif
