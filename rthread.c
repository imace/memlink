/** 
 * 读操作线程
 * @file rthread.c
 * @author zhaowei
 * @ingroup memlink
 * @{
 */
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "logfile.h"
#include "rthread.h"
#include "hashtable.h"
#include "serial.h"
#include "myconfig.h"
#include "wthread.h"
#include "utils.h"
#include "common.h"
#include "info.h"
#include "runtime.h"

/**
 * Execute the read command and send response.
 *
 * @param conn connection
 * @param data command data
 * @param datalen the length of data parameter
 */
int
rdata_ready(Conn *conn, char *data, int datalen)
{
    //char keybuf[512] = {0}; 
    char tbname[256] = {0};
    char key[256] = {0}; 

    //char value[512] = {0};
    char cmd;
    int  ret = 0;
    //char *msg = NULL;
    char *retdata = NULL;
    int  retlen = 0;
    //uint8_t   valuelen;
    uint8_t   attrnum;
    uint32_t  attrarray[HASHTABLE_ATTR_MAX_ITEM] = {0};
    int    frompos, len;
    struct timeval start, end;
    ThreadServer *st = (ThreadServer *)conn->thread;
    RwConnInfo *conninfo = NULL; 

    int i;
    for (i = 0; i < g_cf->max_conn; i++) {
        conninfo = &(st->rw_conn_info[i]);
        if (conninfo->fd == conn->sock)
            break;
    }
    if (conninfo) conninfo->cmd_count++;

    gettimeofday(&start, NULL);
    memcpy(&cmd, data + sizeof(int), sizeof(char));
    char buf[256] = {0};
    DINFO("data ready cmd: %d, data: %s\n", cmd, formath(data, datalen, buf, 256));

    switch(cmd) {
        case CMD_PING: {
            ret = MEMLINK_OK;
            goto rdata_ready_error;
            break;
        }
        case CMD_RANGE: {
            DINFO("<<< cmd RANGE >>>\n");
            uint8_t kind;
            ret = cmd_range_unpack(data, tbname, key, &kind, &attrnum, attrarray, &frompos, &len);
            DINFO("unpack range return:%d, key:%s, attrnum:%d, frompos:%d, len:%d\n", 
                    ret, key, attrnum, frompos, len);

            if (frompos < 0 || len <= 0) {
                DERROR("from or len small than 0. from:%d, len:%d\n", frompos, len);
                ret = MEMLINK_ERR_RANGE_SIZE;
                goto rdata_ready_error;
            }

            ret = check_table_key(tbname, key);
            if (ret < 0) {
                goto rdata_ready_error;
            }
            
            ret = hashtable_range(g_runtime->ht, tbname, key, kind, attrarray, attrnum, 
                                frompos, len, conn); 
            DINFO("table_range return: %d\n", ret);
            ret = conn_send_buffer(conn);
            DINFO("send return: %d\n", ret);

            break;
        }
        case CMD_SL_RANGE: {
            DINFO("<<< cmd SL_RANGE >>>\n");
            uint8_t kind;
            char valmin[512] = {0};
            char valmax[512] = {0};
            uint8_t vminlen = 0, vmaxlen = 0;

            ret = cmd_sortlist_range_unpack(data, tbname, key, &kind, &attrnum, attrarray, 
                                            valmin, &vminlen, valmax, &vmaxlen);
            DINFO("unpack range return:%d, key:%s, attrnum:%d, vmin:%s,%d, vmax:%s,%d, len:%d\n", 
                            ret, key, attrnum, valmin, vminlen, valmax, vmaxlen, len);

            ret = check_table_key(tbname, key);
            if (ret < 0) {
                goto rdata_ready_error;
            }

            ret = hashtable_sortlist_range(g_runtime->ht, tbname, key, kind, 
                                attrarray, attrnum, valmin, valmax, conn); 
            DINFO("table_range return: %d\n", ret);
            ret = conn_send_buffer(conn);
            DINFO("send return: %d\n", ret);
            break;
        }
        case CMD_STAT: {
            DINFO("<<< cmd STAT >>>\n");
            HashTableStat   stat;
            ret = cmd_stat_unpack(data, tbname, key);
            DINFO("unpack stat return: %d, key: %s\n", ret, key);

            ret = check_table_key(tbname, key);
            if (ret < 0) {
                goto rdata_ready_error;
            }

            ret = hashtable_stat(g_runtime->ht, tbname, key, &stat);
            DINFO("table stat: %d\n", ret);
            DINFO("stat blocks: %d\n", stat.blocks);
    
            //hashtable_print(g_runtime->ht, key); 
            retdata = (char*)&stat;
            retlen  = sizeof(HashTableStat);

            ret = conn_send_buffer_reply(conn, ret, retdata, retlen);
            DINFO("send return: %d\n", ret);
            break;
        }
        case CMD_STAT_SYS: {
            DINFO("<<< cmd STAT_SYS >>>\n");
            MemLinkStatSys stat;

            ret = info_sys_stat(&stat);
            DINFO("table stat sys: %d\n", ret);
    
            retdata = (char*)&stat;
            retlen  = sizeof(HashTableStatSys);

            ret = conn_send_buffer_reply(conn, ret, retdata, retlen);
            DINFO("send return: %d\n", ret);
            break;
        }
        case CMD_COUNT: {
            DINFO("<<< cmd COUNT >>>\n");
            uint8_t attrnum;

            ret = cmd_count_unpack(data, tbname, key, &attrnum, attrarray);
            DINFO("unpack count return: %d, key: %s, attr:%d:%d:%d\n", 
                        ret, key, attrarray[0], attrarray[1], attrarray[2]);
        
            ret = check_table_key(tbname, key);
            if (ret < 0) {
                goto rdata_ready_error;
            }

            int vcount = 0, mcount = 0;
            retlen = sizeof(int) * 2;
            char retrec[retlen];

            ret = hashtable_count(g_runtime->ht, tbname, key, attrarray, attrnum, &vcount, &mcount);
            DINFO("table count, ret:%d, visible_count:%d, tagdel_count:%d\n", ret, vcount, mcount);
            memcpy(retrec, &vcount, sizeof(int));
            memcpy(retrec + sizeof(int), &mcount, sizeof(int));
            //retlen = sizeof(int) + sizeof(int);

            ret = conn_send_buffer_reply(conn, ret, retrec, retlen);
            DINFO("send return: %d\n", ret);
            break;
        }
        case CMD_SL_COUNT: {
            DINFO("<<< cmd SL_COUNT >>>\n");
            char valmin[512] = {0};
            char valmax[512] = {0};
            uint8_t vminlen = 0, vmaxlen = 0;

            ret = cmd_sortlist_count_unpack(data, tbname, key, &attrnum, attrarray, 
                                            valmin, &vminlen, valmax, &vmaxlen);
            DINFO("unpack range return:%d, key:%s, attrnum:%d, vmin:%s,%d, vmax:%s,%d, len:%d\n", 
                            ret, key, attrnum, valmin, vminlen, valmax, vmaxlen, len);

            ret = check_table_key(tbname, key);
            if (ret < 0) {
                goto rdata_ready_error;
            }

            int vcount = 0, mcount = 0;
            retlen = sizeof(int) * 2;
            char retrec[retlen];

            ret = hashtable_sortlist_count(g_runtime->ht, tbname, key, attrarray, attrnum, 
                                       valmin, valmax, &vcount, &mcount); 
            DINFO("table sortlist count, ret:%d, visible_count:%d, tagdel_count:%d\n", ret, vcount, mcount);
            memcpy(retrec, &vcount, sizeof(int));
            memcpy(retrec + sizeof(int), &mcount, sizeof(int));
            //retlen = sizeof(int) + sizeof(int);
            ret = conn_send_buffer_reply(conn, ret, retrec, retlen);
            DINFO("send return: %d\n", ret);
            break;
        }
        case CMD_TABLES: {
            DINFO("<<< cmd TABLES >>>\n");
            retlen = hashtable_tables(g_runtime->ht, &retdata);
            ret = conn_send_buffer_reply(conn, ret, retdata, retlen);
            break;
        }
        case CMD_READ_CONN_INFO: {
            DINFO("<<< cmd READ_CONN_INFO >>>\n");
            ret = info_read_conn(conn);
            ret = conn_send_buffer(conn);
            DINFO("send return: %d\n", ret);
            break;

        }
        case CMD_WRITE_CONN_INFO: {
            DINFO("<<< cmd WRITE_CONN_INFO >>>\n");
            ret = info_write_conn(conn);
            DINFO("write_conn_info return: %d\n", ret);
            ret = conn_send_buffer(conn);
            DINFO("send return: %d\n", ret);
            break;
        }
        case CMD_SYNC_CONN_INFO: {
            DINFO("<<< cmd SYNC_CONN_INFO >>>\n");
            ret = info_sync_conn(conn);
            DINFO("sync_conn_info return: %d\n", ret);
            ret = conn_send_buffer(conn);
            DINFO("send return: %d\n", ret);
            break;
        }
        case CMD_CONFIG_INFO: {
            DINFO("<<< cmd CMD_CONFIG_INFO >>>\n");
            ret = info_sys_config(conn);
            ret = conn_send_buffer(conn);
            DINFO("send return: %d\n", ret);
            break;
        }
        default: {
            ret = MEMLINK_ERR_CLIENT_CMD;
            ret = conn_send_buffer_reply(conn, ret, retdata, retlen);
            DINFO("send return: %d\n", ret);
        }
    }

    gettimeofday(&end, NULL);
    DNOTE("%s:%d cmd:%d use %u us\n", conn->client_ip, conn->client_port, cmd, timediff(&start, &end));

    return 0;

rdata_ready_error:
    ret = conn_send_buffer_reply(conn, ret, NULL, 0);
    gettimeofday(&end, NULL);
    DNOTE("%s:%d cmd:%d use %u us\n", conn->client_ip, conn->client_port, cmd, timediff(&start, &end));
    DINFO("send return: %d\n", ret);

    return 0;
}
/**
 * @}
 */
