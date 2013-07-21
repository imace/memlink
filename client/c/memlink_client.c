#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <netdb.h>
#ifdef __linux
    #include <linux/if.h>
    #include <sys/un.h>
#endif
#include "memlink_client.h"
#include "logfile.h"
#include "zzmalloc.h"
#include "utils.h"
#include "network.h"
#include "serial.h"

#define RECV_PKG_SIZE_LEN    sizeof(int)

MemLink*    
memlink_create(char *host, int readport, int writeport, int timeout)
{
    MemLink *m;

#ifdef DEBUG
    /*if (g_log == NULL) {
        logfile_create("stdout", 3);
    }*/
#endif

    m = (MemLink*)zz_malloc(sizeof(MemLink));
    if (NULL == m) {
        DERROR("malloc MemLink error!\n"); 
        return NULL;
    }
    memset(m, 0, sizeof(MemLink));

    snprintf(m->host, 16, "%s", host);
    m->read_port  = readport;
    m->write_port = writeport;
    m->timeout    = timeout;
    m->auto_reconn= 1;

    return m;
}


static int
memlink_connect(MemLink *m, int fdtype)
{
    int ret;
    int sock;
    
    //DINFO("memlink connect ...\n");
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sock) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("socket create error: %s\n",  errbuf);
        return MEMLINK_ERR_CLIENT_SOCKET;
    }

    struct linger ling = {1, 0}; 
    ret = setsockopt(sock, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("setsockopt LINGER error: %s\n",  errbuf);
    }

    int flags = 1;
    ret = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void *)&flags, sizeof(flags));
    if (ret != 0) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("setsockopt NODELAY error: %s\n",  errbuf);
    }

    flags = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));

    struct sockaddr_in  sin; 

    sin.sin_family = AF_INET;
    if (fdtype == MEMLINK_READER) {
        sin.sin_port = htons((short)(m->read_port));
    }else if (fdtype == MEMLINK_WRITER) {
        sin.sin_port = htons((short)(m->write_port));
    }else{
        return MEMLINK_ERR_CONN_PORT;
    }

    if (m->host[0] == 0) {
        sin.sin_addr.s_addr = htonl(INADDR_ANY);
    }else{
        sin.sin_addr.s_addr = inet_addr(m->host);
    }    

    set_noblock(sock); 

    do { 
        ret = connect(sock, (struct sockaddr*)&sin, sizeof(sin));
    } while (ret == -1 && errno == EINTR);  

    if (ret == -1 && (errno == EINPROGRESS ||errno == EALREADY)) {
        int retv;
        retv = timeout_wait_write(sock, m->timeout);
        if (retv == TRUE) {
            int error = 0;
            socklen_t slen = sizeof(int);
            int retv = getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &slen);
            if (retv != 0 || error != 0) {
                char errbuf[1024];
                strerror_r(errno, errbuf, 1024);
                DERROR("connect error: %s\n", errbuf);
                return MEMLINK_ERR_CONNECT;
            }
        }else if (retv == FALSE){
            DERROR("connect timeout.\n");
            close(sock);
            return MEMLINK_ERR_CONNECT;
        }else if (retv == ERROR) {
            DERROR("connect error.\n");
            close(sock);
            return MEMLINK_ERR_CONNECT;
        }
    }else if (ret == -1 && errno != EISCONN) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("connect error: %s\n",  errbuf);
        close(sock);

        return MEMLINK_ERR_CONNECT;
    }
    //DNOTE("connect ok!\n");
    set_block(sock); 

    if (fdtype == MEMLINK_READER) {
        m->readfd = sock;
    }else{
        m->writefd = sock;
    }

    //DINFO("connect ret: %d\n", ret);
    /*if (ret >= 0) {
        if (fdtype == MEMLINK_READER) {
            m->readfd = sock;
        }else{
            m->writefd = sock;
        }
    }else{
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("connect error: %s\n",  errbuf);
        close(sock);
        ret = MEMLINK_ERR_CONNECT;
    }*/

    return MEMLINK_OK;
}

// TODO should check the received data is not bigger than rlen
static int
memlink_read(MemLink *m, int fdtype, char *rbuf, int rlen)
{
    //int rdlen = 0; 
    //int nleft = rlen; // TODO remove it if useless
    uint32_t datalen = 0;
    int ret;
    int fd;
   
    if (fdtype == MEMLINK_READER) {
        fd = m->readfd;
    }else if (fdtype == MEMLINK_WRITER) {
        fd = m->writefd;
    }else{
        return MEMLINK_ERR_CLIENT;
    }
    //DINFO("fd: %d\n", fd);
    
    if (fd <= 0) {
        //DINFO("read fd connect ...\n");
        ret = memlink_connect(m, fdtype);
        if (ret < 0) {
            return ret;
        }
    }
    ret = readn(fd, rbuf, RECV_PKG_SIZE_LEN, m->timeout);
    //DINFO("read head: %d\n", ret);
    if (ret != RECV_PKG_SIZE_LEN) {
        DERROR("read head error! ret:%d, %lld\n", ret, (long long)RECV_PKG_SIZE_LEN);
        close(fd);

        if (fd == m->readfd) {
            m->readfd = -1;
        }else{
            m->writefd = -1;
        }
        if (ret <= 0) {
            return MEMLINK_ERR_CONN_LOST;
        }else{
            return MEMLINK_ERR_RECV_HEADER;
        }
    }
    memcpy(&datalen, rbuf, RECV_PKG_SIZE_LEN);
    //char buf[64];
    //DINFO("read body: %d, %s\n", datalen, formath(rbuf, RECV_PKG_SIZE_LEN, buf, 62));
    //DINFO("read body: %d\n", datalen);

    if (datalen + RECV_PKG_SIZE_LEN > rlen) {
        DERROR("datalen bigger than buffer size: %d, %d\n", datalen, rlen);
        return MEMLINK_ERR_RECV_BUFFER;
    }

    ret = readn(fd, rbuf + RECV_PKG_SIZE_LEN, datalen, m->timeout);
    //DINFO("readn return: %d\n", ret);
    if (ret != datalen) {
        DERROR("read data error! ret:%d\n", ret);
        close(fd);

        if (fd == m->readfd) {
            m->readfd = -1;
        }else{
            m->writefd = -1;
        }
        return MEMLINK_ERR_RECV;
    }

    return ret + RECV_PKG_SIZE_LEN;
}

static int
memlink_write(MemLink *m, int fdtype, char *wbuf, int wlen)
{
    //int wrlen = 0; 
    //int nleft = wlen;
    int ret;
    int fd;
   
    if (fdtype == MEMLINK_READER) {
        fd = m->readfd;
    }else if (fdtype == MEMLINK_WRITER) {
        fd = m->writefd;
    }else{
        DERROR("fdtype error:%d\n", fdtype);
        return MEMLINK_ERR_CLIENT;
    }
    //DINFO("fd: %d\n", fd);

    int reconn = m->auto_reconn;
    while (1) {
        if (fd <= 0) {
            //DINFO("write fd connect ...\n");
            ret = memlink_connect(m, fdtype);
            //DINFO("memlink_connect return: %d\n", ret);
            if (ret < 0) {
                return ret;
            }

            if (fdtype == MEMLINK_READER) {
                fd = m->readfd;
            }else{
                fd = m->writefd;
            }
        }
      
        // write 1
        int wret = 0;
#ifdef WRITE2
        do {
            wret = write(fd, wbuf, 1);
        }while (wret == -1 && errno == EINTR);
        DINFO("wret:%d\n", wret);
        if (wret < 0) {
            if (fdtype == MEMLINK_READER) {
                m->readfd = -1;
            }else{
                m->writefd = -1;
            }
            close(fd);
            fd = -1;

            if (reconn > 0) {
                DINFO("conn lost, reconnect.\n");
                reconn--;
                continue;
            }
            ret = MEMLINK_ERR_CONN_LOST;
        }
#endif
//#ifdef PEEK
        char buf[4];
#ifdef __linux
        wret = recv(fd, buf, 1, MSG_PEEK|MSG_DONTWAIT);  
#else
        set_noblock(fd);
        wret = recv(fd, buf, 1, MSG_PEEK);  
        set_block(fd);
#endif
        if (wret == 0) {
            if (fdtype == MEMLINK_READER) {
                m->readfd = -1;
            }else{
                m->writefd = -1;
            }
            close(fd);
            fd = -1;

            if (reconn > 0) {
                DINFO("conn lost, reconnect.\n");
                reconn--;
                continue;
            }

            ret = MEMLINK_ERR_CONN_LOST;
        }
        
        wret = 0;
//#endif

        //wret = write(fd, wbuf, 1);
        //DINFO("wret:%d\n", wret);

        ret = writen(fd, (char*)wbuf+wret, wlen-wret, m->timeout);
        DINFO("writen: %d\n", ret);
        //if (ret <= 0) {
        if (ret < 0) {
            if (fdtype == MEMLINK_READER) {
                m->readfd = -1;
            }else{
                m->writefd = -1;
            }
            close(fd);
            fd = -1;

            if (reconn > 0) {
                DINFO("conn lost, reconnect.\n");
                reconn--;
                continue;
            }

            ret = MEMLINK_ERR_CONN_LOST;
        }else if (ret < wlen - wret) {
            DERROR("write data error, ret: %d\n", ret);
            if (fdtype == MEMLINK_READER) {
                m->readfd = -1;
            }else{
                m->writefd = -1;
            }
            close(fd);
            fd = -1;

            if (reconn > 0) {
                DINFO("conn lost, reconnect.\n");
                reconn--;
                continue;
            }

            ret = MEMLINK_ERR_SEND;
        }else{
            ret += wret; // add first send 1b
        }
        break;
    }

    return ret;
}

/**
 * Send a command to memlink server and recieve the response. Response format:
 * -----------------------------------------------------------
 * | response length (2 bytes) | return code (2 bytes)| data |
 * -----------------------------------------------------------
 *
 * @param m memlink client
 * @param fdtype read or write
 * @param data command data
 * @param len command length
 * @param retdata return data
 * @param retlen max length of return data
 * @return return code
 *
 */
static int
memlink_do_cmd(MemLink *m, int fdtype, char *data, int len, char *retdata, int retlen)
{
    int ret;
    //DINFO("======= write to server ...\n");
    ret = memlink_write(m, fdtype, data, len);
    //DINFO("memlink_write ret: %d, len: %d\n", ret, len);
    
    if (ret >= 0 && ret != len) {
        DERROR("memlink write data error! ret:%d, len:%d\n", ret, len);
        return MEMLINK_ERR_SEND;
    }
    if (ret < 0)
        return ret;

    //DINFO("read from server ...\n"); 
    ret = memlink_read(m, fdtype, retdata, retlen);
    //DINFO("memlink_read return: %d\n", ret);

    if (ret > 0) {
        short retcode;
/*#ifdef DEBUG
        char buf[10240];
        DINFO("ret:%d, read: %s\n", ret, formath(retdata, ret, buf, 10240));
#endif*/

        memcpy(&retcode, retdata + RECV_PKG_SIZE_LEN, sizeof(short));
        //DINFO("retcode: %d\n", retcode);
        if (retcode != 0) {
            DNOTE("retcode error:%d\n", retcode);
            return retcode;
        }
        //DINFO("ret: %d\n", ret);
        return ret;  // return read data len
    }else{
        DERROR("memlink recv data error! ret:%d\n", ret);
        //return MEMLINK_ERR_RECV;
        return ret;
    }
}

/**
 * send a simple package to memlink server
 * @param m memlink handle
 * @return status of action
 */
int
memlink_cmd_ping(MemLink *m)
{
    char data[1024];
    int  len, ret;

    len = cmd_ping_pack(data);
    //DINFO("pack ping len: %d\n", len); 

    char retdata[1024];
    ret = memlink_do_cmd(m, MEMLINK_READER, data, len, retdata, 1024);

    if (ret < 0) 
        return ret;
    return MEMLINK_OK;
}

int
memlink_cmd_dump(MemLink *m)
{
    char data[1024] = {0};
    int  len, ret;

    len = cmd_dump_pack(data);
    //DINFO("pack dump len: %d\n", len); 

    char retdata[1024] = {0};
    ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, 1024);

    if (ret < 0) 
        return ret;
    return MEMLINK_OK;
}

int
memlink_cmd_clean(MemLink *m, char *table, char *key)
{
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;

    char data[1024] = {0};
    int  len;

    len = cmd_clean_pack(data, table, key);
    //DINFO("pack clean len: %d\n", len);

    char retdata[1024] = {0};
    int ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, 1024);
    if (ret < 0) 
        return ret;
    return MEMLINK_OK;
}

int
memlink_cmd_clean_all(MemLink *m)
{
    if (m == NULL) {
        return MEMLINK_ERR_PARAM;
    }
    char data[1024] = {0};
    int len;

    len = cmd_clean_all_pack(data);

    char retdata[1024] = {0};
    int ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, 1024);
    if (ret < 0)
        return ret;
    return MEMLINK_OK;
}

int 
memlink_cmd_stat(MemLink *m, char *table, char *key, MemLinkStat *stat)
{
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;

    char data[1024] = {0};
    int  len;

    len = cmd_stat_pack(data, table, key);
    //DINFO("pack stat len: %d\n", len);

    char retdata[1024] = {0};
    int ret = memlink_do_cmd(m, MEMLINK_READER, data, len, retdata, 1024);
    //DINFO("memlink_do_cmd: %d\n", ret);
    if (ret <= 0) {
        return ret;
    }

    int  pos = CMD_REPLY_HEAD_LEN;
    memcpy(stat, retdata + pos, sizeof(MemLinkStat));
    /*
#ifdef DEBUG
    char buf[512] = {0};
    DINFO("stat buf: %s\n", formath((char *)stat, sizeof(MemLinkStat), buf, 512));

    DINFO("stat blocks:%d, data:%d, data_used:%d, mem:%d, valuesize:%d, attrsize:%d\n", 
                    stat->blocks, stat->data, stat->data_used, 
                    stat->mem, stat->valuesize, stat->attrsize);
#endif
    */ 
    return MEMLINK_OK;
}

int 
memlink_cmd_stat_sys(MemLink *m, MemLinkStatSys *stat)
{
    char data[1024] = {0};
    int  len;

    len = cmd_stat_sys_pack(data);
    //DINFO("pack stat sys len: %d\n", len);

    char retdata[1024] = {0};
    int ret = memlink_do_cmd(m, MEMLINK_READER, data, len, retdata, 1024);
    //DINFO("memlink_do_cmd: %d\n", ret);
    if (ret <= 0) {
        return ret;
    }

    int  pos = CMD_REPLY_HEAD_LEN;
    memcpy(stat, retdata + pos, sizeof(MemLinkStatSys));
    return MEMLINK_OK;
}

int
memlink_cmd_create_table(MemLink *m, char *table, int valuelen, char *attrstr,
                   uint8_t listtype, uint8_t valuetype)
{
    if (NULL == table || strlen(table) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;
    if (valuelen <= 0 || valuelen > HASHTABLE_VALUE_MAX)
        return MEMLINK_ERR_PARAM;
    
    char data[1024] = {0};
    int  len;
    int  attrnum = 0;
    uint32_t attrarray[HASHTABLE_ATTR_MAX_ITEM] = {0};   

    attrnum = attr_string2array(attrstr, attrarray);
    //DINFO("create attrnum: %d\n", attrnum);    
    if (attrnum < 0 || attrnum > HASHTABLE_ATTR_MAX_ITEM)
        return MEMLINK_ERR_PARAM;

    int i = 0;
    for (i = 0; i < attrnum; i++) {
        if (attrarray[i] > HASHTABLE_ATTR_MAX_BIT) {
            DERROR("attrarray[%d]: %d\n", i, attrarray[i]);
            return MEMLINK_ERR_PARAM;
        }
    }
   
    len = cmd_create_table_pack(data, table, valuelen, attrnum, attrarray, listtype, valuetype);
    //DINFO("pack create len: %d\n", len);

    char retdata[1024] = {0};
    int ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, 1024);
    if (ret < 0) 
        return ret;
    return MEMLINK_OK;

}

int 
memlink_cmd_create_table_list(MemLink *m, char *table, int valuelen, char *attrstr)
{
    return memlink_cmd_create_table(m, table, valuelen, attrstr, MEMLINK_LIST, MEMLINK_VALUE_OBJ);
}

int 
memlink_cmd_create_table_queue(MemLink *m, char *table, int valuelen, char *attrstr)
{
    return memlink_cmd_create_table(m, table, valuelen, attrstr, MEMLINK_QUEUE, MEMLINK_VALUE_OBJ);
}

int 
memlink_cmd_create_table_sortlist(MemLink *m, char *table, int valuelen, char *attrstr, uint8_t valuetype)
{
    return memlink_cmd_create_table(m, table, valuelen, attrstr, MEMLINK_SORTLIST, valuetype);
}

int
memlink_cmd_create_node(MemLink *m, char *table, char *key)
{
    if (NULL == table || strlen(table) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;
    
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX) {
        return MEMLINK_ERR_PARAM;
    }
    char data[1024] = {0};
    int  len;

    len = cmd_create_node_pack(data, table, key);
    //DINFO("pack del len: %d\n", len);
    char retdata[1024] = {0};
    int ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, 1024);
    if (ret < 0) 
        return ret;
    return MEMLINK_OK;
}


int
memlink_cmd_remove_table(MemLink *m, char *table)
{
    if (NULL == table || table[0] == 0 || strlen(table) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;

    char data[1024] = {0};
    int  len;

    len = cmd_rmtable_pack(data, table);
    //DINFO("pack del len: %d\n", len);
    char retdata[1024] = {0};
    int ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, 1024);
    if (ret < 0) 
        return ret;
    return MEMLINK_OK;

}

int
memlink_cmd_del(MemLink *m, char *table,char *key, char *value, int valuelen)
{
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;
    if (valuelen <= 0 || valuelen > HASHTABLE_VALUE_MAX)
        return MEMLINK_ERR_PARAM;

    char data[1024] = {0};
    int  len;

    len = cmd_del_pack(data, table, key, value, valuelen);
    //DINFO("pack del len: %d\n", len);
    
    /*unsigned short pkglen;
    uint8_t  cmd;

    memcpy(&pkglen, data, sizeof(short));
    memcpy(&cmd, data + sizeof(short), sizeof(char));*/

    //DINFO("pkglen: %d, cmd: %d\n", pkglen, cmd);
    char retdata[1024] = {0};
    int ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, 1024);
    if (ret < 0) 
        return ret;
    return MEMLINK_OK;
}

int
memlink_cmd_sortlist_del(MemLink *m, char *table,char *key, int kind, char *attrstr, 
                        char *valmin, int vminlen, 
                        char *valmax, int vmaxlen)
{
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;
    
    if (MEMLINK_VALUE_ALL != kind && MEMLINK_VALUE_VISIBLE != kind && MEMLINK_VALUE_TAGDEL != kind)
        return MEMLINK_ERR_PARAM;
    

    uint32_t attrarray[HASHTABLE_ATTR_MAX_ITEM] = {0};
    int attrn = 0;

    attrn = attr_string2array(attrstr, attrarray);
    //DINFO("range attr len: %d\n", attrn);
    if (attrn < 0 || attrn > HASHTABLE_ATTR_MAX_ITEM)
        return MEMLINK_ERR_PARAM;
    

    char data[1024] = {0};
    int  len;

    len = cmd_sortlist_del_pack(data, table,key, kind, valmin, vminlen, valmax, vmaxlen, attrn, attrarray);
    //DINFO("pack sortlist_del len: %d\n", len);
    
    /*unsigned short pkglen;
    uint8_t  cmd;

    memcpy(&pkglen, data, sizeof(short));
    memcpy(&cmd, data + sizeof(short), sizeof(char));
    */
    //DINFO("pkglen: %d, cmd: %d\n", pkglen, cmd);
    char retdata[1024] = {0};
    int ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, 1024);
    if (ret < 0)
        return ret;
    return MEMLINK_OK;
}


int
memlink_cmd_insert(MemLink *m, char *table, char *key, char *value, int valuelen, char *attrstr, int pos)
{
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;
    if (valuelen <= 0 || valuelen > HASHTABLE_VALUE_MAX)
        return MEMLINK_ERR_PARAM;
    if (pos < -1)
        return MEMLINK_ERR_PARAM;
    
    char data[1024] = {0};
    int  len;
    uint32_t attrarray[HASHTABLE_ATTR_MAX_ITEM] = {0};
    int attrnum = 0;

    attrnum = attr_string2array(attrstr, attrarray);
    //DINFO("insert attr len: %d\n", attrnum);
    if (attrnum < 0 || attrnum > HASHTABLE_ATTR_MAX_ITEM)
        return MEMLINK_ERR_PARAM;

    len = cmd_insert_pack(data, table, key, value, valuelen, attrnum, attrarray, pos);
    //DINFO("pack del len: %d\n", len);

    char retdata[1024] = {0};
    int ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, 1024);

    if (ret < 0) 
        return ret;
    return MEMLINK_OK;
}

int
memlink_cmd_sortlist_insert(MemLink *m, char *table,char *key, char *value, int valuelen, char *attrstr)
{
    return memlink_cmd_insert(m, table,key, value, valuelen, attrstr, -1);
}

int 
memlink_cmd_move(MemLink *m, char *table,char *key, char *value, int valuelen, int pos)
{
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;
    if (valuelen <= 0 || valuelen > HASHTABLE_VALUE_MAX)
        return MEMLINK_ERR_PARAM;
    if (pos < -1)
        return MEMLINK_ERR_PARAM;

    char data[1024] = {0};
    int  len;

    len = cmd_move_pack(data, table,key, value, valuelen, pos);
    //DINFO("pack move len: %d\n", len);
    char retdata[1024] = {0};
    int ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, 1024);

    if (ret < 0) 
        return ret;
    return MEMLINK_OK;
}

int
memlink_cmd_attr(MemLink *m, char *table,char *key, char *value, int valuelen, char *attrstr)
{
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX) {
        DNOTE("key error:%s, allow max len:%d\n", key, HASHTABLE_KEY_MAX);
        return MEMLINK_ERR_PARAM;
    }
    if (valuelen <= 0 || valuelen > HASHTABLE_VALUE_MAX) {
        DNOTE("value error:%s, allow max len:%d\n", key, HASHTABLE_VALUE_MAX);
        return MEMLINK_ERR_PARAM;
    }

    char data[1024] = {0};
    int  len;
    uint32_t attrarray[HASHTABLE_ATTR_MAX_ITEM] = {0};
    int attrnum = 0;

    attrnum = attr_string2array(attrstr, attrarray);
    //DINFO("attr attr len: %d\n", attrnum);
    if (attrnum < 0 || attrnum > HASHTABLE_ATTR_MAX_ITEM) {
        DNOTE("attr number error:%d, allow max:%d\n", attrnum, HASHTABLE_ATTR_MAX_ITEM);
        return MEMLINK_ERR_PARAM;
    }
    
    //DINFO("key: %s, valuelen: %d\n", key, valuelen);
    len = cmd_attr_pack(data, table,key, value, valuelen, attrnum, attrarray);
    //DINFO("pack attr len: %d\n", len);
    //printh(data, len);
    char retdata[1024] = {0};
    int ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, 1024);

    if (ret < 0) 
        return ret;
    return MEMLINK_OK;
}

int
memlink_cmd_tag(MemLink *m, char *table,char *key, char *value, int valuelen, int tag)
{
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;
    if (valuelen <= 0 || valuelen > HASHTABLE_VALUE_MAX)
        return MEMLINK_ERR_PARAM;

    if ( MEMLINK_TAG_DEL != tag && MEMLINK_TAG_RESTORE != tag )
        return MEMLINK_ERR_PARAM;
    
    char data[1024] = {0};
    int  len;
    
    len = cmd_tag_pack(data, table,key, value, valuelen, tag);
    //DINFO("pack tag len: %d\n", len);
    char retdata[1024] = {0};
    int ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, 1024);
    if (ret < 0) 
        return ret;
    return MEMLINK_OK;
}

int
memlink_cmd_rmkey(MemLink *m, char *table,char *key)
{
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;

    char data[1024] = {0};
    int  len;

    len = cmd_rmkey_pack(data, table,key);
    //DINFO("pack rmkey len: %d\n", len);
    char retdata[1024] = {0};
    int ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, 1024);
    if (ret < 0) 
        return ret;
    return MEMLINK_OK;
}

int 
memlink_cmd_count(MemLink *m, char *table,char *key, char *attrstr, MemLinkCount *count)
{
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;

    uint32_t attrarray[HASHTABLE_ATTR_MAX_ITEM] = {0};
    int attrnum = 0;

    attrnum = attr_string2array(attrstr, attrarray);
    //DINFO("range attr len: %d\n", attrnum);
    if (attrnum < 0 || attrnum > HASHTABLE_ATTR_MAX_ITEM)
        return MEMLINK_ERR_PARAM;

    char data[1024] = {0};
    int  len;

    len = cmd_count_pack(data, table, key, attrnum, attrarray);
    //DINFO("pack rmkey len: %d\n", len);
    char retdata[1024] = {0};
    int ret = memlink_do_cmd(m, MEMLINK_READER, data, len, retdata, 1024);
    if (ret < 0) 
        return ret;

    int  pos = CMD_REPLY_HEAD_LEN;
    memcpy(count, retdata + pos, sizeof(MemLinkCount));
    //DINFO("count, visible: %d, tag: %d\n", count->visible_count, count->tagdel_count);

    return MEMLINK_OK;
}

int 
memlink_cmd_sortlist_count(MemLink *m, char *table, char *key, char *attrstr, char *valmin, int vminlen, 
                char *valmax, int vmaxlen, MemLinkCount *count)
{
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;

    uint32_t attrarray[HASHTABLE_ATTR_MAX_ITEM] = {0};
    int attrnum = 0;

    attrnum = attr_string2array(attrstr, attrarray);
    //DINFO("range attr len: %d\n", attrnum);
    if (attrnum < 0 || attrnum > HASHTABLE_ATTR_MAX_ITEM)
        return MEMLINK_ERR_PARAM;

    char data[1024] = {0};
    int  len;

    len = cmd_sortlist_count_pack(data, table, key, attrnum, attrarray, valmin, vminlen, valmax, vmaxlen);
    //DINFO("pack rmkey len: %d\n", len);
    char retdata[1024] = {0};
    int ret = memlink_do_cmd(m, MEMLINK_READER, data, len, retdata, 1024);
    if (ret < 0) 
        return ret;

    int  pos = CMD_REPLY_HEAD_LEN;
    memcpy(count, retdata + pos, sizeof(MemLinkCount));
    //DINFO("count, visible: %d, tag: %d\n", count->visible_count, count->tagdel_count);

    return MEMLINK_OK;
}

int 
memlink_result_parse(char *retdata, MemLinkResult *result)
{
    uint32_t dlen;
    memcpy(&dlen, retdata, sizeof(int));
    int count = 0;

    memcpy(&count, retdata + dlen, sizeof(int));
    //dlen = *(uint32_t*)(retdata);

    int  pos = CMD_REPLY_HEAD_LEN;

    uint8_t valuesize, attrsize, attrnum;
    uint8_t attrformat[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};

    valuesize = *(char*)(retdata + pos);
    pos += sizeof(char);
    attrsize = *(char*)(retdata + pos);
    pos += sizeof(char);
    attrnum = *(char*)(retdata + pos);
    pos += sizeof(char);
    memcpy(attrformat, retdata + pos, attrnum);
    pos += attrnum;

    //DINFO("valuesize: %d, attrsize: %d, attrnum:%d, pos:%d\n", valuesize, attrsize, attrnum, pos);
    char *vdata   = retdata + pos;
    //char *enddata = retdata + dlen + sizeof(int);

    result->count     = count;
    result->valuesize = valuesize;
    result->attrsize  = attrsize;

    if (count > 0) {
        result->items = (MemLinkItem*)zz_malloc(sizeof(MemLinkItem) * count);
        memset(result->items, 0, sizeof(MemLinkItem)*count);
    }else{
        result->items = NULL; 
    }

    int i;
    MemLinkItem *item = NULL;
    for (i = 0; i < count; i++) {
        item = &result->items[i];
        item->valuesize = valuesize;
        item->attrsize = attrsize;
        memcpy(item->value, vdata, valuesize);
        attr_binary2string(attrformat, attrnum, vdata + valuesize, attrsize, item->attr);
        if (i > 0) {
            result->items[i-1].next = &(result->items[i]);
        }
        vdata += valuesize + attrsize;
    }


/*#ifdef DEBUG
    int  datalen  = valuesize + attrsize;
    DINFO("vdata: %p, enddata: %p, datalen: %d\n", vdata, enddata, datalen);
    char buf1[128], buf2[128];
#endif*/
    //MemLinkResult   *mret;
/*
    MemLinkItem     *root = NULL, *cur = NULL;

    int count = 0;

    while (vdata < enddata) {
        MemLinkItem     *item;
        item = (MemLinkItem*)zz_malloc(sizeof(MemLinkItem));
        if (NULL == item) {
            MemLinkItem *tmp;

            while (root) {
                tmp = root;
                root = root->next;
                zz_free(tmp);
            }
            return MEMLINK_ERR_CLIENT;
        }
        memset(item, 0, sizeof(MemLinkItem));
        
        if (root == NULL) {
            root = item;
        }

        if (cur == NULL) {
            cur = item;
        }else{
            cur->next = item;
            cur = item;
        }
       
        memcpy(item->value, vdata, valuesize);
        //item->value[valuesize] = 0;
#ifdef RANGE_ATTR_STR
        uint8_t mlen;
        memcpy(&mlen, vdata + valuesize, sizeof(char));
        memcpy(item->attr, vdata + valuesize + sizeof(char), mlen);
        //item->attr[mlen] = 0;
        //vdata += datalen;
        vdata += valuesize + sizeof(char) + mlen;
#else
        //int mlen = attr_binary2string(attrformat, attrnum, vdata + valuesize, attrsize, item->attr);
        attr_binary2string(attrformat, attrnum, vdata + valuesize, attrsize, item->attr);
        vdata += valuesize + attrsize;
#endif
        count += 1;
    }
    result->count     = count;
    result->valuesize = valuesize;
    result->attrsize  = attrsize;
    result->root      = root;
*/
    return MEMLINK_OK;
}

// kind: short to int  modified by wyx at 1.4
int 
memlink_cmd_range(MemLink *m, char *table,char *key, int kind,  char *attrstr, 
                  int frompos, int len, MemLinkResult *result)
{
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;
    if (len <= 0 || frompos < 0)
        return MEMLINK_ERR_PARAM;
    if (MEMLINK_VALUE_ALL != kind && MEMLINK_VALUE_VISIBLE != kind && MEMLINK_VALUE_TAGDEL != kind)
        return MEMLINK_ERR_PARAM;
    
    char data[1024] = {0};
    int  plen;
    uint32_t attrarray[HASHTABLE_ATTR_MAX_ITEM] = {0};
    int attrn = 0;

    attrn = attr_string2array(attrstr, attrarray);
    //DINFO("range attr len: %d\n", attrn);
    if (attrn < 0 || attrn > HASHTABLE_ATTR_MAX_ITEM)
        return MEMLINK_ERR_PARAM;
    
    plen = cmd_range_pack(data, table,key, kind, attrn, attrarray, frompos, len);
    //DINFO("pack range len: %d\n", plen);

    // len(4B) + retcode(2B) + valuesize(1B) + attrsize(1B) + attrnum(1B) + attrformat(attrnum B) + value.attr * len
    int retlen = CMD_REPLY_HEAD_LEN + 3 + HASHTABLE_ATTR_MAX_ITEM +  \
                (HASHTABLE_VALUE_MAX + HASHTABLE_ATTR_MAX_BYTE * HASHTABLE_ATTR_MAX_ITEM + 1) * len;
    //DINFO("retlen: %d\n", retlen);
    if (retlen > 1024000) { // do not more than 1M
        return MEMLINK_ERR_PACKAGE_SIZE;
    }
    //char retdata[retlen];
    int y = retlen / 4096;
    int n = retlen % 4096; 
    char *retdata = (char *)zz_malloc(y * 4096 + (n > 0 ? 1: 0) * 4096);

    int ret = memlink_do_cmd(m, MEMLINK_READER, data, plen, retdata, retlen);
    //DINFO("memlink_do_cmd: %d\n", ret);
    if (ret <= 0) {
        DERROR("memlink_do_cmd error: %d\n", ret);
        zz_free(retdata);
        return ret;
    }

    memlink_result_parse(retdata, result);
    zz_free(retdata);
    return MEMLINK_OK;
}

int 
memlink_cmd_sortlist_range(MemLink *m, char *table, char *key, int kind,  char *attrstr, 
                  char *valmin, int vminlen, 
                  char *valmax, int vmaxlen, MemLinkResult *result)
{
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;
    if (MEMLINK_VALUE_ALL != kind && MEMLINK_VALUE_VISIBLE != kind && MEMLINK_VALUE_TAGDEL != kind)
        return MEMLINK_ERR_PARAM;
    
    char data[1024] = {0};
    int  plen;
    uint32_t attrarray[HASHTABLE_ATTR_MAX_ITEM] = {0};
    int attrn = 0;

    attrn = attr_string2array(attrstr, attrarray);
    //DINFO("range attr len: %d\n", attrn);
    if (attrn < 0 || attrn > HASHTABLE_ATTR_MAX_ITEM)
        return MEMLINK_ERR_PARAM;

    plen = cmd_sortlist_range_pack(data, table, key, kind, attrn, attrarray, valmin, vminlen, valmax, vmaxlen);
    //DINFO("pack range len: %d\n", plen);

    // len(4B) + retcode(2B) + valuesize(1B) + attrsize(1B) + attrnum(1B) + attrformat(attrnum B) + value.attr * len

    int retlen = CMD_REPLY_HEAD_LEN + 3 + HASHTABLE_ATTR_MAX_ITEM +  \
                (HASHTABLE_VALUE_MAX + HASHTABLE_ATTR_MAX_BYTE * HASHTABLE_ATTR_MAX_ITEM + 1) * 1000;
    //DINFO("retlen: %d\n", retlen);
    if (retlen > 1024000) {
        return MEMLINK_ERR_PACKAGE_SIZE;
    }
    
    int y = retlen / 4096;
    int n = retlen % 4096; 
    char *retdata = (char *)zz_malloc(y * 4096 + (n > 0 ? 1: 0) * 4096);
    //char retdata[retlen];

    int ret = memlink_do_cmd(m, MEMLINK_READER, data, plen, retdata, retlen);
    //DINFO("memlink_do_cmd: %d\n", ret);
    if (ret <= 0) {
        DERROR("memlink_do_cmd error: %d\n", ret);
        zz_free(retdata);
        return ret;
    }

    memlink_result_parse(retdata, result);

    zz_free(retdata);
    return MEMLINK_OK;
}

/*int
memlink_cmd_insert_mvalue(MemLink *m, char *key, MemLinkInsertVal *values, int num)
{
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;

    int  len;
    int  pkgsize = sizeof(int) + sizeof(char) + 256;
    int  i;
    char *data;

    MemLinkInsertVal    *item; 
    for (i = 0; i < num; i++) {
        item = &values[i];
        item->attrnum = attr_string2array(item->attrstr, item->attrarray);
        pkgsize += item->valuelen + sizeof(char) + item->attrnum * sizeof(int) + sizeof(char) + sizeof(int);
    }
    
    if (pkgsize > 1024000) {
        data = (char *)zz_malloc(pkgsize); 
        if (NULL == data) {
            DERROR("malloc pkgsize:%d error!\n", pkgsize);
            MEMLINK_EXIT;
        }
    }else{
        data = (char *)alloca(pkgsize);
    }

    len = cmd_insert_mvalue_pack(data, key, values, num);
    //DINFO("pack del len: %d\n", len);

    char retdata[1024] = {0};
    int ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, 1024);

    if (pkgsize > 1024000) {
        zz_free(data);
    }

    if (ret < 0) 
        return ret;
    return MEMLINK_OK;
}*/

int 
memlink_cmd_lpush(MemLink *m, char *table,char *key, char *value, int valuelen, char *attrstr)
{
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;
    if (valuelen <= 0 || valuelen > HASHTABLE_VALUE_MAX)
        return MEMLINK_ERR_PARAM;
    
    char data[1024] = {0};
    int  len;
    uint32_t attrarray[HASHTABLE_ATTR_MAX_ITEM] = {0};
    int attrnum = 0;

    attrnum = attr_string2array(attrstr, attrarray);
    //DINFO("insert attr len: %d\n", attrnum);
    if (attrnum < 0 || attrnum > HASHTABLE_ATTR_MAX_ITEM)
        return MEMLINK_ERR_PARAM;

    len = cmd_lpush_pack(data, table, key, value, valuelen, attrnum, attrarray);
    //DINFO("pack lpush len: %d\n", len);

    char retdata[1024] = {0};
    int ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, 1024);

    if (ret < 0) 
        return ret;
    return MEMLINK_OK;
}

int 
memlink_cmd_rpush(MemLink *m, char *table, char *key, char *value, int valuelen, char *attrstr)
{
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;
    if (valuelen <= 0 || valuelen > HASHTABLE_VALUE_MAX)
        return MEMLINK_ERR_PARAM;
    
    char data[1024] = {0};
    int  len;
    uint32_t attrarray[HASHTABLE_ATTR_MAX_ITEM] = {0};
    int attrnum = 0;

    attrnum = attr_string2array(attrstr, attrarray);
    //DINFO("insert attr len: %d\n", attrnum);
    if (attrnum < 0 || attrnum > HASHTABLE_ATTR_MAX_ITEM)
        return MEMLINK_ERR_PARAM;

    len = cmd_rpush_pack(data, table, key, value, valuelen, attrnum, attrarray);
    //DINFO("pack rpush len: %d\n", len);

    char retdata[1024] = {0};
    int ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, 1024);

    if (ret < 0) 
        return ret;
    return MEMLINK_OK;

}

int 
memlink_cmd_lpop(MemLink *m, char *table, char *key, int num, MemLinkResult *result)
{
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;
    
    char data[1024] = {0};
    int  len;

    len = cmd_lpop_pack(data, table, key, num);
    //DINFO("pack lpop len: %d\n", len);

    // len(4B) + retcode(2B) + valuesize(1B) + attrsize(1B) + attrnum(1B) + attrformat(attrnum B) + value.attr * len
    int retlen = CMD_REPLY_HEAD_LEN + 3 + HASHTABLE_ATTR_MAX_ITEM + \
                 (HASHTABLE_VALUE_MAX + HASHTABLE_ATTR_MAX_BYTE * HASHTABLE_ATTR_MAX_ITEM + 1) * num;
    if (retlen > 1024000) { // do not more than 1M
        return MEMLINK_ERR_PACKAGE_SIZE;
    }
    char retdata[retlen];

    int ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, retlen);
    if (ret <= 0) {
        DERROR("memlink_do_cmd error: %d\n", ret);
        return ret;
    }

    memlink_result_parse(retdata, result);

    return MEMLINK_OK;
}

int 
memlink_cmd_rpop(MemLink *m, char *table, char *key, int num, MemLinkResult *result)
{
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;
    
    char data[1024] = {0};
    int  len;

    len = cmd_rpop_pack(data, table, key, num);
    //DINFO("pack rpop len: %d\n", len);

    // len(4B) + retcode(2B) + valuesize(1B) + attrsize(1B) + attrnum(1B) + attrformat(attrnum B) + value.attr * len
    int retlen = CMD_REPLY_HEAD_LEN + 3 + HASHTABLE_ATTR_MAX_ITEM + \
                 (HASHTABLE_VALUE_MAX + HASHTABLE_ATTR_MAX_BYTE * HASHTABLE_ATTR_MAX_ITEM + 1) * num;
    if (retlen > 1024000) { // do not more than 1M
        return MEMLINK_ERR_PACKAGE_SIZE;
    }
    char retdata[retlen];

    int ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, retlen);
    if (ret <= 0) {
        DERROR("memlink_do_cmd error: %d\n", ret);
        return ret;
    }

    memlink_result_parse(retdata, result);

    return MEMLINK_OK;
}


void
memlink_close(MemLink *m)
{
    if (m->readfd > 0) {
        close(m->readfd);
        m->readfd = -1;
    }
    if (m->writefd > 0) {
        close(m->writefd);
        m->writefd = -1;
    }
}


void
memlink_destroy(MemLink *m)
{
    memlink_close(m);

    zz_free(m);
}


void
memlink_result_free(MemLinkResult *result)
{
    //MemLinkItem *item, *tmp;

    //DINFO("free result ...\n");
    if (NULL == result)
        return;

    zz_free(result->items);
    result->items = NULL;

    /*
    item = result->root;
    while (item) {
        tmp = item;
        item = item->next;
        zz_free(tmp);
    }
    result->root = NULL;
    */
}

//add by lanwenhong
int
memlink_cmd_del_by_attr(MemLink *m, char *table, char *key, char *attrstr)
{
    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX)
        return MEMLINK_ERR_PARAM;

    char data[1024] = {0};
    char retdata[1024] = {0};
    uint32_t attrarray[HASHTABLE_ATTR_MAX_ITEM] = {0};
    int attrnum;
    int len;
    int ret;

    attrnum = attr_string2array(attrstr, attrarray);
    //DINFO("attrnum: %d\n", attrnum);
    if (attrnum < 0 || attrnum > HASHTABLE_ATTR_MAX_ITEM)
        return MEMLINK_ERR_PARAM;
    
    len = cmd_del_by_attr_pack(data, table,key, attrarray, attrnum);
    //DINFO("pack del by attr len: %d\n", len);

    ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, 1024);
    if (ret < 0)
        return ret;

    return MEMLINK_OK;

}

MemLinkInsertMkv*
memlink_imkv_create()
{
    MemLinkInsertMkv *mkv = NULL;

    mkv = (MemLinkInsertMkv *)zz_malloc(sizeof(MemLinkInsertMkv));
    if (mkv == NULL) {
        DERROR("malloc MemLinkInsertMkv error!\n");
        return NULL;
    }
    memset(mkv, 0x0, sizeof(MemLinkInsertMkv));
    
    mkv->sum_size += sizeof(int);//package_len
    mkv->sum_size += sizeof(char); //cmd
    mkv->sum_size += sizeof(int); //key nums
    return mkv;
}

MemLinkInsertKey*
memlink_ikey_create(char *table,char *key, int keylen)
{
    MemLinkInsertKey *kobj = NULL;

    if (NULL == key || strlen(key) > HASHTABLE_KEY_MAX) {
        DERROR("param error!\n");
        return NULL;
    }
    kobj = (MemLinkInsertKey *)zz_malloc(sizeof(MemLinkInsertKey));
    if (kobj == NULL) {
        DERROR("malloc MemLinkInsertKey error!\n");
        return NULL;
    }
    memset(kobj, 0x0, sizeof(MemLinkInsertKey));
    snprintf(kobj->key,255,"%s.%s",table,key);
    //memcpy(kobj->key, key, keylen);
    kobj->keylen = keylen;

    return kobj;
}

MemLinkInsertVal*
memlink_ival_create(char *value, int valuelen, char *attrstr, int pos)
{
    MemLinkInsertVal *valobj = NULL;
    int attrnum = 0;

    if (valuelen <= 0 || valuelen > HASHTABLE_VALUE_MAX) {
        DERROR("param error!\n");
        return NULL;
    }
    /*
    if (pos < -1) {
        DERROR("param error!\n");
        return NULL;
    }
    */
    
    valobj = (MemLinkInsertVal *)zz_malloc(sizeof(MemLinkInsertVal));
    if (valobj == NULL) {
        DERROR("malloc MemlinkInsertVal error!\n");
        return NULL;
    }
    memset(valobj, 0x0, sizeof(MemLinkInsertVal));

    memcpy(valobj->value, value, valuelen);
    valobj->valuelen = valuelen;
    memcpy(valobj->attrstr, attrstr, strlen(attrstr));
    attrnum = attr_string2array(valobj->attrstr, valobj->attrarray);
    //DINFO("insert attr len: %d\n", attrnum);
    if (attrnum < 0 || attrnum > HASHTABLE_ATTR_MAX_ITEM) {
        zz_free(valobj);
        return NULL;
    }
    valobj->attrnum = attrnum;
    valobj->pos = pos;

    return valobj;
}

int
memlink_imkv_destroy(MemLinkInsertMkv *mkv)
{
    MemLinkInsertKey *keyitem;
    MemLinkInsertVal *valitem;

    if (mkv == NULL) {
        return MEMLINK_ERR_PARAM;
    }
    while (mkv->keylist!= NULL) {
        keyitem = mkv->keylist;
        while (keyitem->vallist != NULL) {
            valitem = keyitem->vallist;
            keyitem->vallist = valitem->next;
            zz_free(valitem);
        }
        mkv->keylist = keyitem->next;
        zz_free(keyitem);
    }
    zz_free(mkv);
    return 0;
}

int
memlink_imkv_add_key(MemLinkInsertMkv *mkv, MemLinkInsertKey *keyobj)
{
    uint32_t key_size = 0;

    if (mkv == NULL || keyobj == NULL)
        return MEMLINK_ERR_PARAM;

    key_size += sizeof(char);//key size
    key_size += keyobj->keylen; //key
    key_size += sizeof(int);//how match value in key

    if (mkv->tail != NULL) {
        keyobj->next  = mkv->tail->next;
        mkv->tail->next = keyobj;
        mkv->tail = keyobj;
    } else {
        mkv->keylist = keyobj;
        mkv->tail = keyobj;
    }
    mkv->keynum++;
    mkv->sum_size += (key_size + keyobj->sum_val_size);
    return 0;
}

int
memlink_ikey_add_value(MemLinkInsertKey *keyobj, MemLinkInsertVal *valobj)
{
    uint32_t val_size = 0;//该value编码到包里面一共要占用多少字节

    if (keyobj == NULL || valobj == NULL)
        return MEMLINK_ERR_PARAM;

    if (keyobj->vallist == NULL) {
        keyobj->vallist = valobj;
        keyobj->tail = valobj;
    } else {
        keyobj->tail->next = valobj;
        keyobj->tail = valobj;
    }
    val_size += sizeof(char); //value len
    val_size += valobj->valuelen;//value
    val_size += sizeof(char);//attr nums
    val_size += valobj->attrnum * sizeof(int);//attr
    val_size += sizeof(int);//pos
    keyobj->sum_val_size += val_size;
    keyobj->valnum++;

    return 0;
}

int
memlink_cmd_insert_mkv(MemLink *m, MemLinkInsertMkv *mkv)
{
    char data[MAX_PACKAGE_LEN];
    int len;
    int package_len;

    if (m == NULL || mkv == NULL)
        return MEMLINK_ERR_PARAM;
    if (mkv->sum_size > MAX_PACKAGE_LEN) {
        //*kindex = 0;
        //*vindex = 0;
        return MEMLINK_ERR_PACKAGE_SIZE;
    }

    len = cmd_insert_mkv_pack(data, mkv);
    memcpy(&package_len, data, sizeof(int));
    //printf("pack insert mkv len: %d\n", package_len);
    char retdata[1024];
    int ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len , retdata, 1024);
    int recodelen;
    int i,j;
    int size = 0;
    memcpy(&recodelen, retdata, sizeof(int));
    //printf("recodelen: %d\n", recodelen);
    size += sizeof(int);
    size += sizeof(short);
    memcpy(&i, retdata + size, sizeof(int));
    size += sizeof(int);
    memcpy(&j, retdata + size, sizeof(int));
    mkv->kindex = i;
    mkv->vindex = j;

    if (ret < 0)
        return ret;
    return MEMLINK_OK;
}

int memlink_cmd_read_conn_info(MemLink *m, MemLinkRcInfo *rcinfo)
{
    char data[1024];
    int len;
    short ret;
    int psize;
    int count = 0;

    len = cmd_read_conn_info_pack(data);
    int retlen = 1000 * sizeof(MemLinkRcItem);
    char retdata[retlen];

    rcinfo->count = 0;
    rcinfo->items = NULL;
    ret = memlink_do_cmd(m, MEMLINK_READER, data, len, retdata, retlen);
    if (ret <= 0) {
        DERROR("memlink_do cmd error: %d\n", ret);
        return ret;
    }
    //获取包的大小
    memcpy(&psize, retdata, sizeof(int));
    memcpy(&ret, retdata + sizeof(int), sizeof(short));
    if (ret != MEMLINK_OK)
        return ret;
    char *rdata = retdata + CMD_REPLY_HEAD_LEN;

    while (count < psize - sizeof(short)) {
        MemLinkRcItem *item = zz_malloc(sizeof(MemLinkRcItem));
        memset(item, 0x0, sizeof(MemLinkRcItem));
        memcpy(&item->fd, rdata + count, sizeof(int));
        count += sizeof(int);
        uint8_t clen;
        memcpy(&clen, rdata + count, sizeof(char));
        count += sizeof(char);
        memcpy(item->client_ip, rdata + count, clen);
        count += clen;
        memcpy(&item->port, rdata + count, sizeof(int));
        count += sizeof(int);
        memcpy(&item->conn_time, rdata + count, sizeof(int));
        count += sizeof(int);
        memcpy(&item->cmd_count, rdata + count, sizeof(int));
        count += sizeof(int);
        item->next = rcinfo->items;
        rcinfo->items = item;
        rcinfo->count++;
    }
    return MEMLINK_OK;
}

int memlink_cmd_write_conn_info(MemLink *m, MemLinkWcInfo *wcinfo)
{
    char data[1024];
    int len;
    int psize;
    int count = 0;
    short ret;

    len = cmd_write_conn_info_pack(data);
    int retlen = 1000 * sizeof(MemLinkRcItem);
    char retdata[retlen];

    wcinfo->count = 0;
    wcinfo->items = NULL;
    ret = memlink_do_cmd(m, MEMLINK_READER, data, len, retdata, retlen);
    if (ret <= 0) {
        DERROR("memlink_do_cmd error: %d\n", ret);
        return ret;
    }
    //获取包的大小
    memcpy(&psize, retdata, sizeof(int));
    memcpy(&ret, retdata + sizeof(int), sizeof(short));
    if (ret != MEMLINK_OK)
        return ret;
    char *rdata = retdata + CMD_REPLY_HEAD_LEN;

    while (count < psize - sizeof(short)) {
        MemLinkWcItem *item = zz_malloc(sizeof(MemLinkWcItem));
        memset(item, 0x0, sizeof(MemLinkWcItem));
        memcpy(&item->fd, rdata + count, sizeof(int));
        count += sizeof(int);
        uint8_t clen;
        memcpy(&clen, rdata + count, sizeof(char));
        count += sizeof(char);
        memcpy(item->client_ip, rdata + count, clen);
        count += clen;
        memcpy(&item->port, rdata + count, sizeof(int));
        count += sizeof(int);
        memcpy(&item->conn_time, rdata + count, sizeof(int));
        count += sizeof(int);
        memcpy(&item->cmd_count, rdata + count, sizeof(int));
        count += sizeof(int);
        item->next = wcinfo->items;
        wcinfo->items = item;
        wcinfo->count++;
    }
    return MEMLINK_OK;
}

int memlink_cmd_sync_conn_info(MemLink *m, MemLinkScInfo *scinfo)
{
    char data[1024];
    int len;
    int psize;
    int count = 0;
    short ret;

    len = cmd_sync_conn_info_pack(data);
    int retlen = 1000 * sizeof(MemLinkScItem);
    char retdata[retlen];

    scinfo->count = 0;
    scinfo->items = NULL;
    ret = memlink_do_cmd(m, MEMLINK_READER, data, len, retdata, retlen);
    if (ret <= 0) {
        DERROR("memlink_do_cmd error: %d\n", ret);
        return ret;
    }
    memcpy(&psize, retdata, sizeof(int));
    memcpy(&ret, retdata + sizeof(int), sizeof(short));
    if (ret != MEMLINK_OK)
        return ret;
    char *sdata = retdata + CMD_REPLY_HEAD_LEN;

    while (count < psize - sizeof(short)) {
        MemLinkScItem *item = zz_malloc(sizeof(MemLinkScItem));
        memset(item, 0x0, sizeof(MemLinkScItem));
        memcpy(&item->fd, sdata + count, sizeof(int));
        count += sizeof(int);
        uint8_t clen;
        memcpy(&clen, sdata + count, sizeof(char));
        count += sizeof(char);
        memcpy(item->client_ip, sdata + count, clen);
        count += clen;
        memcpy(&item->port , sdata + count, sizeof(int));
        count += sizeof(int);
        memcpy(&item->conn_time, sdata + count, sizeof(int));
        count += sizeof(int);
        memcpy(&item->cmd_count, sdata + count, sizeof(int));
        count += sizeof(int);
        memcpy(&item->logver, sdata + count, sizeof(int));
        count += sizeof(int);
        memcpy(&item->logline, sdata + count, sizeof(int));
        count += sizeof(int);
        memcpy(&item->delay, sdata + count, sizeof(int));
        count += sizeof(int);
        item->next = scinfo->items;
        scinfo->items = item;
        scinfo->count++;

    }
    return MEMLINK_OK;
}

int
memlink_rcinfo_free(MemLinkRcInfo *info)
{
    MemLinkRcItem *tmp;
    
    if (info == NULL)
        return -1;
    while (info->items) {
        tmp = info->items;
        info->items = tmp->next;
        zz_free(tmp);
    }
    return 0;
}

int
memlink_wcinfo_free(MemLinkWcInfo *info)
{
    MemLinkWcItem *tmp;
    
    if (info == NULL)
        return -1;
    while (info->items) {
        tmp = info->items;
        info->items = tmp->next;
        zz_free(tmp);
    }
    return 0;
}

int
memlink_scinfo_free(MemLinkScInfo *info)
{
    MemLinkScItem *tmp;
    
    if (info == NULL)
        return -1;
    while(info->items) {
        tmp = info->items;
        info->items = tmp->next;
        zz_free(tmp);
    }
    return 0;
}

int
memlink_cmd_get_config_info(MemLink *m, MyConfig *config)
{
    char data[1024];
    int len;
    int psize;
    int count = 0;
    short ret;

    len = cmd_config_info_pack(data);
    int n = sizeof(MyConfig) / 4096;
    int y = sizeof(MyConfig) % 4096;

    int retlen = n * 4096 + (y > 1 ? 1: 0) * 4096;
    char retdata[retlen];
    
    ret = memlink_do_cmd(m, MEMLINK_READER, data, len, retdata, retlen);
    if (ret < 0) {
        DERROR("memlink_do_cmd error: %d\n", ret);
        return ret;
    }
    memcpy(&psize, retdata, sizeof(int));
    memcpy(&ret, retdata + sizeof(int), sizeof(short));
    if (ret != MEMLINK_OK)
        return ret;

    char *rdata = retdata + CMD_REPLY_HEAD_LEN;
    count = unpack_config_struct(rdata, config);
    if (count + sizeof(short) != psize) {
        return MEMLINK_ERR_PACKAGE_SIZE;
    }
    return MEMLINK_OK;
}

int
memlink_cmd_set_config_info(MemLink *m, char *key, char *value)
{
    char data [1024];
    int  len;
    short ret;

    if (key == NULL || value == NULL) {
        return MEMLINK_ERR_PARAM;
    }
    if (strcmp(key, "block_data_reduce") == 0 || strcmp(key, "block_clean_cond") == 0 ||
        strcmp(key, "block_clean_start") == 0 || strcmp(key, "sync_interval") == 0 ||
        strcmp(key, "block_clean_num") == 0 || strcmp(key, "timeout") == 0 || 
        strcmp(key, "log_level") == 0 || strcmp(key, "master_sync_host") == 0 || 
        strcmp(key, "master_sync_port") == 0 || strcmp(key, "role") == 0){
        len = cmd_set_config_dynamic_pack(data, key, value);
    } else {
        return MEMLINK_ERR_PARAM;
    }
    char retdata[1024] = {0};
    
    ret = memlink_do_cmd(m, MEMLINK_WRITER, data, len, retdata, 1024);
    
    if (ret < 0)
        return ret;

    return MEMLINK_OK;
}

