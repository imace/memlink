#ifndef MEMLINK_SERIAL_H
#define MEMLINK_SERIAL_H

#include <stdio.h>
#include <stdint.h>
#include "common.h"
#include "info.h"
#include "myconfig.h"

#define CMD_DUMP		    1
#define CMD_CLEAN		    2
#define CMD_STAT		    3
#define CMD_CREATE_TABLE    4
#define CMD_CREATE_NODE     5
#define CMD_RMTABLE			6
#define CMD_DEL			    7
#define CMD_INSERT		    8
#define CMD_MOVE		    9
#define CMD_ATTR		    10
#define CMD_TAG			    11
#define CMD_RANGE		    12
#define CMD_RMKEY           13
#define CMD_COUNT		    14
#define CMD_LPUSH		    15
#define CMD_LPOP		    16
#define CMD_RPUSH		    17
#define CMD_RPOP		    18

//add by lanwenhong
#define CMD_DEL_BY_ATTR     19

#define CMD_PING			20
#define CMD_STAT_SYS		21
// for sorted list
#define CMD_SL_INSERT       22
#define CMD_SL_DEL          23
#define CMD_SL_COUNT        24
#define CMD_SL_RANGE        25

#define CMD_INSERT_MKV      26 
#define CMD_READ_CONN_INFO  27
#define CMD_WRITE_CONN_INFO 28
#define CMD_SYNC_CONN_INFO  29
#define CMD_CONFIG_INFO     30

#define CMD_SET_CONFIG      31

#define CMD_CLEAN_ALL       32
#define CMD_TABLES			33

#define CMD_WRITE			100
#define CMD_WRITE_RESULT	101
#define CMD_GETPORT			102
#define CMD_BACKUP_ACK      103
#define CMD_HEARTBEAT       104

#define CMD_SYNC            150
#define CMD_GETDUMP		    151

#define CMD_VOTE_MIN		200
#define CMD_VOTE            200
#define CMD_VOTE_WAIT       201
#define CMD_VOTE_MASTER     202
#define CMD_VOTE_BACKUP     203
#define CMD_VOTE_NONEED     204
#define CMD_VOTE_UPDATE     205
#define CMD_VOTE_DETECT     206
#define CMD_VOTE_MAX		206

#define cmd_lpush_unpack    cmd_push_unpack
#define cmd_rpush_unpack    cmd_push_unpack

#define cmd_lpop_unpack     cmd_pop_unpack
#define cmd_rpop_unpack     cmd_pop_unpack

int attr_string2array(char *attrstr, uint32_t *result);
int attr_array2binary(uint8_t *attrformat, uint32_t *attrarray, char attrnum, char *attr);
int attr_string2binary(uint8_t *attrformat, char *attrstr, char *attr);
int attr_binary2string(uint8_t *attrformat, int attrnum, char *attr, int attrlen, char *attrstr);
int attr_binary2array(uint8_t *attrformat, int attrnum, char *attr, uint32_t *attrarray);
int attr_array2flag(uint8_t *attrformat, uint32_t *attrarray, char attrnum, char *attr);

int cmd_dump_pack(char *data);
int cmd_dump_unpack(char *data);

int cmd_clean_pack(char *data, char *table, char *key);
int cmd_clean_unpack(char *data, char *table, char *key);

int cmd_rmkey_pack(char *data, char *table, char *key);
int cmd_rmkey_unpack(char *data, char *table, char *key);

int cmd_rmtable_pack(char *data, char *table);
int cmd_rmtable_unpack(char *data, char *table);

int cmd_tables_pack(char *data);
int cmd_tables_unpack(char *data);

int cmd_count_pack(char *data, char *table, char *key, uint8_t attrnum, uint32_t *attrarray);
int cmd_count_unpack(char *data, char *table, char *key, uint8_t *attrnum, uint32_t *attrarray);

int cmd_stat_pack(char *data, char *table, char *key);
int cmd_stat_unpack(char *data, char *table, char *key);

int cmd_stat_sys_pack(char *data);
int cmd_stat_sys_unpack(char *data);

int cmd_create_table_pack(char *data, char *table, uint8_t valuelen, 
                    uint8_t attrnum, uint32_t *attrformat,
                    uint8_t listtype, uint8_t valuetype);
int cmd_create_table_unpack(char *data, char *table, uint8_t *valuelen, 
                      uint8_t *attrnum, uint32_t *attrformat,
                      uint8_t *listtype, uint8_t *valuetype);

int cmd_create_node_pack(char *data, char *table, char *key);
int cmd_create_node_unpack(char *data, char *table, char *key);

int cmd_del_pack(char *data, char *table, char *key, char *value, uint8_t valuelen);
int cmd_del_unpack(char *data, char *table, char *key, char *value, uint8_t *valuelen);

int cmd_insert_pack(char *data, char *table, char *key, char *value, uint8_t valuelen, 
                    uint8_t attrnum, uint32_t *attrarray, int pos);  
int cmd_insert_unpack(char *data, char *table, char *key, char *value, uint8_t *valuelen,
                      uint8_t *attrnum, uint32_t *attrarray, int *pos);

int cmd_move_pack(char *data, char *table, char *key, char *value, uint8_t valuelen, int pos);
int cmd_move_unpack(char *data, char *table, char *key, char *value, uint8_t *valuelen, int *pos);

int cmd_attr_pack(char *data, char *table, char *key, char *value, uint8_t valuelen, 
                  uint8_t attrnum, uint32_t *attrarray);
int cmd_attr_unpack(char *data, char *table, char *key, char *value, uint8_t *valuelen, 
                    uint8_t *attrnum, uint32_t *attrarray);

int cmd_tag_pack(char *data, char *table, char *key, char *value, uint8_t valuelen, uint8_t tag);
int cmd_tag_unpack(char *data, char *table, char *key, char *value, uint8_t *valuelen, uint8_t *tag);

int cmd_range_pack(char *data, char *table, char *key, uint8_t kind, uint8_t attrnum, uint32_t *attrarray, 
                   int frompos, int len);
int cmd_range_unpack(char *data, char *table, char *key, uint8_t *kind, uint8_t *attrnum, uint32_t*attrarray, 
                     int *frompos, int *len);

int cmd_ping_pack(char *data);
int cmd_ping_unpack(char *data);

int cmd_push_pack(char *data, uint8_t cmd, char *table, char *key, char *value, uint8_t valuelen, 
                    uint8_t attrnum, uint32_t *attrarray);
int cmd_lpush_pack(char *data, char *table, char *key, char *value, uint8_t valuelen, 
                    uint8_t attrnum, uint32_t *attrarray);
int cmd_rpush_pack(char *data, char *table, char *key, char *value, uint8_t valuelen, 
                    uint8_t attrnum, uint32_t *attrarray);
int cmd_push_unpack(char *data, char *table, char *key, char *value, uint8_t *valuelen,
                    uint8_t *attrnum, uint32_t *attrarray);

int cmd_pop_pack(char *data, uint8_t cmd, char *table, char *key, int num);
int cmd_lpop_pack(char *data, char *table, char *key, int num);
int cmd_rpop_pack(char *data, char *table, char *key, int num);
int cmd_pop_unpack(char *data, char *table, char *key, int *num);

// for sync client
int cmd_sync_pack(char *data, uint32_t logver, uint32_t logpos, int bcount, char *md5);
int cmd_sync_unpack(char *data, uint32_t *logver, uint32_t *logpos, int *bcount, char *md5);

int cmd_getdump_pack(char *data, uint32_t dumpver, uint64_t size);
int cmd_getdump_unpack(char *data, uint32_t *dumpver, uint64_t *size);

//int cmd_insert_mvalue_pack(char *data, char *key, MemLinkInsertVal *items, int num);
//int cmd_insert_mvalue_unpack(char *data, char *key, MemLinkInsertVal **items, int *num);

//add by lanwenhong
int cmd_del_by_attr_pack(char *data, char *table,char *key, uint32_t *attrarray, uint8_t attrnum);
int cmd_del_by_attr_unpack(char *data, char *table, char *key, uint32_t *attrarray, uint8_t *attrnum);

int cmd_sortlist_count_pack(char *data, char * table, char *key, uint8_t attrnum, uint32_t *attrarray,
                        void *valmin, uint8_t vminlen, void *valmax, uint8_t vmaxlen);
int cmd_sortlist_count_unpack(char *data, char *table, char *key, uint8_t *attrnum, uint32_t *attrarray,
                        void *valmin, uint8_t *vminlen, void *valmax, uint8_t *vmaxlen);
int cmd_sortlist_del_pack(char *data, char *table, char *key, uint8_t kind, char *valmin, uint8_t vminlen, 
                        char *valmax, uint8_t vmaxlen, uint8_t attrnum, uint32_t *attrarray);
int cmd_sortlist_del_unpack(char *data, char *table, char *key, uint8_t *kind, char *valmin, uint8_t *vminlen,
                        char *valmax, uint8_t *vmaxlen, uint8_t *attrnum, uint32_t *attrarray);

int cmd_sortlist_range_pack(char *data, char *table, char *key, uint8_t kind, 
                        uint8_t attrnum, uint32_t *attrarray, 
                        void *valmin, uint8_t vminlen, void *valmax, uint8_t vmaxlen);
int cmd_sortlist_range_unpack(char *data, char *table, char *key, uint8_t *kind, 
                        uint8_t *attrnum, uint32_t *attrarray, 
                        void *valmin, uint8_t *vminlen, void *valmax, uint8_t *vmaxlen);

int cmd_insert_mkv_pack(char *data, MemLinkInsertMkv *mkv);
int cmd_insert_mkv_unpack_packagelen(char *data, uint32_t *package_len);
int cmd_insert_mkv_unpack_key(char *data, char *table, char *key, uint32_t *valcount, char **conutstart);
int cmd_insert_mkv_unpack_val(char *data, char *value, uint8_t *valuelen, uint8_t *attrnum, uint32_t *attrarray, int *pos);
int cmd_read_conn_info_pack(char *data);
int cmd_read_conn_info_unpack(char *data);
int cmd_write_conn_info_pack(char *data);
int cmd_write_conn_info_unpack(char *data);
int cmd_sync_conn_info_pack(char *data);
int cmd_sync_conn_info_unpack(char *data);

int cmd_config_info_pack(char *data);
int cmd_config_info_unpack(char *data);
int pack_config_struct(char *data, MyConfig *config);
int unpack_config_struct(char *data, MyConfig *config);
int cmd_set_config_dynamic_pack(char *data, char *key, char *value);
int cmd_set_config_dynamic_unpack(char *data, char *key, char *value);
int cmd_clean_all_pack(char *data);
int cmd_clean_all_unpack(char *data);
int cmd_vote_pack(char *data, uint64_t id, uint8_t result, uint64_t voteid, uint16_t port);
int cmd_heartbeat_pack(char *data, int port);
int cmd_heartbeat_unpack(char *data, int *port);
int cmd_backup_ack_pack(char *data, char state);
int cmd_backup_ack_unpack(char *data, uint8_t *cmd, short *ret, int *logver, int *logline);
int cmd_vote_pack(char *data, uint64_t id, uint8_t result, uint64_t voteid, uint16_t port);
int unpack_votehost(char *buf, char *ip, uint16_t *port);
int unpack_voteid(char *buf, uint64_t *vote_id);


#endif
