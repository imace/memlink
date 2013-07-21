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
#include <signal.h>
#include <sys/select.h>

#include "vote.h"
#include "../common.h"
#include "logfile.h"
#include "utils.h"

#define VOTE_HOST "127.0.0.1"

int listenfd;
int master;

int iotimeout = 0;
uint64_t id = 0;
uint8_t idflag = 0;
uint64_t vote_id = 0;
uint16_t port = 0;

int
set_signal()
{
    signal(SIGPIPE, SIG_IGN);
    return 0;
}

/*
*sizeof(int) + cmd(CMD_VOTE) + id(64 bits) + idflag(8 bits) + vote_id(64 bits) + port(16 bits)
*/
int
pack_vote(char *buf, uint64_t id, uint8_t idflag, uint64_t vote_id, uint16_t port)
{
    int count = sizeof(int);
    unsigned char cmd = CMD_VOTE;
    memcpy(buf+count, &cmd, sizeof(cmd));
    count += sizeof(cmd);
    memcpy(buf+count, &id, sizeof(id));
    count += sizeof(id);
    memcpy(buf+count, &idflag, sizeof(idflag));
    count += sizeof(idflag);
    memcpy(buf+count, &vote_id, sizeof(vote_id));
    count += sizeof(vote_id);
    memcpy(buf+count, &port, sizeof(port));
    count += sizeof(port);
    int len = count - sizeof(int);
    memcpy(buf, &len, sizeof(len));
    DINFO("len: %d, count: %d\n", len, count);
    return count;
}

int
cmd_vote_unpack(char *data)
{
    int count = 0;
    uint64_t id;
    uint8_t idflag;
    uint64_t vote_id;
    uint16_t port;

    memcpy(&id, data, sizeof(uint64_t));
    count += sizeof(uint64_t);
    memcpy(&idflag, data+count, sizeof(uint8_t));
    count += sizeof(uint8_t);
    memcpy(&vote_id, data+count, sizeof(uint64_t));
    count += sizeof(uint64_t);
    memcpy(&port, data+count, sizeof(uint16_t));

    DINFO("id: %llu, idflag: %d, vote_id: %llu, port: %u\n", (unsigned long long)id, idflag, (unsigned long long)vote_id, port);

    return 0;
}

int
unpack_vote_ret(char *buf, int datalen)
{
    return 0;
}

void            
set_linger(int fd)
{               
    struct linger ling = {1, 0};
    setsockopt(fd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
}
/*
int
timeout_wait(int fd, int timeout, int r)
{
    if (timeout <= 0)
        return 0;

    fd_set fdset;
    struct timeval tm;
    int ret;

    while (1) {
        FD_ZERO(&fdset);
        FD_SET(fd, &fdset);
        tm.tv_sec = timeout;
        tm.tv_usec = 0;

        if (r != 0) {
            ret = select(fd+1, &fdset, NULL, NULL, &tm);
        } else {
            ret = select(fd+1, NULL, &fdset, NULL, &tm);
        }
        
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                DERROR("select: %s\n", strerror(errno));
                return -1;
            }
        } else if (ret == 0) {
            DERROR("timeout: %s\n", strerror(ETIMEDOUT));
            return -1;
        } else {
            return 0;
        }
    }
}
*/

int
client_writen(int fd, char *buf, size_t datalen, int timeout)
{
    size_t nleft;
    ssize_t nwritten;
    const char *ptr;

    ptr = buf;
    nleft = datalen;

    while (nleft > 0) {
        if (timeout > 0 && timeout_wait(fd, timeout, 0) == -1) {
            DERROR("write timeout\n");
            break;
        }
        if ((nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR) {
                nwritten = 0;
            } else {
                DERROR("writen error: %s\n", strerror(errno));
                return -1;
            }
        }

        nleft -= nwritten;
        ptr += nwritten;
    }

    return datalen-nleft;
}

int
client_readn(int fd, char *buf, int datalen, int timeout)
{
    size_t nleft;
    ssize_t nread;
    char *ptr;

    ptr = buf;
    nleft = datalen;

    while (nleft > 0) {
        if (timeout > 0 && timeout_wait(fd, timeout, 1) == -1) {
            DERROR("read timeout\n");
            break;
        }
        if ((nread = read(fd, ptr, nleft)) == -1) {
            if (errno == EINTR) {
                nread = 0;
            } else {
                DERROR("readn error: %s\n", strerror(errno));
                return -1;
            }
        } else if (nread == 0) {
            DERROR("read 0, conn cloes\n");
            break;
        }
        nleft -= nread;
        ptr += nread;
    }

    return datalen-nleft;
}

int
unpack_string(char *s, char *v, unsigned char *vlen)
{
    unsigned char len = 0;


    while (*s != '\0') {
        *v++ = *s++;
        len++;
    }
    *v = '\0';

    if (vlen) {
        *vlen = len;
    }
    return len + 1;
}

int
unpack_voteid(char *buf, uint64_t *vote_id)
{
    memcpy(vote_id, buf, sizeof(*vote_id));
    return sizeof(*vote_id);
}

int
unpack_votehost(char *buf, char *ip, uint16_t *port)
{
    int count = 0;
    count = unpack_string(buf, ip, NULL);
    memcpy(port, buf+count, sizeof(*port));
    count += sizeof(*port);
    return count;
}

int
read_cmd_response(int fd, char *buf)
{
    int count = client_readn(fd, buf, sizeof(int), iotimeout);
    if (count == 0)
        return 0;
    if (count != sizeof(int)) {
        DERROR("readn data invalid\n");
        set_linger(fd);
        close(fd);
        return -1;
    }
    memcpy(&count, buf, sizeof(int));
    DINFO("count: %d\n", count);
    int ret = client_readn(fd, buf, count, iotimeout);
    if (ret != count) {
        DERROR("readn data invalid\n");
        set_linger(fd);
        close(fd);
        return -1;
    }
    return count;
}

void
replyok(int fd)
{
    char buf[1024];

    short retcode = MEMLINK_OK;
    int len = sizeof(retcode);

    memcpy(buf, &len, sizeof(len));
    memcpy(buf+sizeof(int), &retcode, sizeof(retcode));
    write(fd, buf, sizeof(int) + sizeof(short));
    return;
}

void
replyerr(int fd)
{
    char buf[1024];

    short retcode = MEMLINK_ERR_NOT_MASTER;
    int len = sizeof(retcode);

    memcpy(buf, &len, sizeof(len));
    memcpy(buf+sizeof(int), &retcode, sizeof(retcode));
    write(fd, buf, sizeof(int) + sizeof(short));
    return;
}

int
reply_pack(int fd)
{
    char buf[1024];
    int count = sizeof(int);;
    short retcode;

    if (master == 1) {
        retcode = CMD_VOTE_MASTER;
    } else {
        retcode = CMD_VOTE_BACKUP;
    }

    memcpy(buf+count, &retcode, sizeof(retcode));
    count += sizeof(retcode);
    memcpy(buf+count, &vote_id, sizeof(vote_id));
    count += sizeof(retcode);

    int len = count - sizeof(int);
    memcpy(buf, &len, sizeof(len));

    client_writen(fd, buf, count, iotimeout);

    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 6) {
        DERROR("Usage: %s <id> <idflag> <vote_id> <port> <I/O timeout>\n", argv[0]);
        exit(-1);
    }

    set_signal();
    id = atoi(argv[1]);
    idflag = atoi(argv[2]);
    vote_id = atoi(argv[3]);
    port = atoi(argv[4]);
    iotimeout = atoi(argv[5]);

    //DINFO("id: %llu, idflag: %d\nvote_id: %llu\nport: %d\ntimeout: %d\n", id, idflag, vote_id, port, iotimeout);
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        DERROR("socket: %s\n", strerror(errno));
        MEMLINK_EXIT;
    }
    int flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    struct sockaddr_in s;
    bzero(&s, sizeof(s));
    s.sin_family = AF_INET;
    s.sin_addr.s_addr = htonl(INADDR_ANY);
    s.sin_port = htons(port);

    int ret;
    ret = bind(listenfd, (struct sockaddr *)&s, sizeof(s));
    if (ret == -1) {
        DERROR("bind: %s\n", strerror(errno));
        MEMLINK_EXIT;
    }
    
    if (listen(listenfd, 128) == -1) {
        DERROR("listen: %s\n", strerror(errno));
        MEMLINK_EXIT;
    }

    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1) {
        DERROR("socket: %s\n", strerror(errno));
        MEMLINK_EXIT;
    }
    setsockopt(clientfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    s.sin_family = AF_INET;
    s.sin_port = htons(11010);
    inet_pton(AF_INET, VOTE_HOST, &s.sin_addr);


    ret = connect(clientfd, (struct sockaddr *)&s, sizeof(s));
    if (ret == -1) {
        DERROR("connect: %s\n", strerror(errno));
        MEMLINK_EXIT;
    }

    char buf[1024];
    int count = pack_vote(buf, id, idflag, vote_id, port);
    cmd_vote_unpack(buf+CMD_REQ_HEAD_LEN);
    if (client_writen(clientfd, buf, count, iotimeout) != count) {
        DERROR("write count should be %d\n", count);
        set_linger(clientfd);
        close(clientfd);
        return -1;
    }

    fd_set rset;
    FD_ZERO(&rset);
    int closed = 0;
    while (1) {
        if (closed == 0)
            FD_SET(clientfd, &rset);
        FD_SET(listenfd, &rset);
        int maxfd = clientfd > listenfd ? clientfd : listenfd;

        ret = select(maxfd+1, &rset, NULL, NULL, NULL);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                DERROR("*** select error: %s\n", strerror(errno));
                exit(-1);
            }
        }

        if (FD_ISSET(listenfd, &rset)) {
            clientfd = accept(listenfd, NULL, 0);
            closed = 0;
        }
        if (FD_ISSET(clientfd, &rset)) {
            count = read_cmd_response(clientfd, buf);
            if (count < 0) {
                DERROR("response error\n");
                return -1;
            } else if (count == 0) {
                DINFO("*** voter closed\n");
                close(clientfd);
                closed = 1;
                FD_CLR(clientfd, &rset);
                continue;
            }

            char *ptr = buf;
            char ip[INET_ADDRSTRLEN];
            uint16_t pt;
            uint64_t v_id;
            uint8_t cmd;
            short rcode;

            memcpy(&cmd, ptr, sizeof(cmd));
            ptr += sizeof(cmd);
            count -= sizeof(cmd);
            DINFO("cmd: %u\n", cmd);
            switch (cmd) {
                case CMD_VOTE_DETECT:
                    reply_pack(clientfd);
                    break;
                case CMD_VOTE_UPDATE:
                    DINFO("*** UPDATE\n");
                    ret = unpack_voteid(ptr, &v_id);
                    DINFO("voteid: %llu\n", (unsigned long long)v_id);
                    ptr += ret;
                    count -= ret;
                    if (master == 1) {
                        DINFO("I'm master\n");
                    } else {
                        DINFO("I'm backup\n");
                    }
                    while (count > 0) {
                        ret = unpack_votehost(ptr, ip, &pt);
                        DINFO("HOST: %s(%d)\n", ip, pt);
                        ptr += ret;
                        count -= ret;
                    }
                    if (master == 1) {
                        replyok(clientfd);
                    } else {
                        replyerr(clientfd);
                    }

                    break;
                case CMD_VOTE:
                    memcpy(&rcode, ptr, sizeof(rcode));
                    ptr += sizeof(rcode);
                    count -= sizeof(rcode);
                    if (rcode == CMD_VOTE_BACKUP)
                        master = 0;
                    switch (rcode) {
                        case CMD_VOTE_WAIT:
                            DINFO("I should wait vote finished\n");

                            break;
                        case CMD_VOTE_MASTER:
                            master = 1;
                        case CMD_VOTE_BACKUP:
                            DINFO("VOTE SUCCESS\n");
                            ret = unpack_voteid(ptr, &v_id);
                            DINFO("voteid: %llu\n", (unsigned long long)v_id);
                            vote_id = v_id;
                            ptr += ret;
                            count -= ret;
                            if (master == 1) {
                                DINFO("I'm master\n");
                            } else {
                                DINFO("I'm backup\n");
                            }
                            while (count > 0) {
                                ret = unpack_votehost(ptr, ip, &pt);
                                DINFO("HOST: %s(%d)\n", ip, pt);
                                ptr += ret;
                                count -= ret;
                            }
                            replyok(clientfd);

                            break;
                        case CMD_VOTE_NONEED:
                            DINFO("*** no need vote\n");

                            break;
                        case MEMLINK_ERR_VOTE_PARAM:
                            DERROR("id flag error: %d\n", idflag);

                            break;
                        default:
                            break;
                    }

                    break;
                default:
                    break;
            }
        }
    }
    return 0;
}

/**
 * @}
 */
