#ifndef BASE_LOGFILE_H
#define BASE_LOGFILE_H

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <pthread.h>

#define LOG_ROTATE_NO		0
#define LOG_ROTATE_TIME		1
#define LOG_ROTATE_SIZE		2
#define LOG_ROTATE_DAY		3

#define LOG_INFO	4
#define LOG_NOTE	3
#define LOG_NOTICE	3
#define LOG_WARN	2
#define LOG_ERROR	1
//#define LOG_FATAL	0

typedef struct _logfile
{
    volatile int logfd;
    volatile int errfd; // for error
    char     filepath[PATH_MAX];
    char     errpath[PATH_MAX];
	char	 *levelstr[5];// = {"ERR", "WARN", "NOTICE", "INFO"};
    int      loglevel;
	int		 rotype; // rotate type: LOG_ROTATE_TIME(by minute), LOG_ROTATE_SIZE
	int		 maxsize; // logfile max size
	int		 maxtime; // logfile max time
	int		 count; // logfile count
	int		 check_interval;
	uint32_t last_rotate_time;
	uint32_t last_check_time; // last check time
	pthread_mutex_t lock;
	pthread_key_t	put_buf;
}LogFile;

LogFile*    logfile_create(char *filepath, int loglevel);
void        logfile_destroy(LogFile *log);
int			logfile_error_separate(LogFile *log, char *errfile);
int			logfile_put(LogFile *log, int level, char *format, ...);
void		logfile_put_flush(LogFile *log, char *file, int line);
void		logfile_rotate_day(LogFile *log);
void		logfile_rotate_size(LogFile *log, int logsize, int logcount);
void		logfile_rotate_time(LogFile *log, int logtime, int logcount);
void		logfile_rotate_no(LogFile *log);
void		logfile_flush(LogFile *log);
void        logfile_write(LogFile *log, int level, char *file, int line, char *format, ...);

extern LogFile *g_log;

#ifdef NOLOG

//#define DFATALERR(format,args...) 
#define DERROR(format,args...) 
#define DWARNING(format,args...) 
#define DNOTE(format,args...) 
#define DNOTICE(format,args...) 
#define DINFO(format,args...) 
#define DASSERT(e) ((void)0)

#define PUT_ERROR(format, ...)
#define PUT_WARNING(format, ...)
#define PUT_NOTICE(format, ...) 
#define PUT_DEBUG(format, ...)  
#define PUT_INFO(format, ...)  
#define PUT_FLUSH()	

#else

/*#define DFATALERR(format,args...) do{\
        if (!g_log) {\
            fprintf(stderr, format, ##args);\
        }else if(g_log->loglevel >= 0) {\
            logfile_write(g_log, LOG_FATAL, __FILE__, __LINE__, format, ##args);\
			exit(EXIT_FAILURE);\
        }\
    }while(0)
*/

#define DERROR(format,args...) do{\
        if (!g_log) {\
            fprintf(stderr, format, ##args);\
        }else if(g_log->loglevel >= 1) {\
            logfile_write(g_log, LOG_ERROR, __FILE__, __LINE__, format, ##args);\
        }\
    }while(0)

#define DWARNING(format,args...) do{\
        if (!g_log) {\
            fprintf(stderr, format, ##args);\
        }else if(g_log->loglevel >= 2) {\
            logfile_write(g_log, LOG_WARN, __FILE__, __LINE__, format, ##args);\
        }\
    }while(0)

#define DNOTE(format,args...) do{\
        if (!g_log) {\
            fprintf(stderr, format, ##args);\
        }else if (g_log->loglevel >= 3) {\
            logfile_write(g_log, LOG_NOTE, __FILE__, __LINE__, format, ##args);\
        }\
    }while(0)

#define DNOTICE(format,args...) do{\
        if (!g_log) {\
            fprintf(stderr, format, ##args);\
        }else if (g_log->loglevel >= 3) {\
            logfile_write(g_log, LOG_NOTICE, __FILE__, __LINE__, format, ##args);\
        }\
    }while(0)



#define DINFO(format,args...) do{\
        if (!g_log) {\
            fprintf(stderr, format, ##args);\
        }else if (g_log->loglevel >= 4) {\
            logfile_write(g_log, LOG_INFO, __FILE__, __LINE__, format, ##args);\
        }\
    }while(0)

#define DASSERT(e) \
	((void) ((!g_log) ? ((void) ((e) ? 0 : (fprintf(stderr,"assert failed: (%s) %s %s:%d\n",#e,__FUNCTION__,__FILE__,__LINE__),abort()))) : ((void) ((e) ? 0 : (logfile_write(g_log,LOG_ERROR,__FILE__,__LINE__,"assert failed: (%s) %s %s:%d\n",#e,__FUNCTION__,__FILE__,__LINE__),abort())))))
	

#define PUT_ERROR(format, ...)      logfile_put(g_log,LOG_ERROR, format, ##__VA_ARGS__)
#define PUT_WARNING(format, ...)    logfile_put(g_log,LOG_WARN, format, ##__VA_ARGS__)
#define PUT_NOTICE(format, ...)     logfile_put(g_log,LOG_NOTICE, format, ##__VA_ARGS__)
#define PUT_INFO(format, ...)       logfile_put(g_log,LOG_INFO, format, ##__VA_ARGS__)
#define PUT_DEBUG(format, ...)      logfile_put(g_log,LOG_INFO, format, ##__VA_ARGS__)
#define PUT_FLUSH()					logfile_put_flush(g_log,__FILE__, __LINE__)



#endif



#endif
