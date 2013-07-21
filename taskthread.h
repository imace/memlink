#ifndef MEMLINK_TASKTHREAD_H
#define MEMLINK_TASKTHREAD_H

#include <stdio.h>
#include <pthread.h>

#define MAX_TASK	1024

#define TASK_CLEAN	1

#define TASK_HEAD \
	short	type;

typedef struct _memlink_task
{
	TASK_HEAD	 // task type:  clean, ...
}Task;

typedef struct _memlink_task_clean
{
	TASK_HEAD
	char	key[255];
}TaskClean;

typedef struct _memlink_taskthread
{
	pthread_t		tid;
	pthread_mutex_t	locker;
	pthread_cond_t	cond;
	Task		*tasks[MAX_TASK];  // cycle list
	int			task_addi;
	int			task_readi;
}TaskThread;

TaskThread*	taskthread_create();
void		taskthread_destroy(TaskThread*);
int			taskthread_add_task(TaskThread*, Task*);
Task*		taskthread_get_task(TaskThread*, int timeout);

#endif
