#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include "base/logfile.h"
#include "base/utils.h"

int testcount(char *filename, int *err, int *warn, int *note, int *info)
{
    FILE    *f;
    int lines = 0;
    int line_err = 0, line_warn = 0, line_note = 0, line_info = 0;
    char buffer[8192];

    f = fopen(filename, "r");
    if (NULL == f) {
        return -1;
    }
    
    while (fgets(buffer, 8192, f) != NULL) {
        lines++;
        if (strstr(buffer, "[ERR]") != NULL) {
            line_err++;
        }else if (strstr(buffer, "[WARN]") != NULL) {
            line_warn++;
        }else if (strstr(buffer, "[NOTE]") != NULL || strstr(buffer, "[NOTICE]") != NULL) {
            line_note++;
        }else if (strstr(buffer, "[INFO]") != NULL) {
            line_info++;
        }
    }
    fclose(f);
    
    *err  = line_err;
    *warn = line_warn;
    *note = line_note;
    *info = line_info;

    return lines;
}

int test_stdout()
{
    LogFile *log = logfile_create("stdout", LOG_INFO);
    int i;
    
    for (i=0; i<100; i++) {
        DINFO("info %d ...\n", i);
    }
    for (i=0; i<100; i++) {
        DNOTE("info %d ...\n", i);
    }
    for (i=0; i<100; i++) {
        DWARNING("info %d ...\n", i);
    }
    for (i=0; i<100; i++) {
        DERROR("info %d ...\n", i);
    }
    
    logfile_rotate_size(log, 1000000, 10);
    for (i=0; i<100; i++) {
        DINFO("info %d ...\n", i);
    }
    for (i=0; i<100; i++) {
        DNOTE("info %d ...\n", i);
    }
    for (i=0; i<100; i++) {
        DWARNING("info %d ...\n", i);
    }
    for (i=0; i<100; i++) {
        DERROR("info %d ...\n", i);
    }
    
    logfile_rotate_time(log, 1, 10);
    for (i=0; i<100; i++) {
        DINFO("info %d ...\n", i);
    }
    for (i=0; i<100; i++) {
        DNOTE("info %d ...\n", i);
    }
    for (i=0; i<100; i++) {
        DWARNING("info %d ...\n", i);
    }
    for (i=0; i<100; i++) {
        DERROR("info %d ...\n", i);
    }
 
    logfile_rotate_no(log);
    for (i=0; i<100; i++) {
        DINFO("info %d ...\n", i);
    }
    for (i=0; i<100; i++) {
        DNOTE("info %d ...\n", i);
    }
    for (i=0; i<100; i++) {
        DWARNING("info %d ...\n", i);
    }
    for (i=0; i<100; i++) {
        DERROR("info %d ...\n", i);
    }
    
    char *logpath = "error.log";
    if (isfile(logpath)) {
        unlink(logpath);
    }

    logfile_error_separate(log, logpath);
    for (i=0; i<100; i++) {
        DINFO("info %d ...\n", i);
    }
    for (i=0; i<100; i++) {
        DNOTE("info %d ...\n", i);
    }
    for (i=0; i<100; i++) {
        DWARNING("info %d ...\n", i);
    }
    for (i=0; i<100; i++) {
        DERROR("info %d ...\n", i);
    }
    logfile_flush(log);

    int err, warn, note, info;
    err = warn = note = info = 0;

    int lines = testcount(logpath, &err, &warn, &note, &info);
    DINFO("lines:%d\n", lines);
    DASSERT(lines == 200);
    DASSERT(err == 100);
    DASSERT(warn == 100);
    DASSERT(note == 0);
    DASSERT(info == 0);

    logfile_destroy(log);

    return 0;
}

int filecount(char *logdir, char *logname, char *logids)
{
    int logname_len = strlen(logname);
    DIR *mydir;
    struct dirent *node;

    mydir = opendir(logdir);
    if (mydir == NULL) {
        DERROR("dir open error:%s\n", strerror(errno));
        exit(-1);
    }
    int logi = 0;
    while ((node = readdir(mydir)) != NULL) {
        if (strncmp(node->d_name, logname, logname_len) == 0) {
            logids[logi] = atoi(&node->d_name[logname_len]);
            logi++;
        }
    }
    return logi;
}

void remove_files(char *logpath)
{
    int i;
    char checkpath[1024] = {0};

    if (isfile(logpath)) {
        if (unlink(logpath) == -1) {
            DERROR("unlink %s error: %s\n", logpath, strerror(errno));
            exit(-1);
        }
    }

    for (i = 1; i < 21; i++) {
        sprintf(checkpath, "%s.%d", logpath, i);
        if (isfile(checkpath)) {
            if (unlink(checkpath) == -1) {
                DERROR("unlink %s error: %s\n", logpath, strerror(errno));
                exit(-1);
            }
        }
    }

}

int test_file()
{
    char *logpath = "test.log";
    
    remove_files(logpath);
    
    LogFile *log = logfile_create(logpath, LOG_INFO);
    
    int i;
    for (i=0; i<100; i++) {
        DINFO("info.\n"); 
        DNOTE("note.\n");
        DWARNING("warn.\n");
        DERROR("err.\n");
    }
    logfile_flush(log);
    int err, warn, note, info;
    err = warn = note = info = 0;

    int lines = testcount(logpath, &err, &warn, &note, &info);
    DASSERT(lines == 400);
    DASSERT(err  == 100);
    DASSERT(warn == 100);
    DASSERT(note == 100);
    DASSERT(info == 100);
    
    log->loglevel = LOG_NOTE;
    for (i=0; i<100; i++) {
        DINFO("info.\n"); 
        DNOTE("note.\n");
        DWARNING("warn.\n");
        DERROR("err.\n");
    }
    logfile_flush(log);

    lines = testcount(logpath, &err, &warn, &note, &info);
    DASSERT(lines == 700);
    DASSERT(err  == 200);
    DASSERT(warn == 200);
    DASSERT(note == 200);
    DASSERT(info == 100);
    
    log->loglevel = LOG_WARN;
    for (i=0; i<100; i++) {
        DINFO("info.\n"); 
        DNOTE("note.\n");
        DWARNING("warn.\n");
        DERROR("err.\n");
    }
    logfile_flush(log);

    lines = testcount(logpath, &err, &warn, &note, &info);
    DASSERT(lines == 900);
    DASSERT(err  == 300);
    DASSERT(warn == 300);
    DASSERT(note == 200);
    DASSERT(info == 100);
 
    log->loglevel = LOG_ERROR;
    for (i=0; i<100; i++) {
        DINFO("info.\n"); 
        DNOTE("note.\n");
        DWARNING("warn.\n");
        DERROR("err.\n");
    }
    logfile_flush(log);

    lines = testcount(logpath, &err, &warn, &note, &info);
    DASSERT(lines == 1000);
    DASSERT(err  == 400);
    DASSERT(warn == 300);
    DASSERT(note == 200);
    DASSERT(info == 100);
    
    log->loglevel = LOG_INFO;
    log->check_interval = 0;

    logfile_rotate_size(log, 1000, 3);

    char wbuf[1024];

    memset(wbuf, 'a', 1024);
    
    char *logdir = ".";
    char *logname = "test.log.";
    char logids[10] = {0};
    int  logi = 0;
    
    for (i=0; i<100; i++) {
        DINFO("%s\n", wbuf);    
        logfile_flush(log);
        DINFO("gogo%d\n", i);

        logi = filecount(logdir, logname, logids); 
        //DINFO("i:%d, logi:%d\n", i, logi);
        if (i >= 0 && i < 2) {
            DASSERT(logi == i+1);
        }else if (i >= 2) {
            DASSERT(logi == 3);
        }
    }

    char *errpath = "test.err.log";
    char *errname = "test.err.log.";

    remove_files(errpath);

    int  testlog_size = file_size(logpath);

    logfile_error_separate(log, errpath);
    for (i=0; i<100; i++) {
        DERROR("%s\n", wbuf);    
        logfile_flush(log);
        DERROR("gogo%d\n", i);

        logi = filecount(logdir, errname, logids); 
        if (i >= 0 && i < 2) {
            DASSERT(logi == i+1);
        }else if (i >= 2) {
            DASSERT(logi == 3);
        }
        // test.log not changed
        DASSERT(testlog_size == file_size(logpath));
    }

    logfile_destroy(log);
 
    return 0;
}

int test_put()
{
    char *logpath = "test.log";
    
    remove_files(logpath);
    
    LogFile *log = logfile_create(logpath, LOG_INFO);
    int i;
    
    for (i = 0; i < 100; i++) {
        PUT_INFO("tes put %d\n", i);        
    }

    PUT_FLUSH();
 
    logfile_destroy(log);
    return 0;
}

void * _put_log(void *arg)
{
    int i, j; 
    for (j = 0; j < 100; j++) {
        for (i = 0; i < 10; i++) {
            PUT_INFO("put log %d\n", i);
        }
        PUT_FLUSH();
    }

    return NULL;
}

int test_put_thread()
{
    char *logpath = "test.log";
    
    remove_files(logpath);
    
    LogFile *log = logfile_create(logpath, LOG_INFO);
    int threadnum = 10;
    pthread_t threads[threadnum];
    int i;

    for (i = 0; i < threadnum; i++) {
        if (pthread_create(&threads[i], NULL, _put_log, log) != 0) {
            DERROR("pthread create error!\n");
            return -1;
        }
    }
    sleep(1);
    for (i = 0; i < threadnum; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            DERROR("pthread join error!\n");
            return -1;
        }
    }

    logfile_destroy(log);
    return 0;
}

int main()
{
    test_stdout();
    test_file();
    test_put();
    test_put_thread();

    return 0;
}
