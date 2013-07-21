/**
 * 写操作线程
 * @file wthread.c
 * @author zhaowei
 * @ingroup memlink
 * @{
 */
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef __linux
#include <linux/if.h>
#endif
#include <sys/un.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/time.h>
#include <netdb.h>
#include <signal.h>
#include "wthread.h"
#include "serial.h"
#include "network.h"
#include "myconfig.h"
#include "base/logfile.h"
#include "dumpfile.h"
#include "rthread.h"
#include "sthread.h"
#include "base/zzmalloc.h"
#include "base/utils.h"
#include "base/pack.h"
#include "common.h"
#include "sslave.h"
#include "runtime.h"
#include "vote.h"
#include "backup.h"
#include "master.h"

/**
 * 回复数据
 *
 * Send respose to client.
 *
 * @param conn
 * @param retcode return code
 * @param return return data length
 * @param data return data
 *
 * ------------------------------------
 * | length (4B)| retcode (2B) | data |
 * ------------------------------------
 * Length is the count of bytes following it.
 */

static int
is_clean_cond(HashNode *node)
{
    DINFO("check clean cond, used:%d, all:%d, blocks:\n", node->used, node->all);
    // not do clean, when blocks is small than block_clean_start
    int blockcount = 0;
    DataBlock *block = node->data;
    while (block) {
        blockcount++;
        block = block->next;
        if (blockcount >= g_cf->block_clean_start) {
            break;
        }
    }
    if (blockcount < g_cf->block_clean_start) {
        //DNOTE("no clean, blockcount:%d smaller than %d\n", blockcount, g_cf->block_clean_start);
        return 0;
    }

    if (node->all == 0) {
        //DNOTE("no clean, no value.\n");
        return 0;
    }

    if (g_runtime->inclean) {
        //DNOTE("no clean, other inclean ...\n");
        return 0;
    }

    double rate = 1.0 - (double)node->used / node->all;
    DINFO("check clean rate: %f\n", rate);

    if (g_cf->block_clean_cond > rate) {
        //DNOTE("no clean, rate:%f smaller than %f.\n", rate, g_cf->block_clean_cond);
        return 0;
    }
    
    //DNOTE("try clean, blockcount:%d, rate:%f\n", blockcount, rate);
    return 1;
}

/*
void*
wdata_do_clean(void *args)
{
    HashNode    *node = (HashNode*)args;
    int            ret;
    struct timeval start, end;

    g_runtime->inclean = TRUE;
    pthread_mutex_lock(&g_runtime->mutex);
    //snprintf(g_runtime->cleankey, 512, "%s", node->key);
    
    if (is_clean_cond(node) == 0) {
        //DNOTE("thread check not need clean %s\n", node->key);
        goto wdata_do_clean_over;
    }

    DNOTE("start clean %s ...\n", node->key);
    gettimeofday(&start, NULL);
    ret = hashtable_clean(g_runtime->ht, node->key);
    if (ret != 0) {
        DERROR("wdata_do_clean error: %d\n", ret);
    }
    gettimeofday(&end, NULL);
    DNOTE("clean %s complete, use %u us\n", node->key, timediff(&start, &end));
    //g_runtime->cleankey[0] = 0;

wdata_do_clean_over:
    g_runtime->inclean = FALSE;
    pthread_mutex_unlock(&g_runtime->mutex);

    return NULL;
}
*/
void
wdata_check_clean(char *tbname, char *key)
{
    HashNode    *node;
    Table       *tb; 

    if (tbname == NULL || tbname[0] == 0 || key == NULL || key[0] == 0) 
        return;
    
    // not need 
    if (g_cf->block_clean_cond < 0.001) {
        return;
    }
    tb = hashtable_find_table(g_runtime->ht, tbname);
    if (NULL == tb)
        return;

    node = table_find(tb, key);
    if (NULL == node)
        return;

    if (is_clean_cond(node) == 0) {
        return;
    }
    
    
    /*
    pthread_t       threadid;
    pthread_attr_t  attr;
    int             ret;
    
    DNOTE("create clean thread ..\n");
    ret = pthread_attr_init(&attr);
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("pthread_attr_init error: %s\n",  errbuf);
        MEMLINK_EXIT;
    }

    ret = pthread_create(&threadid, &attr, wdata_do_clean, node);
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("pthread_create error: %s\n",  errbuf);
        MEMLINK_EXIT;
    }

    ret = pthread_detach(threadid);
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("pthread_detach error; %s\n",  errbuf);
        MEMLINK_EXIT;
    }
    */
}

/**
 * Execute the write command.
 *
 * @param data command data
 * @param datalen command data length
 * @param writelog non-zero to write sync log; otherwise, not to write sync 
 * log.
 */
int 
wdata_apply(char *data, int datalen, int writelog, Conn *conn)
{
    //char keybuf[512] = {0}; 
    char tbname[256] = {0};
    char key[256] = {0};
    char value[512] = {0};
    char cmd;
    int  ret = 0;
    uint8_t   valuelen;
    uint8_t   attrnum;
    uint32_t  attrformat[HASHTABLE_ATTR_MAX_ITEM];
    uint32_t  attrarray[HASHTABLE_ATTR_MAX_ITEM];
    uint8_t   tag;
    uint8_t   listtype, valuetype;
    int       pos;
    int       vnum;
    int       num = 0;
    int       i;
    WThread     *wt;
    RwConnInfo  *conninfo = NULL;
    
    if (conn) {    
        wt = (WThread *)conn->thread;
        for (i = 0; i < g_cf->max_write_conn; i++) {
            conninfo = &(wt->rw_conn_info[i]);
            if (conninfo->fd == conn->sock)
                break;
        }
    }
    if (conn && conninfo) conninfo->cmd_count++;

    memcpy(&cmd, data + sizeof(int), sizeof(char));
    char buf[256] = {0};
    DINFO("data ready cmd: %d, data: %s\n", cmd, formath(data, datalen, buf, 256));

    switch(cmd) {
        case CMD_DUMP:
            DINFO("<<< cmd DUMP >>>\n");
            ret = dumpfile(g_runtime->ht);
            goto wdata_apply_over;
            break;
        case CMD_CLEAN: {
            DINFO("<<< cmd CLEAN >>>\n");
            ret = cmd_clean_unpack(data, tbname, key);
            if (ret <= 0) {
                DINFO("unpack clean error! ret: %d, key:%s\n", ret, key);
                goto wdata_apply_over;
            }

            ret = check_table_key(tbname, key);
            if (ret < 0) {
                goto wdata_apply_over;
            }  

            //hashtable_print(g_runtime->ht, key);
            DINFO("clean unpack key: %s\n", key);
            ret = hashtable_clean(g_runtime->ht, tbname, key); 
            DINFO("clean return:%d\n", ret); 
            //hashtable_print(g_runtime->ht, key);
            goto wdata_apply_over;
            break;
        }
        case CMD_CLEAN_ALL:
            DINFO("<<< cmd CLEAN ALL >>>\n");

            ret = hashtable_clean_all(g_runtime->ht);
            DINFO("clean all return: %d\n", ret);
            goto wdata_apply_over;
            break;
        case CMD_CREATE_TABLE:
            DINFO("<<< cmd CREATE TABLE >>>\n");
            ret = cmd_create_table_unpack(data, tbname, &valuelen, &attrnum, attrformat, 
                                         &listtype, &valuetype);
            if (ret <= 0) {
                DINFO("unpack create error! ret: %d\n", ret);
                goto wdata_apply_over;
            }
            DINFO("unpack key:%s, valuelen:%d, listtype:%d attrnum:%d, attrarray:%d,%d,%d\n", 
                    tbname, valuelen, listtype, attrnum, attrformat[0], attrformat[1], attrformat[2]);

            if (tbname[0] == 0 || valuelen <= 0 || listtype <= 0 || listtype > MEMLINK_SORTLIST) {
                DINFO("param error!\n");
                ret = MEMLINK_ERR_PARAM;
                goto wdata_apply_over;
            }
            
            if (attrnum > HASHTABLE_ATTR_MAX_ITEM) {
                DINFO("create attr too long: %d, max:%d\n", attrnum, HASHTABLE_ATTR_MAX_ITEM);
                ret = MEMLINK_ERR_ATTR;
                goto wdata_apply_over;
            }
            vnum = valuelen;

            ret = hashtable_create_table(g_runtime->ht, tbname, vnum, attrformat, attrnum, 
                                        listtype, valuetype);
            DINFO("hashtable_create_table return: %d\n", ret);
            break; 
        case CMD_RMTABLE:
            DINFO("<<< cmd RMTABLE >>>\n");
            ret = cmd_rmtable_unpack(data, tbname);
            if (ret <= 0) {
                DINFO("unpack tag error! ret: %d, table:%s\n", ret, tbname);
                goto wdata_apply_over;
            }
            if (tbname[0] == 0) {
                ret = MEMLINK_ERR_PARAM;
                goto wdata_apply_over;
            }
            
            pthread_mutex_lock(&g_runtime->wthread->rmlock);
            ret = hashtable_remove_table(g_runtime->ht, tbname);
            pthread_mutex_unlock(&g_runtime->wthread->rmlock);
            DINFO("hashtable_remove_table ret: %d\n", ret);
            break;

        case CMD_CREATE_NODE: {
            DINFO("<<< cmd CREATE_NODE >>>\n");
            ret = cmd_create_node_unpack(data, tbname, key);
            if (ret <= 0) {
                DINFO("unpack create node error! ret:%d\n", ret);
                goto wdata_apply_over;
            }
            if (tbname[0] == 0 || key[0] == 0) {
                ret = MEMLINK_ERR_PARAM;
                goto wdata_apply_over;
            }
            ret = hashtable_create_node(g_runtime->ht, tbname, key);
            DINFO("hashtable_create_node ret:%d\n", ret);
            break;
        }
        case CMD_DEL: {
            DINFO("<<< cmd DEL >>>\n");
            ret = cmd_del_unpack(data, tbname, key, value, &valuelen);
            if (ret <= 0) {
                DINFO("unpack del error! ret: %d\n", ret);
                goto wdata_apply_over;
            }

            DINFO("unpack del, key: %s, value: %s, valuelen: %d\n", key, value, valuelen);
            
            ret = check_table_key(tbname, key);
            if (ret < 0) {
                goto wdata_apply_over;
            }  

            ret = hashtable_del(g_runtime->ht, tbname, key, value);
            DINFO("hashtable_del: %d\n", ret);
            break;
        }
        case CMD_SL_DEL: {
            DINFO("<<< cmd SL_DEL >>>\n");
            char valmin[512] = {0};
            char valmax[512] = {0};
            uint8_t kind;
            uint8_t vminlen = 0, vmaxlen = 0;

            ret = cmd_sortlist_del_unpack(data, tbname, key, &kind, valmin, &vminlen, 
                                valmax, &vmaxlen, &attrnum, attrarray);
            if (ret <= 0) {
                DINFO("unpack sortlist_del error! ret: %d\n", ret);
                goto wdata_apply_over;
            }

            DINFO("unpack del, key: %s, valmin:%s, valmax:%s\n", key, valmin, valmax);
            ret = check_table_key(tbname, key);
            if (ret < 0) {
                goto wdata_apply_over;
            }  

            ret = hashtable_sortlist_mdel(g_runtime->ht, tbname, key, kind, 
                                valmin, valmax, attrarray, attrnum);
            DINFO("hashtable_sortlist_del: %d\n", ret);

            break;
        }
        case CMD_INSERT: {
            DINFO("<<< cmd INSERT >>>\n");
            ret = cmd_insert_unpack(data, tbname, key, value, &valuelen, &attrnum, attrarray, &pos);
            if (ret <= 0) {
                DINFO("unpack insert error! ret: %d\n", ret);
                goto wdata_apply_over;
            }

            //DINFO("unpack key:%s, value:%d, pos:%d, attr: %d, array:%d,%d,%d\n", 
            //    key, *(int*)value, pos, attrnum, attrarray[0], attrarray[1], attrarray[2]);
            if (pos == -1) {
                pos = INT_MAX;
            } else if (pos < 0) {
                ret = MEMLINK_ERR_PARAM;
                DINFO("insert pos < 0, %d", pos);
                goto wdata_apply_over;
            }

            ret = check_table_key(tbname, key);
            if (ret < 0) {
                goto wdata_apply_over;
            }  

            ret = hashtable_insert(g_runtime->ht, tbname, key, value, attrarray, attrnum, pos);
            DINFO("hashtable_insert: %d\n", ret);

            //hashtable_print(g_runtime->ht, key);
            break;
        }
        case CMD_MOVE: {
            DINFO("<<< cmd MOVE >>>\n");
            ret = cmd_move_unpack(data, tbname, key, value, &valuelen, &pos);
            if (ret <= 0) {
                DINFO("unpack move error! ret: %d\n", ret);
                goto wdata_apply_over;
            }
            DINFO("unpack move, table:%s key:%s, value:%s, pos:%d\n", tbname, key, value, pos);
    
            if (pos == -1) {
                pos = INT_MAX;
            } else if (pos < 0) {
                ret = MEMLINK_ERR_PARAM;
                DINFO("move pos < 0, %d", pos);
                goto wdata_apply_over;
            }
            ret = check_table_key(tbname, key);
            if (ret < 0) {
                goto wdata_apply_over;
            }  

            ret = hashtable_move(g_runtime->ht, tbname, key, value, pos);
            DINFO("table_move: %d\n", ret);
            break; 
        }
        case CMD_ATTR: {
            DINFO("<<< cmd ATTR >>>\n");
            ret = cmd_attr_unpack(data, tbname, key, value, &valuelen, &attrnum, attrarray);
            if (ret <= 0) {
                DINFO("unpack attr error! ret: %d\n", ret);
                goto wdata_apply_over;
            }

            DINFO("unpack attr table:%s key: %s, valuelen: %d, attrnum: %d, attrarray: %d,%d,%d\n", 
                    tbname, key, valuelen, 
                    attrnum, attrarray[0], attrarray[1], attrarray[2]);
            ret = check_table_key(tbname, key);
            if (ret < 0) {
                goto wdata_apply_over;
            }  

            ret = hashtable_attr(g_runtime->ht, tbname, key, value, attrarray, attrnum);
            DINFO("table_attr: %d\n", ret);
            break; 
        }
        case CMD_TAG: {
            DINFO("<<< cmd TAG >>>\n");
            ret = cmd_tag_unpack(data, tbname, key, value, &valuelen, &tag);
            if (ret <= 0) {
                DINFO("unpack tag error! ret: %d\n", ret);
                /*char ebuf[256];
                DINFO("unpack: %s\n", formath(data, datalen, ebuf, 256));*/
                goto wdata_apply_over;
            }

            DINFO("unpack tag, table:%s key:%s, value:%s, tag:%d\n", tbname, key, value, tag);
            ret = check_table_key(tbname, key);
            if (ret < 0) {
                goto wdata_apply_over;
            }  

            ret = hashtable_tag(g_runtime->ht, tbname, key, value, tag);
            DINFO("hashtable_tag: %d\n", ret);
            break;
        }
        case CMD_RMKEY:
            DINFO("<<< cmd RMKEY >>>\n");
            ret = cmd_rmkey_unpack(data, tbname, key);
            if (ret <= 0) {
                DINFO("unpack tag error! ret: %d\n", ret);
                goto wdata_apply_over;
            }

            DINFO("unpack rmkey, table:%s key:%s\n", tbname, key);
            ret = check_table_key(tbname, key);
            if (ret < 0) {
                goto wdata_apply_over;
            }  

            ret = hashtable_remove_key(g_runtime->ht, tbname, key);
            DINFO("hashtable_remove_key ret: %d\n", ret);
            break;
        case CMD_DEL_BY_ATTR:
            DINFO("<<< cmd DEL_BY_ATTR >>>\n");
            ret = cmd_del_by_attr_unpack(data, tbname, key, attrarray, &attrnum);
            if (ret <= 0) {
                DINFO("unpack del_by_attr error! ret: %d\n", ret);
                goto wdata_apply_over;
            }
            DINFO("unpack table:%s key: %s, attrnum: %d, attrarray: %d,%d,%d\n", 
                    tbname, key, attrnum, attrarray[0], attrarray[1],attrarray[2]);
            ret = check_table_key(tbname, key);
            if (ret < 0) {
                goto wdata_apply_over;
            }  

            ret = hashtable_del_by_attr(g_runtime->ht, tbname, key, attrarray, attrnum);
            DINFO("hashtable_del_by_attr ret: %d\n", ret);
            if (conn && ret >= 0 && writelog) {
                ret = conn_send_buffer_reply(conn, ret, NULL, 0);
            }
            DINFO("conn_send_buffer_reply return: %d\n", ret);
            if (ret >= 0) {
                if (conn) {
                    ret = MEMLINK_REPLIED;
                }else{
                    ret = MEMLINK_OK;
                }
            }

            break;
        case CMD_LPUSH:
            DINFO("<<< cmd LPUSH >>>\n");
            ret = cmd_lpush_unpack(data, tbname, key, value, &valuelen, &attrnum, attrarray);
            if (ret <= 0) {
                DINFO("unpack lpush error! ret: %d\n", ret);
                goto wdata_apply_over;
            }
            DINFO("unpack table:%s key: %s, attrnum: %d, attrarray: %d,%d,%d\n", 
                    tbname, key, attrnum, attrarray[0], attrarray[1],attrarray[2]);
            ret = check_table_key(tbname, key);
            if (ret < 0) {
                goto wdata_apply_over;
            }  

            ret = hashtable_lpush(g_runtime->ht, tbname, key, value, attrarray, attrnum);
            DINFO("hashtable_lpush ret: %d\n", ret);
            break;
        case CMD_RPUSH:
            DINFO("<<< cmd RPUSH >>>\n");
            ret = cmd_rpush_unpack(data, tbname, key, value, &valuelen, &attrnum, attrarray);
            if (ret <= 0) {
                DINFO("unpack rpush error! ret: %d\n", ret);
                goto wdata_apply_over;
            }
            DINFO("unpack table:%s key: %s, attrnum: %d, attrarray: %d,%d,%d\n", 
                    tbname, key, attrnum, attrarray[0], attrarray[1],attrarray[2]);
            ret = check_table_key(tbname, key);
            if (ret < 0) {
                goto wdata_apply_over;
            }  

            ret = hashtable_rpush(g_runtime->ht, tbname, key, value, attrarray, attrnum);
            DINFO("hashtable_rpush ret: %d\n", ret);
            break;
        case CMD_LPOP:
            DINFO("<<< cmd LPOP >>>\n");
            ret = cmd_pop_unpack(data, tbname, key, &num);
            if (ret <= 0) {
                DINFO("unpack rpush error! ret: %d\n", ret);
                goto wdata_apply_over;
            }
            ret = check_table_key(tbname, key);
            if (ret < 0) {
                goto wdata_apply_over;
            }  

            if (num <= 0) {
                DINFO("num:%d, key:%s\n", num, key);
                ret = MEMLINK_ERR_PARAM;
                goto wdata_apply_over;
            }   

            //hashtable_print(g_runtime->ht, key);
            ret = hashtable_lpop(g_runtime->ht, tbname, key, num, conn); 
            DINFO("hashtable_lpop return: %d\n", ret);
            //hashtable_print(g_runtime->ht, key);

            if (conn && ret >= 0 && writelog) {
                ret = conn_send_buffer(conn);
            }
            DINFO("conn_send_buffer_reply return: %d\n", ret);
            if (ret >= 0) {
                if (conn) {
                    ret = MEMLINK_REPLIED;
                }else{
                    ret = MEMLINK_OK;
                }
            }
            break;
        case CMD_RPOP:
            DINFO("<<< cmd RPOP >>>\n");
            ret = cmd_pop_unpack(data, tbname, key, &num);
            if (ret <= 0) {
                DINFO("unpack rpush error! ret: %d\n", ret);
                goto wdata_apply_over;
            }
            ret = check_table_key(tbname, key);
            if (ret < 0) {
                goto wdata_apply_over;
            }  

            if (num <= 0) {
                DINFO("num:%d, key:%s\n", num, key);
                ret = MEMLINK_ERR_PARAM;
                goto wdata_apply_over;
            }   

            //hashtable_print(g_runtime->ht, key);
            ret = hashtable_rpop(g_runtime->ht, tbname, key, num, conn); 
            DINFO("hashtable_range return: %d\n", ret);
            //hashtable_print(g_runtime->ht, key);

            if (conn && ret >= 0 && writelog) {
                ret = conn_send_buffer(conn);
            }
            DINFO("conn_send_buffer_reply return: %d\n", ret);
            //ret = MEMLINK_REPLIED;
            if (ret >= 0) {
                if (conn) {
                    ret = MEMLINK_REPLIED;
                }else{
                    ret = MEMLINK_OK;
                }
            }
            break;
        case CMD_INSERT_MKV:
            {
                uint32_t package_len = 0, valcount = 0;
                uint32_t count = 0;//统计解析了多少字节的包    
                uint32_t psize;
                uint32_t keysize = 0;
                uint8_t skip = 1;
                char retdata[128];
                
                DINFO("<<< cmd INSERT MKV >>>\n");
                //解析包的大小
                psize = cmd_insert_mkv_unpack_packagelen(data, &package_len);
                DINFO("package_len: %d\n", package_len);
                count += psize;
                //skip cmd
                count += sizeof(char);
                int i = 0;
                int j = 0;
                while (count < package_len && skip == 1) {
                    char *countstart;
                    //解析key    
                    memset(key, 0, sizeof(key));    
                    memset(tbname, 0, sizeof(tbname));
                    psize = cmd_insert_mkv_unpack_key(data + count, tbname, key, &valcount, &countstart);
                    keysize = psize;
                    DINFO("key: %s, valcount: %d\n", key, valcount);
                    count += psize;    

                    ret = check_table_key(tbname, key);
                    if (ret < 0) {
                        //当前key的第一个value就出错了
                        count -= keysize;
                        break;
                    }  
                    //解析value
                    j = 0;
                    while (j < valcount) {
                        memset(value, 0x0, 512);
                        psize = cmd_insert_mkv_unpack_val(data + count, value, &valuelen, &attrnum, attrarray, &pos);
                        DINFO("i: %d, value: %s, valuelen: %d, attrnum: %d, pos: %d\n", i, value, valuelen, attrnum, pos);
                        if (pos == -1) {
                            pos = INT_MAX;
                        } else if (pos < 0) {
                            DINFO("insert pos < 0, %d\n", pos);
                            ret = MEMLINK_ERR_PARAM;
                            //发现插入的位置为负, 直接跳出循环
                            skip = 0;
                            if (j == 0) {
                                //当前key的第一个value就出错了
                                count -= keysize;
                            } else {
                                //当前key的第x个value出错了
                                memcpy(countstart, &j, sizeof(int));
                            }
                            break;    
                        }
                        ret  = hashtable_insert(g_runtime->ht, tbname, key, value, 
                                                attrarray, attrnum, pos);
                        DINFO("hashtable_add_attr: %d\n", ret);
                        if (ret < 0) {
                            //插入hashtable有错， 直接跳出循环
                            skip = 0;
                            if (j == 0) {
                                count -= keysize;
                            } else {
                                memcpy(countstart, &j, sizeof(int));
                            }
                            break;
                        }
                        count += psize;
                        j++;//value计数
                    }
                    i++;//key计数
                }
                DINFO("count: %d\n", count);
                if (conn && writelog) {
                    memcpy(retdata, &i, sizeof(int));
                    memcpy(retdata + sizeof(int), &j, sizeof(int));
                    conn_send_buffer_reply(conn, ret, retdata, sizeof(int) * 2);
                    ret = MEMLINK_REPLIED;
                    //如果插入第一个key的第一个value有错， 不记录binlog
                    if (count == sizeof(int) + sizeof(char)) {
                        goto wdata_apply_over;
                    }
                    //改变包的大小
                    if (count < package_len) {
                        datalen = count;
                        count -= sizeof(int);
                        memcpy(data, &count, sizeof(int));
                    }
                }
            }
            break;
        case CMD_SET_CONFIG:
        {
            DINFO("<<< CMD SET CONFIG >>>\n"); 
            char key[512] = {0};
            char value[512] = {0};
            int  ret;
            uint8_t need_kill = 0;
            int  setrole = 0;

            ret = cmd_set_config_dynamic_unpack(data, key, value);
            if (ret <= 0) {
                DINFO("set config error! ret: %d\n", ret);
                goto wdata_apply_over;
            }

            //char master_sync_host[20] = {0};
            //int  master_sync_port = 0;

            DINFO("key: %s\n", key);
            if (strcmp(key, "block_data_reduce") == 0) {
                g_cf->block_data_reduce = atof(value);
            } else if (strcmp(key, "block_clean_cond") == 0) {
                g_cf->block_clean_cond = atof(value);
            } else if (strcmp(key, "block_clean_start") == 0) {
                g_cf->block_clean_start = atoi(value);
            } else if (strcmp(key, "sync_check_interval") == 0) {
                g_cf->sync_check_interval = atoi(value);
            } else if (strcmp(key, "block_clean_num") == 0) {
                g_cf->block_clean_num = atoi(value);
            } else if (strcmp(key, "timeout") == 0) {
                g_cf->timeout = atoi(value);
            } else if (strcmp(key, "log_level") == 0) {
                g_cf->log_level = atoi(value);
                g_log->loglevel = g_cf->log_level;
            } else if (strcmp(key, "master_sync_host") == 0) {
                if (strcmp(g_cf->master_sync_host, value) != 0) {
                    strcpy(g_cf->master_sync_host, value);
                    need_kill = 1;
                }
            } else if (strcmp(key, "master_sync_port") == 0) {
                int port = atoi(value);
                if (g_cf->master_sync_port != port) {
                    g_cf->master_sync_port = port;
                    need_kill = 1;
                }
            } else if (strcmp(key, "role") == 0) {
                if (strcmp(value, "master") == 0)
                    setrole = ROLE_MASTER;
                else if (strcmp(value, "slave") == 0)
                    setrole = ROLE_SLAVE;
                else if (strcmp(value, "backup") == 0)
                    setrole = ROLE_BACKUP;
            } else {
                ret = MEMLINK_ERR_PARAM;
            }

            if (g_cf->role == ROLE_SLAVE) {//本身是从
                if (setrole == ROLE_MASTER) {//需要切换成主
                    //wait_thread_exit(g_runtime->slave->threadid);
                    sslave_stop(g_runtime->slave);
                    g_cf->role = ROLE_MASTER;
                    DINFO("=========slave to master\n");
                } else if (setrole == ROLE_BACKUP) {//需要切换成backup状态
                    //wait_thread_exit(g_runtime->slave->threadid);
                    sslave_stop(g_runtime->slave);
                    g_cf->role = ROLE_BACKUP;
                    DINFO("=========slave to backup\n");
                } else if (need_kill && (setrole == ROLE_SLAVE || setrole == 0)) {//更改了ip或者port
                    //wait_thread_exit(g_runtime->slave->threadid);
                    /*
                    if (g_runtime->slave) {
                        sslave_thread(g_runtime->slave);
                    } else {
                        g_runtime->slave = sslave_create();
                    }*/
                    sslave_go(g_runtime->slave);
                }
            } else if ((g_cf->role == ROLE_BACKUP || g_cf->role == ROLE_MASTER) && setrole == ROLE_SLAVE) {
                //本身能时master或者backup,需要切换成从
                /*
                if (g_runtime->slave) {
                    sslave_thread(g_runtime->slave);
                } else {
                    g_runtime->slave = sslave_create();
                }*/
                g_cf->role = ROLE_SLAVE;
                sslave_go(g_runtime->slave);
                DINFO("=============master to slave\n");
            }
            ret = MEMLINK_OK;
            
            //myconfig_change();
            goto wdata_apply_over;
        }
        default:
            ret = MEMLINK_ERR_CLIENT_CMD;
            goto wdata_apply_over;
            break;
    }
    DINFO("============================================ret: %d\n", ret);
    // write binlog
    if (writelog && (ret >= 0 || ret == MEMLINK_REPLIED)) {
        int sret = synclog_write(g_runtime->synclog, data, datalen);
        if (sret < 0) {
            DERROR("synclog_write error: %d\n", sret);
            MEMLINK_EXIT;
        }
        sret = syncmem_write(g_runtime->syncmem, data, datalen, g_runtime->synclog->version, g_runtime->synclog->index_pos -1);
        if (sret < 0) {
            DERROR("synclog_write error: %d\n", sret);
            MEMLINK_EXIT;
        }
    }
    
    if (time(NULL) % 10 < 3) {
        wdata_check_clean(tbname, key);
    }
wdata_apply_over:
    return ret;
}

/**
 * Execute the write command and send response to client.
 *
 * @param conn
 * @param data command data
 * @param datalen the length of data parameter
 */
int
wdata_ready(Conn *conn, char *data, int datalen)
{
    struct timeval start, end;
    uint8_t cmd;
    int ret = 0;

    zz_check(data);
    //memcpy(&cmd, data + sizeof(short), sizeof(char));
    // slave only part of write commands
    memcpy(&cmd, data + sizeof(int), sizeof(char));

#ifdef WITH_MASTER_BACKUP
    if (cmd >= CMD_VOTE_MIN && cmd <= CMD_VOTE_MAX) {
        vote_ready(conn, data, datalen);
        return 0;
    }
#endif

    if (g_cf->role == ROLE_SLAVE && cmd != CMD_DUMP && 
        cmd != CMD_CLEAN && cmd != CMD_SET_CONFIG) {
        ret = MEMLINK_ERR_CLIENT_CMD;
        goto wdata_ready_over;
    }


    gettimeofday(&start, NULL);
    // check max memory
    if (start.tv_sec - g_runtime->mem_check_last > MEM_CHECK_INTERVAL) {
        g_runtime->mem_used = get_process_mem(getpid());
        g_runtime->mem_check_last = start.tv_sec;
    }
    
    //DNOTE("check mem, used:%lld, max:%lld\n", g_runtime->mem_used, g_cf->max_mem);
    if (g_cf->max_mem > 0 && g_runtime->mem_used > g_cf->max_mem) {
        DERROR("memory used/max: %lld/%lld\n", g_runtime->mem_used, g_cf->max_mem);
        conn_send_buffer_reply(conn, MEMLINK_ERR_MEM, NULL, 0);
        return 0;
    }
    
    DINFO("mode:%d, role:%d\n", g_cf->sync_mode, g_cf->role);
#ifdef WITH_MASTER_SLAVE
    if (g_cf->sync_mode == MODE_MASTER_BACKUP) {
        if (g_cf->role == ROLE_MASTER) {
            DINFO("master ready ...\n");
            ret = master_ready(conn, data, datalen);
            return 0;
        }else{
            DINFO("back ready ...\n");
            ret = backup_ready(conn, data, datalen);
        }
    }else{
        pthread_mutex_lock(&g_runtime->mutex);
        ret = wdata_apply(data, datalen, MEMLINK_WRITE_LOG, conn);
        pthread_mutex_unlock(&g_runtime->mutex);
    }
#else
    pthread_mutex_lock(&g_runtime->mutex);
    ret = wdata_apply(data, datalen, MEMLINK_WRITE_LOG, conn);
    pthread_mutex_unlock(&g_runtime->mutex);
#endif
    
    zz_check(conn);

wdata_ready_over:
    if (ret != MEMLINK_REPLIED) {
        conn_send_buffer_reply(conn, ret, NULL, 0);
    }
    gettimeofday(&end, NULL);
    DINFO("%s:%d cmd:%d %d %u us\n", conn->client_ip, conn->client_port, cmd, ret, timediff(&start, &end));
    DINFO("conn_send_buffer_reply return: %d\n", ret);

    return 0;
}

void
wconn_destroy(Conn *conn)
{
    WThread *wt;
    ConnInfo *conninfo = NULL; //SyncConnInfo *sconninfo;

    wt = (WThread *)conn->thread;
    if (wt) {
        conninfo = (ConnInfo *)wt->rw_conn_info;
        wt->conns--;
    }
    int i;
    if (conninfo) {
        for (i = 0; i < g_cf->max_write_conn; i++) {
            if (conn->sock == conninfo[i].fd) {
                memset(&conninfo[i], 0x0, sizeof(ConnInfo));
                break;
            }
        }
    }

    conn_destroy(conn);
}

/**
 * Callback for incoming client write connection.
 *
 * @param fd file descriptor for listening socket
 * @param event
 * @param arg thread base
 */
void
wthread_read(int fd, short event, void *arg)
{
    WThread *wt = (WThread*)arg;
    Conn    *conn;
    
    DINFO("wthread_read ...\n");
    conn = conn_create(fd, sizeof(Conn));
    if (conn) {
        int ret = 0;
        conn->port  = g_cf->write_port;
        conn->base  = wt->base;
        conn->ready = wdata_ready;
        conn->destroy = wconn_destroy;
        //conn->read  = client_read;
        //conn->write = client_write;
        //连接统计信息
        int i;
        RwConnInfo *conninfo = wt->rw_conn_info;
        wt->conns++;
        
        for (i = 0; i < g_cf->max_write_conn; i++) {
            conninfo = &(wt->rw_conn_info[i]);
            if (conninfo->fd == 0) {
                conninfo->fd = conn->sock;
                strcpy(conninfo->client_ip, conn->client_ip);
                conninfo->port = conn->client_port;
                memcpy(&conninfo->start, &conn->ctime, sizeof(struct timeval));
                break;
            }    
        }
        conn->thread = wt;

        // check connection limit
        if (conn_check_max((Conn*)conn) != MEMLINK_OK) {
            DERROR("too many write conn.\n");
            conn->destroy((Conn*)conn);
            return;
        }
    
        DINFO("new conn: %d\n", conn->sock);
        DINFO("change event to read.\n");
        ret = change_event(conn, EV_READ|EV_PERSIST, g_cf->timeout, 1);

        zz_check(conn);

        if (ret < 0) {
            DERROR("change_event error: %d, close conn.\n", ret);
            conn->destroy(conn);
        }
    }
}

/**
 * Read client request, execute the command and send response. 
 *
 * @param fd
 * @param event
 * @param arg connection
 */
/*void
client_read(int fd, short event, void *arg)
{
    Conn *conn = (Conn*)arg;
    int     ret;
    //unsigned short   datalen = 0;
    uint32_t datalen = 0;

    zz_check(conn);

    if (event & EV_TIMEOUT) {
        DWARNING("read timeout:%d, close\n", fd);
        //conn->destroy(conn);
        conn->timeout(conn);
        return;
    }
    //  Called more than one time for the same command and aready receive the 
    //  2-byte command length.
    if (conn->rlen >= sizeof(int)) {
        //memcpy(&datalen, conn->rbuf, sizeof(short)); 
        memcpy(&datalen, conn->rbuf, sizeof(int));
    }
    DINFO("client read datalen: %d, fd: %d, event:%x\n", datalen, fd, event);
    DINFO("conn rlen: %d\n", conn->rlen);

    while (1) {
        int rlen = datalen;
        // If command length is unavailable, use max length.
        if (rlen == 0) {
            rlen = CONN_MAX_READ_LEN - conn->rlen;
        }
        DINFO("try read len: %d\n", rlen);
        if (conn->rsize - conn->rlen < rlen) {

            DINFO("conn->rsize: %d, conn->rlen: %d, malloc new rbuf\n", conn->rsize, conn->rlen);
            char *newbuf = (char *)zz_malloc(rlen + conn->rsize);
            memcpy(newbuf, conn->rbuf, conn->rlen);
            conn->rsize += rlen;
            zz_free(conn->rbuf);
            conn->rbuf = newbuf;
        }
        ret = read(fd, &conn->rbuf[conn->rlen], rlen);
        DINFO("read return: %d\n", ret);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }else if (errno != EAGAIN) {
                char errbuf[1024];
                strerror_r(errno, errbuf, 1024);
                DINFO("%d read error: %s, close conn.\n", fd,  errbuf);
                conn->destroy(conn);
                return;
            }else{
                char errbuf[1024];
                strerror_r(errno, errbuf, 1024);
                DERROR("%d read EAGAIN, error %d: %s\n", fd, errno,  errbuf);
            }
        }else if (ret == 0) {
            DINFO("read 0, close conn %d.\n", fd);
            conn->destroy(conn);
            return;
        }else{
            conn->rlen += ret;
            DINFO("2 conn rlen: %d\n", conn->rlen);
        }

        break;
    }

    zz_check(conn);

    DINFO("conn rbuf len: %d\n", conn->rlen);
    while (conn->rlen >= sizeof(int)) {
        //memcpy(&datalen, conn->rbuf, sizeof(short));
        memcpy(&datalen, conn->rbuf, sizeof(int));
        DINFO("check datalen: %d, rlen: %d\n", datalen, conn->rlen);
        //int mlen = datalen + sizeof(short);
        int mlen = datalen + sizeof(int);

        if (conn->rlen >= mlen) {
            conn->ready(conn, conn->rbuf, mlen);
            memmove(conn->rbuf, conn->rbuf + mlen, conn->rlen - mlen);
            conn->rlen -= mlen;

            zz_check(conn->rbuf);
        }else{
            break;
        }
    }
}
*/
/**
 * Send data inside the connection to client. If all the data has been sent, 
 * register read event for this connection.
 *
 * @param fd
 * @param event
 * @param arg connection
 */
/*void
client_write(int fd, short event, void *arg)
{
    Conn  *conn = (Conn*)arg;
    if (event & EV_TIMEOUT) {
        DWARNING("write timeout:%d, close\n", fd);
        //conn->destroy(conn);
        conn->timeout(conn);
        return;
    }

    if (conn->wpos == conn->wlen) {
        DINFO("client write ok!\n");
        conn->wlen = conn->wpos = 0;
        conn->wrote(conn);
        return;
    }
    conn_write(conn);
}*/

WThread*
wthread_create()
{
    WThread *wt;

    wt = (WThread*)zz_malloc(sizeof(WThread));
    if (NULL == wt) {
        DERROR("wthread malloc error.\n");
        MEMLINK_EXIT;
    }
    memset(wt, 0, sizeof(WThread));
    
    //写连接统计信息    
    wt->rw_conn_info = (RwConnInfo *)zz_malloc(sizeof(RwConnInfo) * g_cf->max_write_conn); 
    if (wt->rw_conn_info == NULL) {
        DERROR("wthread connect info malloc error.\n");
        MEMLINK_EXIT;
    }
    memset(wt->rw_conn_info, 0x0, sizeof(RwConnInfo) * g_cf->max_write_conn);

    wt->sock = tcp_socket_server(g_cf->host, g_cf->write_port); 
    if (wt->sock == -1) {
        MEMLINK_EXIT;
    }
    
    DINFO("write socket create ok\n");

    wt->base = event_base_new();
    
    event_set(&wt->event, wt->sock, EV_READ | EV_PERSIST, wthread_read, wt);
    event_base_set(wt->base, &wt->event);
    event_add(&wt->event, 0);

    if (g_cf->dump_interval > 0) {
        struct timeval tm;
        evtimer_set(&wt->dumpevt, dumpfile_call_loop, &wt->dumpevt);
        evutil_timerclear(&tm);
        tm.tv_sec = g_cf->dump_interval; 
        event_base_set(wt->base, &wt->dumpevt);
        event_add(&wt->dumpevt, &tm);
    }

    if (g_cf->sync_disk_interval > 0) {
        struct timeval tm;
        evtimer_set(&wt->sync_disk_evt, synclog_sync_disk, &wt->sync_disk_evt);
        evutil_timerclear(&tm);
        tm.tv_sec = g_cf->sync_disk_interval; 
        event_base_set(wt->base, &wt->sync_disk_evt);
        event_add(&wt->sync_disk_evt, &tm);
    }



    g_runtime->wthread = wt;

    // for test
#ifdef WITH_MASTER_BACKUP
    if (g_cf->sync_mode == MODE_MASTER_BACKUP) {
        g_cf->role = ROLE_NONE;
        uint64_t eid = 0;
        eid |= g_runtime->synclog->version;
        eid = eid << 32;
        eid |= g_runtime->synclog->index_pos;
        g_runtime->voteid = 0;
        request_vote(eid, COMMITED, g_runtime->voteid+1, g_cf->write_port);
    }
#endif
    pthread_t       threadid;
    pthread_attr_t  attr;
    int             ret;
    
    ret = pthread_mutex_init(&wt->rmlock, NULL);
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("pthread_mutex_init error: %s\n",  errbuf);
        MEMLINK_EXIT;
        return NULL;
    }
    DINFO("mutex init ok!\n");


    ret = pthread_attr_init(&attr);
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("pthread_attr_init error: %s\n",  errbuf);
        MEMLINK_EXIT;
    }

    ret = pthread_create(&threadid, &attr, wthread_loop, wt);
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("pthread_create error: %s\n",  errbuf);
        MEMLINK_EXIT;
    }

    ret = pthread_detach(threadid);
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("pthread_detach error; %s\n",  errbuf);
        MEMLINK_EXIT;
    }

    DINFO("create WThread ok!\n");

    return wt;
}

void
sig_wthread_handler()
{
    DINFO("---------------this is wthread\n");
}

void*
wthread_loop(void *arg)
{
    struct sigaction sigact;
    WThread *wt = (WThread*)arg;
    DINFO("wthread_loop ...\n");
    event_base_loop(wt->base, 0);
    
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;

    sigact.sa_handler = sig_wthread_handler;
    sigaction(SIGUSR1, &sigact, NULL);
    return NULL;
}

void
wthread_destroy(WThread *wt)
{
    zz_free(wt->rw_conn_info);
    zz_free(wt);
}

/**
 * @}
 */
