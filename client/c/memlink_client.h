#ifndef MEMLINK_CLIENT_H
#define MEMLINK_CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "common.h"
#include "myconfig.h"

#define MEMLINK_READER  1
#define MEMLINK_WRITER  2
#define MEMLINK_ALL     3

#define MAX_PACKAGE_LEN 1024000

typedef struct _memlink_client
{
    char    host[16];
    int     read_port;
    int     write_port;
    int     readfd;
    int     writefd;
    int     timeout;
	int		auto_reconn;
}MemLink;

typedef struct _memlink_count
{
	unsigned int	visible_count;
	unsigned int	tagdel_count;
}MemLinkCount;

typedef struct _memlink_item
{
	struct _memlink_item *next;	
    char    value[256];
	unsigned short	valuesize;
    char    attr[256];
	unsigned char	attrsize;
}MemLinkItem;

typedef struct _memlink_result
{
    int         count;
    int         valuesize;
    int         attrsize;
    MemLinkItem *items;
}MemLinkResult;

typedef struct _memlink_rconn_item
{
	int     fd;
	char    client_ip[16];
	int     port;
	int     conn_time;
	int     cmd_count;
	struct _memlink_rconn_item *next;
}MemLinkRcItem;

typedef struct _memlink_wconn_item
{
	int     fd;
	char    client_ip[16];
	int     port;
	int     cmd_count;
	unsigned int    conn_time;
	struct _memlink_wconn_item *next;
}MemLinkWcItem;

typedef struct _memlink_sconn_item
{
	int     fd;
	char	client_ip[16];
	int     port;
	int     cmd_count;
	unsigned int    conn_time;
	int     logver;
	int     logline;
	int     delay;
	struct _memlink_sconn_item *next;
}MemLinkScItem;

typedef struct _memlink_rconn_info
{
	int             count;
	MemLinkRcItem   *items;
}MemLinkRcInfo;

typedef struct _memlink_wconn_info
{
	int             count;
	MemLinkWcItem   *items;
}MemLinkWcInfo;

typedef struct _memlink_Sconn_info
{
	int             count;
	MemLinkScItem   *items;
}MemLinkScInfo;

typedef struct
{
	char name[HASHTABLE_TABLE_NAME_SIZE];
	unsigned char  listtype;
	unsigned char  valuetype;
	unsigned short valuesize;
	unsigned char  sortfield;
	unsigned char  attrnum;
	unsigned char  attrsize;
	char attrformat[16];
}MemLinkTableInfo;

int         memlink_result_parse(char *retdata, MemLinkResult *result);
void        memlink_result_free(MemLinkResult *result);

MemLink*    memlink_create(char *host, int readport, int writeport, int timeout);
void        memlink_destroy(MemLink *m);
void		memlink_close(MemLink *m);

int         memlink_cmd_ping(MemLink *m);
int         memlink_cmd_dump(MemLink *m);
int			memlink_cmd_clean(MemLink *m, char *table, char *key);
int         memlink_cmd_clean_all(MemLink *m);
int			memlink_cmd_stat(MemLink *m, char *table, char *key, MemLinkStat *stat);
int			memlink_cmd_stat_sys(MemLink *m, MemLinkStatSys *stat);
int			memlink_cmd_create_table(MemLink *m, char *table, int valuelen, 
						char *attrstr, 
                        unsigned char listtype, unsigned char valuetype);

int			memlink_cmd_create_table_list(MemLink *m, char *table, int valuelen, 
						char *attrstr);
int			memlink_cmd_create_table_queue(MemLink *m, char *table, int valuelen, 
						char *attrstr);
int			memlink_cmd_create_table_sortlist(MemLink *m, char *table, int valuelen, 
						char *attrstr, unsigned char valuetype);
int			memlink_cmd_remove_table(MemLink *m, char *table);
int			memlink_cmd_create_node(MemLink *m, char *table, char *key);
int			memlink_cmd_del(MemLink *m, char *table,char *key, char *value, 
						int valuelen);
int			memlink_cmd_insert(MemLink *m, char *table,char *key, char *value, 
						int valuelen, char *attrstr, int pos);

int			memlink_cmd_lpush(MemLink *m, char *table, char *key, char *value, int valuelen, char *attrstr);
int			memlink_cmd_rpush(MemLink *m, char *table, char *key, char *value, int valuelen, char *attrstr);
int			memlink_cmd_lpop(MemLink *m, char *table, char *key, int num, MemLinkResult *result);
int			memlink_cmd_rpop(MemLink *m, char *table, char *key, int num, MemLinkResult *result);

int			memlink_cmd_move(MemLink *m, char *table, char *key, char *value, int valuelen, int pos);
int			memlink_cmd_attr(MemLink *m, char *table, char *key, char *value, int valuelen, char *attrstr);
int			memlink_cmd_tag(MemLink *m, char *table, char *key, char *value, int valuelen, int tag);
int			memlink_cmd_range(MemLink *m, char *table, char *key, int kind, char *attrstr, 
                              int frompos, int len, MemLinkResult *result);
int         memlink_cmd_rmkey(MemLink *m, char *table, char *key);
int         memlink_cmd_count(MemLink *m, char *table, char *key, char *attrstr, MemLinkCount *count);
//int         memlink_cmd_insert_mvalue(MemLink *m, char *key, MemLinkInsertVal *values, int num);

//add by lanwenhong
int         memlink_cmd_del_by_attr(MemLink *m, char *table, char *key, char *attrstr);

int			memlink_cmd_sortlist_insert(MemLink *m, char *table, char *key, char *value, int valuelen, char *attrstr);
int			memlink_cmd_sortlist_range(MemLink *m, char *table, char *key, int kind, char *attrstr, 
                              char *valmin, int vminlen, 
                              char *valmax, int vmaxlen, MemLinkResult *result);

int			memlink_cmd_sortlist_del(MemLink *m, char *table, char *key, int kind, char *attrstr, 
                            char *valmin, int vminlen, 
                            char *valmax, int vmaxlen);
int         memlink_cmd_sortlist_count(MemLink *m, char *table, char *key, char *attrstr, 
                            char *valmin, int vminlen, 
                            char *valmax, int vmaxlen, MemLinkCount *count);


int         memlink_cmd_insert_mkv(MemLink *m, MemLinkInsertMkv *mkv);
int         memlink_imkv_destroy(MemLinkInsertMkv *mkv);
int         memlink_imkv_add_key(MemLinkInsertMkv *mkv, MemLinkInsertKey *keyobj);
int         memlink_ikey_add_value(MemLinkInsertKey *keyobj, MemLinkInsertVal *valobj);
MemLinkInsertMkv*         memlink_imkv_create(); 
MemLinkInsertKey*         memlink_ikey_create(char *table, char *key, int keylen);
MemLinkInsertVal*         memlink_ival_create(char *value, int valuelen, char *attrstr, int pos);

int memlink_cmd_read_conn_info(MemLink *m, MemLinkRcInfo *rcinfo);
int memlink_cmd_write_conn_info(MemLink *m, MemLinkWcInfo *wcinfo);
int memlink_cmd_sync_conn_info(MemLink *m, MemLinkScInfo *scinfo);

int memlink_rcinfo_free(MemLinkRcInfo *info);
int memlink_wcinfo_free(MemLinkWcInfo *info);
int memlink_scinfo_free(MemLinkScInfo *info);
int memlink_cmd_get_config_info(MemLink *m, MyConfig *config);
int memlink_cmd_set_config_info(MemLink *m, char *key, char *value);
#endif


