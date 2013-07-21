#include "taskthread.h"
#include "common.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <base/logfile.h>
#include <base/zzmalloc.h>

static void*
taskthread_run(void *arg)
{
    TaskThread  *tt = (TaskThread*)arg;
    
    DINFO("task thread:%lu\n", (unsigned long)tt->tid);

    return NULL;
}

TaskThread* 
taskthread_create()
{
    int ret;
    TaskThread  *tt = zz_malloc(sizeof(TaskThread));
    memset(tt, 0, sizeof(TaskThread));

    ret = pthread_create(&tt->tid, NULL, taskthread_run, tt);
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("pthread_create error: %s\n",  errbuf);
        MEMLINK_EXIT;
    }

    ret = pthread_mutex_init(&tt->locker, NULL);
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("pthread_mutex_init error: %s\n",  errbuf);
        MEMLINK_EXIT;
    }

    ret = pthread_cond_init(&tt->cond, NULL);
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("pthread_cond_init error: %s\n",  errbuf);
        MEMLINK_EXIT;
    }

    ret = pthread_detach(tt->tid);
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
		DERROR("pthread_detach error: %s\n",  errbuf);
        MEMLINK_EXIT;
    }

    return tt;
}


int 
taskthread_add_task(TaskThread *tt, Task *task)
{
    int retcode = MEMLINK_OK;

    pthread_mutex_lock(&tt->locker);
    if ((tt->task_addi == tt->task_readi) && tt->tasks[tt->task_addi] != NULL) {
        // full
        retcode = MEMLINK_ERR_FULL;
        goto add_task_over;
    }
    tt->tasks[tt->task_addi] = task; 
    tt->task_addi++;

    tt->task_addi = tt->task_addi % MAX_TASK;

    pthread_cond_signal(&tt->cond);
    pthread_mutex_unlock(&tt->locker);

add_task_over:
    return retcode;
}

Task*   
taskthread_get_task(TaskThread *tt, int timeout)
{
    Task *task = NULL;
    struct timespec ts;
    int ret;

    pthread_mutex_lock(&tt->locker);
    while ((tt->task_addi == tt->task_readi) && tt->tasks[tt->task_addi] == NULL) {
        // empty
        if (timeout <= 0) {
            goto get_task_over;
        }
        ts.tv_sec = time(NULL) + timeout;
        ts.tv_nsec = 0;
 
        ret = pthread_cond_timedwait(&tt->cond, &tt->locker, &ts);
        if (ret == ETIMEDOUT) 
            return NULL;
    }

    task = tt->tasks[tt->task_readi]; 
    tt->task_readi++;

    tt->task_readi = tt->task_readi % MAX_TASK;
    pthread_mutex_unlock(&tt->locker);
get_task_over:
    return task;
}
    

