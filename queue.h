#ifndef MEMLINK_QUEUE_H
#define MEMLINK_QUEUE_H

#include <stdio.h>
#include "wthread.h"

#define MEMLINK_QUEUE_MAX_FREE  1000

typedef struct _queue_item
{
    Conn            *conn;
    struct _queue_item   *next;
}QueueItem;

typedef struct _queue
{
    QueueItem           *head;
    QueueItem           *tail;
    pthread_mutex_t     lock;

    QueueItem           *freelist;
    int                 maxfree;
}Queue;

Queue*      queue_create();
void        queue_destroy(Queue *q);
int         queue_size(Queue *q);
int         queue_append(Queue *q, Conn *conn);
int         queue_remove_last(Queue *q, Conn *conn);
QueueItem*  queue_get(Queue *q);
void        queue_free(Queue *q, QueueItem *item);


#endif
