#ifndef _VOTE_H
#define _VOTE_H

#include "conn.h"

#define VOTE_REQUEST 1
#define VOTE_WAITING 2
#define VOTE_TIMEOUT 3
#define VOTE_CONTINUE 4
#define VOTE_ERR_PARAM 5
#define VOTE_OK 6

typedef struct _voteconn
{
    CONN_MEMBER
//    int status;
    uint64_t id;
    uint64_t voteid;
    uint16_t wport;
    uint8_t idflag;
}VoteConn;

typedef struct _host
{
    char ip[INET_ADDRSTRLEN];
    uint16_t port;
    struct _host *next;
}Host;

int vote_ready(Conn *conn, char *buf, int datalen);
int request_vote(uint64_t id, unsigned char idflag, uint64_t voteid, unsigned short port);

#endif
