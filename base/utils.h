#ifndef MEMLINK_UTILS_H
#define MEMLINK_UTILS_H

#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>

int		timeout_wait(int fd, int timeout, int writing);
int		timeout_wait_read(int fd, int timeout);
int		timeout_wait_write(int fd, int timeout);

ssize_t readn(int fd, void *vptr, size_t n, int timeout);
ssize_t writen(int fd, const void *vptr, size_t n, int timeout);
void    printb(char *data, int datalen);
void    printh(char *data, int datalen);
char*   formatb(char *data, int datalen, char *buf, int blen);
char*   formath(char *data, int datalen, char *buf, int blen);

void         timestart(struct timeval *start);
unsigned int timeend(struct timeval *start, struct timeval *end, char *info);
unsigned int timediff(struct timeval *start, struct timeval *end);

int			isfile(char *path);
int			isdir(char *path);
int			islink(char *path);
int			isexists(char *path);
long long	file_size(char *path);
int			compare_int ( const void *a , const void *b );

size_t		ffwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t		ffread(void *ptr, size_t size, size_t nmemb, FILE *stream);

int			change_group_user(char *user);
//int         check_thread_alive(pthread_t id);
//int         wait_thread_exit(pthread_t id);
//int         thread_exit(pthread_t id);
long long   get_process_mem(int pid);
int         truncate_file(int fd, int len);
int			int2string(char *s, unsigned int val);
int			create_filename(char *filename);

/*static inline void		
atom_inc(unsigned int *v)
{
	asm volatile ("lock; incl %0"
			: "+m" (*v));
}

static inline void		
atom_dec(unsigned int *v)
{
	asm volatile ("lock; decl %0"
			: "+m" (*v));
}*/


#endif
