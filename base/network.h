#ifndef MEMLINK_NETWORK_H
#define MEMLINK_NETWORK_H

#include <stdio.h>

int tcp_socket_server(char *host, int port);
int	tcp_socket_connect(char *host, int port, int timeout, int block);
int tcp_server_setopt(int fd);
int udp_sock_server(char *host, int port);
int set_noblock(int fd);
int set_block(int fd);

int timeout_wait(int fd, int timeout, int writing);
int timeout_wait_read(int fd, int timeout);
int timeout_wait_write(int fd, int timeout);

ssize_t readn(int fd, void *vptr, size_t n, int timeout);
ssize_t writen(int fd, const void *vptr, size_t n, int timeout);
int is_connected(int fd);

#endif
