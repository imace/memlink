/**
 * 调试信息
 * @file logfile.c
 * @author zhaowei
 * @ingroup memlink
 * @{
 */
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include "zzmalloc.h"
#include "logfile.h"
#include "utils.h"
#include "defines.h"

#define LOGFILE_ERROR(format, args...) \
    char errbuf[1024];\
    strerror_r(errno, errbuf, 1024);\
    char outbuf[1024];\
    snprintf(outbuf, 1024, format, ##args, errbuf);\
    logfile_write_real(log, TRUE, outbuf, 0);



LogFile *g_log;


LogFile*
logfile_create(char *filepath, int loglevel)
{
    LogFile *logger;

    if (strlen(filepath) >= PATH_MAX) {
        fprintf(stderr, "log file name too long: %s\n", filepath);
        exit(EXIT_FAILURE);
        return NULL;
    }
    
    logger = (LogFile*)zz_malloc(sizeof(LogFile));
    memset(logger, 0, sizeof(LogFile));
    logger->loglevel = loglevel;
    logger->check_interval = 1; // check log rotate interval

    logger->levelstr[0] = "FATAL";
    logger->levelstr[1] = "ERR";
    logger->levelstr[2] = "WARN";
    logger->levelstr[3] = "NOTICE";
    logger->levelstr[4] = "INFO";

    if (strcmp(filepath, "stdout") == 0) {
        logger->filepath[0] = 0;
        logger->logfd = STDOUT_FILENO;
    }else{
        strcpy(logger->filepath, filepath);
        logger->logfd = open(filepath, O_CREAT|O_WRONLY|O_APPEND, 0644);
        if (-1 == logger->logfd) {
            char errbuf[1024];
            strerror_r(errno, errbuf, 1024);
            fprintf(stderr, "open log file %s error: %s\n", filepath,  errbuf);
            zz_free(logger);
            exit(EXIT_FAILURE);
            return NULL;
        }
    }
    if (pthread_mutex_init(&logger->lock, NULL) != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        fprintf(stderr, "mutex init error: %s\n", errbuf);
        zz_free(logger);
        exit(EXIT_FAILURE);
    }

    if (pthread_key_create(&logger->put_buf, NULL) != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        fprintf(stderr, "mutex init error: %s\n", errbuf);
        zz_free(logger);
        exit(EXIT_FAILURE);
    }
    int ret = pthread_setspecific(logger->put_buf, NULL);
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        fprintf(stderr, "mutex init error: %s\n", errbuf);
        zz_free(logger);
        exit(EXIT_FAILURE);
    }

    g_log = logger;

    return logger;
}

void
logfile_destroy(LogFile *log)
{
    if (NULL == log) {
        return;
    }
    //fsync(log->fd);
    if (log->logfd > 2) {
        close(log->logfd); 
    }
    if (log->errfd > 2) {
        close(log->errfd); 
    }

    pthread_key_delete(log->put_buf);
 
    zz_free(log);

    g_log = NULL;
}

int
logfile_error_separate(LogFile *log, char *errfile)
{
    if (strcmp(log->filepath, "stdout") == 0 && errfile == NULL) {
        return 0;
    }
   
    if (errfile == NULL) {
        sprintf(log->errpath, "%s.err", log->filepath);
    }else{
        if (strlen(errfile) >= PATH_MAX) {
            fprintf(stderr, "err file too long: %s\n", log->errpath);
            return -1;
        }
        strcpy(log->errpath, errfile);
    }
    int fd = open(log->errpath, O_CREAT|O_WRONLY|O_APPEND, 0644);
    if (-1 == fd) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        fprintf(stderr, "open log file %s error: %s\n", log->errpath,  errbuf);
        return -1;
    }
    log->errfd = fd; 

    return 0;
}

void
logfile_rotate_day(LogFile *log)
{
    log->rotype = LOG_ROTATE_DAY;
}

void
logfile_rotate_size(LogFile *log, int logsize, int logcount)
{
    log->maxsize = logsize;
    log->count   = logcount;
    log->rotype  = LOG_ROTATE_SIZE;
}

void
logfile_rotate_time(LogFile *log, int logtime, int logcount)
{
    log->maxtime = logtime;
    log->count   = logcount;
    log->rotype  = LOG_ROTATE_TIME;
}

void
logfile_rotate_no(LogFile *log)
{
    log->rotype = LOG_ROTATE_NO;
}

void
logfile_flush(LogFile *log)
{
    if (log->logfd > 2) {
        fsync(log->logfd);
    }
    if (log->errfd > 2) {
        fsync(log->errfd);
    }
}

static inline int
logfile_write_real(LogFile *log, int iserr, char *data, int datalen)
{
    int wlen = datalen;
    int wrn  = 0;
    int ret;

    if (datalen == 0) {
        wlen = strlen(data);
    }

    while (wlen > 0) {
        if (log->errfd > 2 && iserr) {
            ret = write(log->errfd, data + wrn, wlen);   
        }else{
            ret = write(log->logfd, data + wrn, wlen);   
        }
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }else{
                break;
            }
        }
        wlen -= ret;
        wrn   += ret;
    }
    return wrn; 
}



int
logfile_rotate(LogFile *log, int iserrfd)
{
    char logdir[PATH_MAX] = {0}; 
    char *logname = NULL;
    int i;
    char logpath[PATH_MAX];

    if (iserrfd) {
        //snprintf(logpath, PATH_MAX, "%s.err", log);
        strcpy(logpath, log->errpath);
    }else{
        strcpy(logpath, log->filepath);
    }
    
    logname = strrchr(logpath, '/');
    if (logname == NULL) {
        logname = logpath;
        strcpy(logdir, ".");
    }else{
        char *copy = logpath;
        i = 0;
        while (copy < logname) {
            logdir[i] = *copy;
            i++;
            copy++;
        }
        logdir[i] = 0;
        logname++;
    }
    
    int logs[1000];
    char logname_prefix[256];
    int  logname_prefix_len;
    DIR *mydir;
    struct dirent *nodes;

    strcpy(logname_prefix, logname);
    strcat(logname_prefix, ".");
    
    logname_prefix_len = strlen(logname_prefix);
    
    //printf("logname:%s, logdir:%s, logname_prefix:%s\n", logname, logdir, logname_prefix);
    mydir = opendir(logdir);
    if (NULL == mydir) {
        LOGFILE_ERROR("open dir %s error: %s\n", logdir);
        exit(EXIT_FAILURE);
    }
    i = 0;
    while ((nodes = readdir(mydir)) != NULL) {
        if (strncmp(nodes->d_name, logname_prefix, logname_prefix_len) == 0 && \
            isdigit(nodes->d_name[logname_prefix_len])){
            int logid = atoi(&nodes->d_name[logname_prefix_len]);
            logs[i] = logid;    
            i++;
        }
    }
    closedir(mydir);
    
    qsort(logs, i, sizeof(int), compare_int);
    char filename[PATH_MAX];
    char newfilename[PATH_MAX];
    for (;i > 0; i--) {
        sprintf(filename, "%s.%d", logpath, i);
        //printf("rotate %s, count:%d, i:%d\n", filename, log->count, i);
        if (i >= log->count) {
            if (unlink(filename) == -1) {
                LOGFILE_ERROR("unlink %s error: %s\n", filename);
                exit(EXIT_FAILURE);
            }
        }else{
            sprintf(newfilename, "%s.%d", logpath, i+1);
            if (rename(filename, newfilename) == -1) {
                LOGFILE_ERROR("rename %s to %s error: %s\n", filename, newfilename);
                exit(EXIT_FAILURE);
            }
        }
    }
    sprintf(newfilename, "%s.%d", logpath, 1);
    if (rename(logpath, newfilename) == -1) {
        LOGFILE_ERROR("rename %s to %s error: %s\n", filename, newfilename);
        exit(EXIT_FAILURE);
    }
   
    //printf("reopen:%s\n", logpath);
    int fd = open(logpath, O_CREAT|O_WRONLY|O_APPEND, 0644);
    if (-1 == fd) {
        LOGFILE_ERROR("open log file %s error: %s\n", logpath);
        exit(EXIT_FAILURE);
    }
    if (iserrfd) {
        close(log->errfd);
        log->errfd = fd;
    }else{
        close(log->logfd);
        log->logfd = fd;
    }

    return 0;
}

int logfile_check_rotate(LogFile *log, time_t timenow, int iserr)
{
    int ret;
    struct stat fs;
    int fd = iserr ? log->errfd : log->logfd;

    if (fd > 2) {
        switch (log->rotype) {
            case LOG_ROTATE_SIZE:
                ret = fstat(fd, &fs);
                if (ret == -1) {
                    LOGFILE_ERROR("fstat logfd error: %s\n");
                }else{
                    if (fs.st_size >= log->maxsize) {
                        logfile_rotate(log, iserr);
                    }
                }
                break;
            case LOG_ROTATE_TIME:
                if (timenow - log->last_rotate_time >= log->maxtime) {
                    logfile_rotate(log, iserr);
                    log->last_rotate_time = timenow;
                }
                break;
            case LOG_ROTATE_DAY: {
                char filepath[PATH_MAX];  
                struct tm   timestru;
                char *path;

                if (iserr) {
                    path = log->errpath;
                }else{
                    path = log->filepath;
                }
                localtime_r(&timenow, &timestru);

                if (timestru.tm_hour == 0 && timenow - log->last_rotate_time > 3600) {
                    snprintf(filepath, PATH_MAX, "%s.%d%02d%02d", path, 
                            timestru.tm_year+1900, timestru.tm_mon+1, timestru.tm_mday); 
                    if (!isfile(filepath)) {
                        logfile_rotate(log, iserr);
                    }
                    log->last_rotate_time = timenow;
                }
                
                break;
            }
        }
    }
    return 0;
}

void
logfile_write(LogFile *log, int level, char *file, int line, char *format, ...)
{
    char    buffer[8192];
    char    *writepos;
    va_list arg;
    int     maxlen = 8192;
    int     ret, wlen = 0;
    time_t  timenow;
    char    *shortname;
    struct tm   timestru;

    if (level > log->loglevel)
        return;
   
    time(&timenow);
    localtime_r(&timenow, &timestru);

    shortname = strrchr(file, '/');
    if (shortname) {
        file = shortname + 1;
    }
    
    ret = snprintf(buffer, 8192, "%d%02d%02d %02d:%02d:%02d %lu %s:%d [%s] ", 
                    timestru.tm_year+1900, timestru.tm_mon+1, timestru.tm_mday, 
                    timestru.tm_hour, timestru.tm_min, timestru.tm_sec, 
                    (unsigned long)pthread_self(), file, line, log->levelstr[level]);

    maxlen -= ret;
    writepos = buffer + ret;
    wlen = ret;
    
    //printf("maxlen: %d\n", maxlen);
    va_start (arg, format);
    ret = vsnprintf (writepos, maxlen, format, arg); 
    wlen += ret;
    va_end (arg);

    // ERR
    int iserr = (level <= LOG_WARN) ? TRUE: FALSE;
    logfile_write_real(log, iserr, buffer, wlen);
    //logfile_write_real(log, TRUE, buffer, wlen);

	if (log->rotype > 0 && (log->logfd >2 || log->errfd > 2) && \
        timenow - log->last_check_time >= log->check_interval) {
        // maybe one thread crash case deadlock
        pthread_mutex_lock(&log->lock);
        log->last_check_time = timenow;
    
        logfile_check_rotate(log, timenow, TRUE);
        logfile_check_rotate(log, timenow, FALSE);

        pthread_mutex_unlock(&log->lock);
    }
}

#define PUT_BUFFER_SIZE  8192
int 
logfile_put(LogFile *log, int level, char *format, ...)
{
    char    buffer[PUT_BUFFER_SIZE];
    va_list arg;
    int     maxlen = PUT_BUFFER_SIZE;
    int     ret, wlen = 0;
    
    if (level > log->loglevel) {
        return 0;
    }

    char *buf = pthread_getspecific(log->put_buf);

    va_start (arg, format);
    ret = vsnprintf (buffer, maxlen, format, arg); 
    wlen += ret;
    va_end (arg);

    if (buf == NULL) {
        buf = (char*)zz_malloc(PUT_BUFFER_SIZE); 
        strcpy(buf, buffer);
            
        ret = pthread_setspecific(log->put_buf, buf);
        if (ret != 0) {
            LOGFILE_ERROR("setspecific error: %s\n");
            return -1;
        }
    }else{
        int buflen  = strlen(buf);
        int newsize = buflen + wlen + 1;
        if (newsize >= PUT_BUFFER_SIZE) {
            int allocsize = (newsize/4096 + (newsize%4096 > 0)? 1: 0) * 4096;
            char *newbuf = (char*)zz_malloc(allocsize);
            memset(newbuf, 0, allocsize);
            memcpy(newbuf, buf, buflen);
           
            buf = newbuf;

            ret = pthread_setspecific(log->put_buf, newbuf);
            if (ret != 0) {
                LOGFILE_ERROR("setspecific error: %s\n");
                return -1;
            }
        }

        strcpy(buf+buflen, buffer);

    }

    
    return 0;
}

void
logfile_put_flush(LogFile *log, char *file, int line)
{
    char *buf = pthread_getspecific(log->put_buf);
    
    if (buf == NULL) {
        return;
    }

    logfile_write(log, log->loglevel, file, line, "%s", buf);
    buf[0] = 0; 

}



/**
 * @}
 */

