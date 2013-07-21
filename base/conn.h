#ifndef BASE_CONN_H
#define BASE_CONN_H

#include <stdio.h>
#include <event.h>
#include <sys/time.h>

#define CONN_MAX_READ_LEN   1024

#define CONN_MEMBER \
    int     sock;\
	char    *rbuf;\
	int     rsize;\
    int     rlen;\
    char    *wbuf;\
    int     wsize;\
    int     wlen;\
    int     wpos;\
    int     port;\
	int		headsize;\
	char	client_ip[16];\
	int     client_port;\
    int		iotimeout; \
	struct event		evt;\
    struct event_base   *base;\
    struct timeval      ctime;\
	void	(*destroy)(struct _conn *conn);\
	void	(*read)(int fd, short event, void *arg);\
	void	(*write)(int fd, short event, void *arg);\
	int		(*ready)(struct _conn *conn, char *data, int datalen);\
	int		(*connected)(struct _conn *conn);\
	int		(*wrote)(struct _conn *conn);\
	int		(*timeout)(struct _conn *conn);\
	void    *thread;\
    unsigned char		vote_status; \
    char    is_destroy;

typedef struct _conn
{
	CONN_MEMBER
}Conn;

int		change_event(Conn *conn, int newflag, int timeout, int isnew);
int		change_sock_event(struct event_base *base, int fd, int newflag, int timeout, int isnew, 
					struct event *event, void (*func)(int fd, short event, void *args), void *arg);

Conn*   conn_create(int svrfd, int connsize);
Conn*   conn_client_create(char *svrip, int svrport, int connsize);
void    conn_write(Conn *conn);
void    conn_destroy(Conn *conn);
void    conn_destroy_udp(Conn *conn);
int     conn_wrote(Conn *conn);
int     conn_timeout(Conn *conn);
int		conn_send_buffer(Conn *conn);
int		conn_send_buffer_reply(Conn *conn, int retcode, char *retdata, int retlen);
char*   conn_write_buffer(Conn *conn, int size);
char*   conn_write_buffer_append(Conn *conn, void *data, int datalen);
int     conn_write_buffer_head(Conn *conn, int retcode, int len);
int		conn_write_buffer_reply(Conn *conn, int retcode, char *retdata, int retlen);
int		conn_check_max(Conn *conn);
Conn*   conn_create_udp(int sock, int connsize);
void    conn_destroy_delay(Conn *conn);
void    conn_event_read(int fd, short event, void *arg);
void    conn_event_write(int fd, short event, void *arg);

#endif
