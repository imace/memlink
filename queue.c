/**
 * 事件队列
 * @file queue.c
 * @author zhaowei
 * @ingroup memlink
 * @{
 */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "queue.h"
#include "myconfig.h"
#include "logfile.h"
#include "zzmalloc.h"

Queue*      
queue_create()
{
    Queue   *q;

    q = (Queue*)zz_malloc(sizeof(Queue));
    if (NULL == q) {
        DERROR("malloc Queue error!\n");
        MEMLINK_EXIT;
        return NULL;
    }
    memset(q, 0, sizeof(Queue));
    
    int ret = pthread_mutex_init(&q->lock, NULL);
    if (-1 == ret) {
        DERROR("pthread_mutex_init error!\n");
        MEMLINK_EXIT;
        return NULL;
    }

    return q;
}

void        
queue_destroy(Queue *q)
{
    QueueItem   *item, *tmp;

    item = q->head;

    while (item) {
        tmp = item; 
        item = item->next;
        zz_free(tmp);
    }
    pthread_mutex_destroy(&q->lock);
    zz_free(q);
}

int
queue_size(Queue *q)
{
    int count = 0;    
    QueueItem    *item;

    pthread_mutex_lock(&q->lock);
    item = q->head;    
    while (item) {
        count++;
        item = item->next;
    }
    pthread_mutex_lock(&q->lock);

    return count;
}

int         
queue_append(Queue *q, Conn *conn)
{
    int ret = 0;

    QueueItem *item = (QueueItem*)zz_malloc(sizeof(QueueItem));
    if (NULL == item) {
        DERROR("malloc QueueItem error!\n");
        MEMLINK_EXIT;
        //goto queue_append_over;
    }
    item->conn = conn;
    item->next = NULL;

    pthread_mutex_lock(&q->lock);
    
    if (q->tail == NULL) {
        q->tail = item;
        q->head = item;
    }else{
        q->tail->next = item;
        q->tail = item;
    }
//queue_append_over:
    pthread_mutex_unlock(&q->lock);

    return ret;
}

int
queue_remove_last(Queue *q, Conn *conn)
{
    QueueItem    *item, *prev = NULL, *last = NULL;

    pthread_mutex_lock(&q->lock);
    item = q->head;    
    while (item) {
        prev = last;
        last = item;
        item = item->next;
    }
    if (last && last->conn == conn) {
        if (prev) {
            prev->next = NULL;
        }else{
            q->head = NULL;
        }
        close(conn->sock);
        zz_free(conn);
        zz_free(last);
    }
    pthread_mutex_unlock(&q->lock);

    return 0;
}

QueueItem*  
queue_get(Queue *q)
{
    QueueItem   *ret;

    pthread_mutex_lock(&q->lock);
    ret = q->head;
    q->head = q->tail = NULL;
    pthread_mutex_unlock(&q->lock);

    return ret;
}


void
queue_free(Queue *q, QueueItem *item)
{
    QueueItem   *tmp;
    //DINFO("queue free head:%p\n", item);
    while (item) {
        tmp = item;
        item = item->next;
        zz_free(tmp);
    }
}

/**
 * @}
 */
