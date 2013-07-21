#ifndef MEMLINK_RTHREAD_H
#define MEMLINK_RTHREAD_H

#include <stdio.h>
#include "conn.h"

int rdata_ready(Conn *conn, char *data, int datalen);

#endif

