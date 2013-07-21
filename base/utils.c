/**
 * 通用函数
 * @file utils.c
 * @author zhaowei
 * @ingroup memlink
 * @{
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <pwd.h>
#include <poll.h>
#ifdef __APPLE__
#include <mach/mach_init.h>
#include <mach/task.h>
#include <mach/task_info.h>
#include <sys/resource.h>
#include <sys/time.h>
#endif
#include "defines.h"
#include "utils.h"
#include "logfile.h"


/**
 * 等待读或写事件
 */

/*
int 
timeout_wait(int fd, int timeout, int writing)
{
    if (timeout <= 0)
        return TRUE; // true

    if (fd < 0) {
        DERROR("fd error: %d\n", fd);
        return -1; //error
    }

    fd_set fds; 
    struct timeval tv;
    int n;

    while (1) {
        tv.tv_sec  = (int)timeout;
        tv.tv_usec = (int)((timeout - tv.tv_sec) * 1e6);

        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        if (writing)
            n = select(fd+1, NULL, &fds, NULL, &tv);
        else 
            n = select(fd+1, &fds, NULL, NULL, &tv);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            char errbuf[1024];
            strerror_r(errno, errbuf, 1024);
            DERROR("select error: %d, %s\n", n,  errbuf);
            return -1; // error
        }
        break;
    }

    if (n == 0)
        return FALSE; // false
    return TRUE;  // true
}*/

/*
int
timeout_wait(int fd, int timeout, int writing)
{
    if (timeout <= 0)
        return TRUE; // true

    if (fd < 0) {
        DERROR("timeout_wait fd error: %d\n", fd);
        return FALSE; //error
    }
    // second to millisecond 
    timeout = timeout * 1000;
    struct pollfd fds[1];
    int ret;
    //struct timeval start, end;

    fds[0].fd = fd;
    while (1) {
        //gettimeofday(&start, NULL);
        if (writing)
            fds[0].events = POLLOUT;
        else
            fds[0].events = POLLIN;

        ret = poll(fds, 1, timeout);
        if (ret < 0) {
            if (errno == EINTR) {
                //gettimeofday(&end, NULL);
                //unsigned int td = timediff(&start, &end);
                //timeout -= td / 1000;
                //if (timeout <= 0)
                //    return FALSE;
                continue;
            }
            char errbuf[1024];
            strerror_r(errno, errbuf, 1024);
            DWARNING("timeout_wait poll error: %d, %s\n", fds[0].fd,  errbuf);
            return FALSE;
        }
        break;
    }
    if ((fds[0].revents & POLLOUT) && writing)
        return TRUE;

    if ((fds[0].revents & POLLIN) && !writing)
        return TRUE;

    return FALSE;
}

int
timeout_wait_read(int fd, int timeout)
{
    return timeout_wait(fd, timeout, FALSE);
}

int
timeout_wait_write(int fd, int timeout)
{
    return timeout_wait(fd, timeout, TRUE);
}

/
 * readn - try to read n bytes with the use of a loop
 *
 * Return the bytes read. On error, -1 is returned.
 /
ssize_t 
readn(int fd, void *vptr, size_t n, int timeout)
{
    size_t  nleft;
    ssize_t nread;
    char    *ptr;

    ptr = vptr;
    nleft = n;

    while (nleft > 0) {
        if (timeout > 0 && timeout_wait_read(fd, timeout) != TRUE) {
            DWARNING("read timeout.\n");
            break;
        }
        if ((nread = read(fd, ptr, nleft)) < 0) {
            char errbuf[1024];
            strerror_r(errno, errbuf, 1024);
            //DERROR("nread: %d, error: %s\n", nread,  errbuf);
            if (errno == EINTR) {
                nread = 0;
            }else {
                char errbuf[1024];
                strerror_r(errno, errbuf, 1024);
                DERROR("readn error: %s\n",  errbuf);
                //MEMLINK_EXIT;
                return -1;
            }
        }else if (nread == 0) {
            DERROR("read 0, maybe conn close.\n");
            break;
        }
        nleft -= nread;
        ptr += nread;
    }

    return (n - nleft);
}

/
 * writen - write n bytes with the use of a loop
 *
 /
ssize_t
writen(int fd, const void *vptr, size_t n, int timeout)
{
    size_t  nleft;
    ssize_t nwritten;
    const char *ptr;

    ptr = vptr;
    nleft = n;

    while (nleft > 0) {
        //DINFO("try write %d via fd %d\n", (int)nleft, fd);
        if (timeout > 0 && timeout_wait_write(fd, timeout) != TRUE) {
            break;
        }

        if ((nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR){
                nwritten = 0;
            }else{
                char errbuf[1024];
                strerror_r(errno, errbuf, 1024);
                DERROR("writen error: %s\n",  errbuf);
                //MEMLINK_EXIT;
                return -1;
            }
        }
        //char buf[1024];
        //DINFO("nwritten: %d, %s\n", (int)nwritten, formath((char*)ptr, (int)nwritten, buf, 1024));
        nleft -= nwritten;
        ptr += nwritten;
    }
    return n;
}
*/

/**
 * 显示为2进制
 */
void 
printb(char *data, int datalen)
{
    int i, j;
    unsigned char c;

    for (i = 0; i < datalen; i++) {
        c = 0x80;
        for (j = 0; j < 8; j++) {
            if (c & data[datalen - i - 1]) {
                printf("1");
            }else{
                printf("0");
            }   
            c = c >> 1;
        }   
        printf(" ");
    }   
    printf("\n");
}

/**
 * 显示为16进制
 */
void 
printh(char *data, int datalen)
{
    int i;
    unsigned char c;

    for (i = datalen - 1; i >= 0; i--) {
        c = data[i];
        printf("%02x ", c);
    }   
    printf("\n");
}

/** 
 * 按2进制数格式化，从后往前
 */
char*
formatb(char *data, int datalen, char *buf, int blen)
{
    int i, j;
    unsigned char c;
    int idx = 0;
    int maxlen = blen - 1;

    buf[maxlen] = 0;

    for (i = 0; i < datalen; i++) {
        c = 0x80;
        for (j = 0; j < 8; j++) {
            if (c & data[datalen - i - 1]) {
                buf[idx] = '1';
            }else{
                buf[idx] = '0';
            }   
            idx ++;
            if (idx >= maxlen) {
                return buf;
            }
            c = c >> 1;
        }   
        buf[idx] = ' ';
        idx ++;

        if (idx >= maxlen) {
            return buf;
        }
    }   
    buf[idx] = 0;

    return buf;
}

/**
 * 按16进制数格式化，从后往前
 */
char*
formath(char *data, int datalen, char *buf, int blen)
{
    int i;
    unsigned char c;
    int idx = 0;
    int maxlen = blen - 4;

    for (i = datalen - 1; i >= 0; i--) {
        if (idx >= maxlen)
            break;
        c = data[i];
        snprintf(buf + idx, blen-idx, "%02x ", c);
        idx += 3;
    }   
    buf[idx] = 0;

    return buf;
}

void
timestart(struct timeval *start)
{
    gettimeofday(start, NULL);
}

unsigned int
timeend(struct timeval *start, struct timeval *end, char *info)
{
    gettimeofday(end, NULL);
    unsigned int td = timediff(start, end);

    DNOTE("=== %s time: %u us ===\n", info, td);
    return td;
}

/**
 * 两个时间点之间的时间差，单位为微秒
 */
unsigned int 
timediff(struct timeval *start, struct timeval *end)
{
    return 1000000 * (end->tv_sec - start->tv_sec) + (end->tv_usec - start->tv_usec);
}

/**
 * Test whether the path exists and is a regular file.
 * 
 * @return 1 if the path exists and is a regular file, 0 otherwise.
 */
int
isfile (char *path)
{
    struct stat buf;
    if (stat(path, &buf) != 0)
        return 0;
    if (!S_ISREG(buf.st_mode))
        return 0;
    return 1;
}

/**
 * 是否为目录
 */
int
isdir (char *path)
{
    struct stat buf;
    if (stat(path, &buf) != 0)
        return 0;
    if (!S_ISDIR(buf.st_mode))
        return 0;
    return 1;
}

/**
 * 是否为符号连接
 */
int
islink (char *path)
{
    struct stat buf;
    if (stat(path, &buf) != 0)
        return 0;
    if (!S_ISLNK(buf.st_mode))
        return 0;
    return 1;
}

/**
 * Test whether the path exists.
 * 
 * @return 1 if the path exists, 0 otherwise.
 */
int
isexists (char *path)
{
    struct stat buf;
    if (stat(path, &buf) != 0)
        return 0;
    if (!S_ISDIR(buf.st_mode) && !S_ISREG(buf.st_mode) && !S_ISLNK(buf.st_mode))
        return 0;
    return 1;
}

/**
 * 获取文件大小
 */
long long
file_size(char *path)
{
    struct stat buf;
    if (stat(path, &buf) != 0) {
        return -1;
    }
    if (S_ISREG(buf.st_mode)) {
        return buf.st_size;
    }
    return -2;
}

/**
 * 整型比较，给qsort用
 */
int 
compare_int ( const void *a , const void *b ) 
{ 
    return *(int *)a - *(int *)b; 
} 

/**
 * 必须写入指定长度
 */
size_t
ffwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t ret = fwrite(ptr, size, nmemb, stream);
    if (ret != nmemb) {
        DERROR("fwrite error, write:%d, must:%d\n", (int)ret, (int)nmemb);
        //MEMLINK_EXIT;
        exit(EXIT_FAILURE);
    }
    return ret;
}

/**
 * 必须要读到指定长度
 */
size_t
ffread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t ret = fread(ptr, size, nmemb, stream);
    if (ret != nmemb) {
        DERROR("fread error, read:%d, must:%d\n", (int)ret, (int)nmemb);
        //MEMLINK_EXIT;
        exit(EXIT_FAILURE);
    }
    return ret;
}

/**
 * 改变进程的user/group
 */
int
change_group_user(char *user)
{
    if (user == NULL || user[0] == 0) {
        //DNOTE("not set user/group.\n");
        return 0;
    }
    
    struct passwd *pw;

    pw = getpwnam(user);
    if (pw == NULL) {
        DERROR("can't find user %s to switch.\n", user);
        return -1;
    }
    int ret;
    ret = setgid(pw->pw_gid);
    if (ret < 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("failed to set group: %d, %s\n", pw->pw_gid,  errbuf);
        return -1;
    }
    ret = setuid(pw->pw_uid);
    if (ret < 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("failed to set user %s, %s\n", user,  errbuf);
        return -1;
    }
    
    return 0;
}

/*int
check_thread_alive(pthread_t id)
{
    int err;

    err = pthread_kill(id, 0);
    if (err == ESRCH)
        return 0;
    else
        return 1;
}

int
wait_thread_exit(pthread_t id)
{
    int err;

    pthread_kill(id, SIGUSR1);
    
    err = pthread_kill(id, 0);

    while (err != ESRCH) {
        err = pthread_kill(id, 0);
        usleep(10);
    }
    return 1;
}

int
thread_exit(pthread_t id)
{
    pthread_kill(id, SIGUSR1);
    return 1;
}*/

/**
 * 获取进程占用内存
 */
long long
get_process_mem(int pid)
{
#ifdef __linux
    char buf[256];
    snprintf(buf, 256, "/proc/%d/status", pid);

    FILE *fp;
    fp = (FILE *)fopen(buf, "r");
    if (fp == NULL) {
        DERROR("open file %s error! \n", buf);
        return -1;
    }

    DINFO("get memlink mem size.\n");
    char line[512];
    char num[512] = {0};
    while (fgets(line, 512, fp)) {
        if (strncmp(line, "VmRSS", 5) == 0) {
            DINFO("line: %s", line);
            char *ptr = line;
            char *ptrnum = num;

            while (*ptr != '\0') {
                if (*ptr >= '0' && *ptr <= '9')
                    break;
                ptr++;
            }
            while (*ptr >= '0' && *ptr <= '9')
                *ptrnum++ = *ptr++;
            break;
        }
    }
    fclose(fp);
    if (num[0] != '\0') {
        return strtoll(num, NULL, 10) * 1024;
    }
#endif

#ifdef __APPLE__
    struct task_basic_info  info;
    kern_return_t           rval = 0;
    mach_port_t             task = mach_task_self();
    mach_msg_type_number_t  tcnt = TASK_BASIC_INFO_COUNT;
    task_info_t             tptr = (task_info_t) &info;
    
    memset(&info, 0, sizeof(info));
    
    rval = task_info(task, TASK_BASIC_INFO, tptr, &tcnt);
    if (!(rval == KERN_SUCCESS)) return 0;
   
    return info.resident_size;
#endif

    return 0;
}

int
truncate_file(int fd, int len)
{
    int ret;
    while (1) {
        ret = ftruncate(fd, len);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }else{
                char errbuf[1024];
                strerror_r(errno, errbuf, 1024);
                DERROR("ftruncate %d, %d error: %s\n", fd, len,  errbuf);
                //MEMLINK_EXIT;
            }
        }
        break;
    }
    return 0;
}

int
int2string(char *s, unsigned int val)
{
    unsigned int v = val;
    int yu; 
    int ret, i = 0, j = 0;
    char ss[32];

    if (v == 0) {
        s[0] = '0';
        return 1;    }   

    if (v == UINT_MAX)
        return 0;

    while (v > 0) {
        yu = v % 10; 
        v  = v / 10; 

        ss[i] = yu + 48; 
        i++;
    }   
    ret = i;

    for (i = i - 1; i >= 0; i--) {
        s[j] = ss[i];
        j++;
    }   

    return ret;
}


int	
create_filename(char *filename)
{
    char ckname[PATH_MAX];
    int  i = 1;

    strcpy(ckname, filename);
    while (isfile(ckname)) {
        snprintf(ckname, PATH_MAX, "%s.%d", filename, i);
        i++;
    }
    strcpy(filename, ckname);

    return 0;
}
/**
 * @}
 */
