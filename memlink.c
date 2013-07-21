/**
 * 入口
 * @file memlink.c
 * @defgroup memlink
 & @author zhaowei
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/resource.h>
#include <unistd.h>
#include "myconfig.h"
#include "logfile.h"
#include "server.h"
#include "daemon.h"
#include "utils.h"
#include "common.h"
#include "runtime.h"
#include "myconfig.h"
#include "sslave.h"

#define MEMLINK_VERSION "memlink-0.5.0"

static void 
sig_handler(const int sig) {
    DERROR("====== SIGNAL %d handled ======\n", sig);
    exit(EXIT_SUCCESS);
}

static void 
sig_handler_segv(const int sig) {
    DERROR("====== SIGSEGV handled ======\n");
    abort();
}

static void 
sig_handler_hup(const int sig) 
{
    DNOTE("====== SIGHUP handled ======\n");
    //int  master_sync_port = 0;
    //int  role;
    int  need_kill = 0;

    FILE *fp;
    int  lineno = 0;
    int  confrole = 0;

    fp = fopen(g_runtime->conffile, "r");
    if (NULL == fp) {
        DERROR("open config file error: %s\n", g_runtime->conffile);
    }
    char buffer[2048];
    while (1) {
        if (fgets(buffer, 2048, fp) == NULL) {
            //DINFO("config file read complete!\n");
            break;
        }
        lineno ++;
        //DINFO("buffer: %s\n", buffer);
        if (buffer[0] == '#' || buffer[0] == '\r'|| buffer[0] =='\n') { // skip comment
            continue;
        }
        char *sp = strchr(buffer, '=');
        if (sp == NULL) {
            DERROR("config file error at line %d: %s\n", lineno, buffer);
            MEMLINK_EXIT;
        }

        char *end = sp - 1;
        while (end > buffer && isblank(*end)) {
            end--;
        }
        *(end + 1) = 0;

        char *start = sp + 1;
        while (isblank(*start)) {
            start++;
        }

        end = start;
        while (*end != 0) {
            if (*end == '\r' || *end == '\n') {
                end -= 1;
                while (end > start && isblank(*end)) {
                    end--;
                }
                *(end + 1) = 0;
                break;
            }
            end += 1;
        }
        if (strcmp(buffer, "block_data_reduce") == 0) {
            g_cf->block_data_reduce = atof(start);
        } else if (strcmp(buffer, "block_clean_cond") == 0) {
            g_cf->block_clean_cond = atof(start);
        } else if (strcmp(buffer, "block_clean_start") == 0) {
            g_cf->block_clean_start = atoi(start);
        } else if (strcmp(buffer, "block_clean_num") == 0) {
            g_cf->block_clean_num = atoi(start);
        } else if (strcmp(buffer, "log_level") == 0) {
            g_cf->log_level = atoi(start);
            //logfile_create(g_cf->log_name, g_cf->log_level);
            g_log->loglevel = g_cf->log_level;
        } else if (strcmp(buffer, "timeout") == 0) {
            g_cf->timeout = atoi(start);
        } else if (strcmp(buffer, "sync_check_interval") == 0) {
            g_cf->sync_check_interval = atoi(start);
        } else if (strcmp(buffer, "role") == 0) {
            if (strcmp(start, "master") == 0) 
                confrole = ROLE_MASTER;
            else if (strcmp(start, "slave") == 0)
                confrole = ROLE_SLAVE;
            else if (strcmp(start, "backup") == 0)
                confrole = ROLE_BACKUP;
        } else if(strcmp(buffer, "master_sync_host") == 0) {
            if (strcmp(g_cf->master_sync_host, start) != 0) {
                snprintf(g_cf->master_sync_host, IP_ADDR_MAX_LEN, "%s", start);
                need_kill = 1;
            }
        } else if (strcmp(buffer, "master_sync_port") == 0) {
            if (g_cf->master_sync_port != atoi(start)) {
                g_cf-> master_sync_port = atoi(start);
                need_kill = 1;
            }
        }
    }
    DINFO("role: %d, need_kill: %d\n", confrole, need_kill);

    if (g_cf->role == ROLE_SLAVE ) { //如果memlink作为从服务器
        if (confrole == ROLE_MASTER) {//需要切换成主,杀掉从线程
            //wait_thread_exit(g_runtime->slave->threadid);
            sslave_stop(g_runtime->slave);
            g_cf->role = ROLE_MASTER;
            DINFO("========slave to master\n");
        } else if (confrole == ROLE_BACKUP) {
            //wait_thread_exit(g_runtime->slave->threadid);
            sslave_stop(g_runtime->slave);
            g_cf->role = ROLE_BACKUP;
            DINFO("========slave to backup\n");
        } else if ((confrole == ROLE_SLAVE) && need_kill == 1) {
            //不需要切换，查看master_sync_host和master_sync_port是否改变
            //从新启动从线程
            
            //wait_thread_exit(g_runtime->slave->threadid);
            /*if (g_runtime->slave) {
                sslave_thread(g_runtime->slave);
            } else {
                g_runtime->slave = sslave_create();
            }*/
            sslave_go(g_runtime->slave);
            DINFO("=========restart slave thread\n");
        }
    } else if ((g_cf->role == ROLE_MASTER || g_cf->role == ROLE_BACKUP) && confrole == ROLE_SLAVE) {
        //主切换成从， 只需启动从线程
        /*if (g_runtime->slave) {
            sslave_thread(g_runtime->slave);
        } else {
            g_runtime->slave = sslave_create();
        }*/
        sslave_go(g_runtime->slave);
        g_cf->role = ROLE_SLAVE;
        DINFO("===========master to slave\n");
    }
    fclose(fp);
}

int 
signal_install()
{
    struct sigaction sigact;

    sigact.sa_handler = sig_handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;

    sigaction(SIGINT, &sigact, NULL);
    
    sigact.sa_handler = sig_handler_segv;
    sigaction(SIGSEGV, &sigact, NULL);

    sigact.sa_handler = sig_handler_hup;
    sigaction(SIGHUP, &sigact, NULL);

    sigact.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sigact, NULL);

    return 0;
}

void 
master(char *pgname, char *conffile) 
{
    runtime_create_master(pgname, conffile);
    DINFO("master runtime ok!\n");

    mainserver_loop(g_runtime->server);
}

void 
slave(char *pgname, char *conffile) 
{
    runtime_create_slave(pgname, conffile);
    DINFO("slave runtime ok!\n");

    mainserver_loop(g_runtime->server);
}

void 
usage()
{
    fprintf(stderr, "%s\nusage: memlink [config file path]\n", MEMLINK_VERSION);
}

int main(int argc, char *argv[])
{
    int ret;
    struct rlimit rlim;
    char *conffile;

    if (argc == 2) {
        conffile = argv[1];
    }else{
        //conffile = "etc/memlink.conf";
        usage();
        return 0;
    }

    //DINFO("%s\n", MEMLINK_VERSION);
    //DINFO("config file: %s\n", conffile);
    myconfig_create(conffile);
    DNOTE("====== %s ======\n", MEMLINK_VERSION);
    //DNOTE("config file: %s\n", conffile);
    //DNOTE("data dir: %s\n", g_cf->datadir);
    
    if (g_cf->max_core) {
        struct rlimit rlim_new;
        if (getrlimit(RLIMIT_CORE, &rlim) == 0) {
            rlim_new.rlim_cur = rlim_new.rlim_max = RLIM_INFINITY;
            if (setrlimit(RLIMIT_CORE, &rlim_new)!= 0) {
                /* failed. try raising just to the old max */
                rlim_new.rlim_cur = rlim_new.rlim_max = rlim.rlim_max;
                (void)setrlimit(RLIMIT_CORE, &rlim_new);
            }
        }
        if ((getrlimit(RLIMIT_CORE, &rlim) != 0) || rlim.rlim_cur == 0) {
            DERROR("failed to ensure corefile creation\n");
            MEMLINK_EXIT;
        }
    }

    
    if (getrlimit(RLIMIT_NOFILE, &rlim) != 0) { 
        DERROR("failed to getrlimit number of files\n");
        MEMLINK_EXIT;
    } else {
        int maxfiles = g_cf->max_conn + 20;
        if (rlim.rlim_cur < maxfiles)
            rlim.rlim_cur = maxfiles;
        if (rlim.rlim_max < rlim.rlim_cur)
            rlim.rlim_max = rlim.rlim_cur;
        if (setrlimit(RLIMIT_NOFILE, &rlim) != 0) { 
            DERROR("failed to set rlimit for open files. Try running as root or requesting smaller maxconns value.\n");
            MEMLINK_EXIT;
        }    
    }
    
    if (change_group_user(g_cf->user) < 0) { 
        DERROR("change group user error!\n");
        MEMLINK_EXIT;
    }
    signal_install();

    if (g_cf->is_daemon) {
        ret = daemonize(g_cf->max_core, 0);
        if (ret == -1) {
            char errbuf[1024];
            strerror_r(errno, errbuf, 1024);
            DERROR("daemon error! %s\n",  errbuf);
            MEMLINK_EXIT;
        }
    }
    
    logfile_create(g_cf->log_name, g_cf->log_level);
    if (g_cf->log_error_name[0] != 0) {
        logfile_error_separate(g_log, g_cf->log_error_name);
    }
    if (g_cf->log_rotate_type == LOG_ROTATE_SIZE) {
        logfile_rotate_size(g_log, g_cf->log_size, g_cf->log_count);
    }else if (g_cf->log_rotate_type == LOG_ROTATE_TIME){
        logfile_rotate_time(g_log, g_cf->log_time, g_cf->log_count);
    }
    DINFO("logfile ok!\n");
    myconfig_print(g_cf);

    if (g_cf->sync_mode == MODE_MASTER_BACKUP) {
        master(argv[0], conffile);
    } else if (g_cf->sync_mode == MODE_MASTER_SLAVE) {
        if (g_cf->role == ROLE_MASTER) {
            master(argv[0], conffile);
        }else{
            slave(argv[0], conffile);
        }
    } else {
        DERROR("sync_mode is error\n");
        MEMLINK_EXIT;
    }

    return 0;
}


