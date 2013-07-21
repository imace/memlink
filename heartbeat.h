#ifndef _MEMLINK_HEARTBEAT_H
#define _MEMLINK_HEARTBEAT_H

#include "wthread.h"

int  change_event2(int fd, int newflag, int timeout, int isnew, struct event *event, CallBackFunc func, void *arg);
void master_hb_read(int fd, short event, void *arg);
void master_hb_write(int fd, short event, void *arg);
void backup_hb_write(int fd, short event, void *arg);
void backup_hb_read(int fd, short event, void *arg);
void master_create_heartbeat();
void backup_create_heartbeat();

#endif
