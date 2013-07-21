/**
 * 各种宏定义
 * @file common.h
 * @author zhaowei
 * @ingroup memlink
 * @{
 */
#ifndef MEMLINK_COMMON_H
#define MEMLINK_COMMON_H

#include "base/defines.h"

// 客户端错误
#define MEMLINK_ERR_CLIENT          -10
// 服务器端错误
#define MEMLINK_ERR_SERVER          -11
// 服务器端临时错误
#define MEMLINK_ERR_SERVER_TEMP     -12
// 客户端发送的命令号错误
#define MEMLINK_ERR_CLIENT_CMD      -13
// key不存在
#define MEMLINK_ERR_NOKEY           -14 
// key已经存在
#define MEMLINK_ERR_EKEY            -15
// 网络连接错误
#define MEMLINK_ERR_CONNECT         -16
// 返回代码错误
#define MEMLINK_ERR_RETCODE         -17
// value不存在
#define MEMLINK_ERR_NOVAL           -18
// 内存错误
#define MEMLINK_ERR_MEM				-19
// attr错误
#define MEMLINK_ERR_ATTR			-20
// 包错误
#define MEMLINK_ERR_PACKAGE			-21
// 该项已删除
#define MEMLINK_ERR_REMOVED         -22
// range条目错误 
#define MEMLINK_ERR_RANGE_SIZE		-23
// 发送数据错误
#define MEMLINK_ERR_SEND            -24
// 接收数据错误
#define MEMLINK_ERR_RECV            -25
// 命令执行超时
#define MEMLINK_ERR_TIMEOUT         -26
// key错误 
#define MEMLINK_ERR_KEY				-27
// 客户端传递参数错误 
#define MEMLINK_ERR_PARAM			-28
// 文件IO错误
#define MEMLINK_ERR_IO              -29
// list类型错误
#define MEMLINK_ERR_LIST_TYPE       -30
// 客户端接收缓冲区错误
#define MEMLINK_ERR_RECV_BUFFER     -31
// 客户端socket创建错误
#define MEMLINK_ERR_CLIENT_SOCKET   -32
// 客户端连接server端口错误
#define MEMLINK_ERR_CONN_PORT       -33
// 客户端接收头部长度信息失败
#define MEMLINK_ERR_RECV_HEADER     -34
// 客户端连接读端口失败
#define MEMLINK_ERR_CONNECT_READ    -35
// 客户端连接写端口失败
#define MEMLINK_ERR_CONNECT_WRITE   -36
// 不支持的操作
#define MEMLINK_ERR_NOACTION        -37
// 客户端发送的数据包过大
#define MEMLINK_ERR_PACKAGE_SIZE	-38
// 连接已断开
#define MEMLINK_ERR_CONN_LOST		-39
// value在list的所有node中都不存在
#define MEMLINK_ERR_NOVAL_ALL		-40
// 连接太多
#define MEMLINK_ERR_CONN_TOO_MANY	-41
// 不能写入
#define MEMLINK_ERR_NOWRITE			-42
// 同步错误
#define MEMLINK_ERR_SYNC			-43
// 状态错误
#define MEMLINK_ERR_STATE			-44
// VOTE 参数错误
#define MEMLINK_ERR_VOTE_PARAM      -45
// 不是主
#define MEMLINK_ERR_NOT_MASTER      -46
// 角色未知
#define MEMLINK_ERR_NO_ROLE         -47
// 处于slave模式
#define MEMLINK_ERR_SLAVE           -48
// 已满
#define MEMLINK_ERR_FULL			-49
// 已空
#define MEMLINK_ERR_EMPTY			-50
// Table名称太长
#define MEMLINK_ERR_TABLE_NAME		-51
// 没有Table
#define MEMLINK_ERR_NOTABLE			-52
// Table已存在
#define MEMLINK_ERR_ETABLE			-53
// Table太多
#define MEMLINK_ERR_TABLE_TOO_MANY	-54
// 其他错误
#define MEMLINK_ERR                 -1
// 操作失败
#define MEMLINK_FAILED				-2
// 执行成功
#define MEMLINK_OK                  0
// 真
#define MEMLINK_TRUE				TRUE
// 假
#define MEMLINK_FALSE				FALSE

// GETDUMP 命令中大小错误
#define MEMLINK_ERR_DUMP_SIZE       -100
// GETDUMP 命令中dump文件错误
#define MEMLINK_ERR_DUMP_VER        -101

// 命令已回复
#define MEMLINK_REPLIED             -10000

// 主
#define ROLE_MASTER		1
// 备
#define ROLE_BACKUP		2
// 从
#define ROLE_SLAVE		3
#define ROLE_NONE		0

#define COMMITED	1
#define ROLLBACKED	2
#define INCOMPLETE	3  

// 命令执行返回信息头部长度
// datalen(4B) + retcode(2B)
#define CMD_REPLY_HEAD_LEN  (sizeof(int)+sizeof(short))
#define CMD_REQ_HEAD_LEN    (sizeof(int)+sizeof(char))
#define CMD_REQ_SIZE_LEN    sizeof(int)

// format + version + log version + log position + size
#define DUMP_HEAD_LEN	    (sizeof(short)+sizeof(int)+sizeof(int)+sizeof(int)+sizeof(long long))
// format + version + index size
#define SYNCLOG_HEAD_LEN	(sizeof(short)+sizeof(int)+sizeof(int))

#define SYNCLOG_INDEXNUM    5000000
#define SYNCPOS_LEN		    (sizeof(int)+sizeof(int))

// 标记删除
#define MEMLINK_TAG_DEL         1
// 恢复被标记删除的
#define MEMLINK_TAG_RESTORE     0

#define CMD_GETDUMP_OK			1
#define CMD_GETDUMP_CHANGE		2
#define CMD_GETDUMP_SIZE_ERR	3

#define CMD_SYNC_OK		        0
#define CMD_SYNC_FAILED	        1
#define CMD_SYNC_MD5_ERROR      2

#define CMD_RANGE_MAX_SIZE			1024000

// HashTable中最大Table数
#define HASHTABLE_MAX_TABLE			1000
// Table名称最大长度
#define HASHTABLE_TABLE_NAME_SIZE	64
/// HashTable中桶的数量
#define HASHTABLE_BUNKNUM           1000000
/// 最大允许的attr项数
#define HASHTABLE_ATTR_MAX_BIT      32
#define HASHTABLE_ATTR_MAX_BYTE     4
#define HASHTABLE_ATTR_MAX_ITEM     15
// key最大长度
#define HASHTABLE_KEY_MAX           255
// value最大长度
#define HASHTABLE_VALUE_MAX         65535

// 可见和标记删除
#define MEMLINK_VALUE_ALL            0
// 可见
#define MEMLINK_VALUE_VISIBLE        1
// 标记删除
#define MEMLINK_VALUE_TAGDEL         2
// 真实删除
#define MEMLINK_VALUE_REMOVED        4

// 列表
#define MEMLINK_LIST        1
// 队列
#define MEMLINK_QUEUE       2
// 按value排序的列表
#define MEMLINK_SORTLIST    3

// 查找排序列表时，每次每次跳过多少个block
#define MEMLINK_SORTLIST_LOOKUP_STEP    10

#define MEMLINK_VALUE_INT           1
#define MEMLINK_VALUE_INT4          1
#define MEMLINK_VALUE_UINT          2
#define MEMLINK_VALUE_UINT4         2
#define MEMLINK_VALUE_LONG          3 
#define MEMLINK_VALUE_INT8          3 
#define MEMLINK_VALUE_ULONG         4
#define MEMLINK_VALUE_UINT8         4
#define MEMLINK_VALUE_FLOAT         5 
#define MEMLINK_VALUE_FLOAT4        5 
#define MEMLINK_VALUE_DOUBLE        6 
#define MEMLINK_VALUE_FLOAT8        6 
#define MEMLINK_VALUE_STRING        7 
#define MEMLINK_VALUE_OBJ           8

#define MEMLINK_SORTLIST_ORDER_ASC  0x00010000
#define MEMLINK_SORTLIST_ORDER_DESC 0x00020000

#define SYNC_BUF_SIZE       1024 * 1024 

#define MEMLINK_WRITE_LOG   1
#define MEMLINK_NO_LOG      0

// 从前往后查找
#define MEMLINK_FIND_ASC	1
// 从后往前查找
#define MEMLINK_FIND_DESC   2

#define MEMLINK_CMP_RANGE   1
#define MEMLINK_CMP_EQUAL   2

// 内存限制检查间隔时间，秒
#define MEM_CHECK_INTERVAL	3


#define STATE_INIT			200 
#define STATE_DATAOK		201
#define STATE_ALLREADY		202
#define STATE_NOCONN		203
#define STATE_SYNC			204
#define STATE_NOWRITE		205

#define MODE_MASTER_SLAVE	1
#define MODE_MASTER_BACKUP	2

#define BINLOG_CHECK_COUNT  10

//#define BACKUP_READY		100
//#define BACKUP_NOREADY		101

#define MEMLINK_EXIT \
	exit(-1);

typedef struct _memlink_insert_mvalue_item
{
    char            value[255];
    unsigned char   valuelen;
    char			attrstr[HASHTABLE_ATTR_MAX_ITEM * 3];
    unsigned int    attrarray[HASHTABLE_ATTR_MAX_ITEM];
    unsigned char   attrnum;
    int             pos;
	struct _memlink_insert_mvalue_item *next;
}MemLinkInsertVal;

typedef struct _memlink_insert_mkey_item
{
	unsigned int     sum_val_size;

	char             key[255];
	unsigned char    keylen; 
	unsigned int     valnum;
	MemLinkInsertVal *vallist;
	MemLinkInsertVal *tail;
	struct _memlink_insert_mkey_item  *next;
}MemLinkInsertKey;

typedef struct _memlink_insert_mkv
{
	unsigned int sum_size;

	unsigned int keynum;
    int          kindex;
    int          vindex;
	MemLinkInsertKey *keylist;
	MemLinkInsertKey *tail;
}MemLinkInsertMkv;

typedef struct _memlink_stat
{
    unsigned char   valuesize;
    unsigned char   attrsize;
    unsigned int    blocks; // all blocks
    unsigned int    data;   // all alloc data item
    unsigned int    data_used; // all data item used
    unsigned int    mem;       // all alloc mem
	unsigned int    visible;
	unsigned int    tagdel;
}MemLinkStat;

typedef MemLinkStat HashTableStat;

typedef struct _ht_stat_sys
{
	int          version;
    unsigned int keys;
    unsigned int values;
    unsigned int blocks;
	unsigned int data_all;
	unsigned int ht_mem;
	unsigned int pool_mem;
	unsigned int pool_blocks;
	unsigned int all_mem;

	unsigned short conn_read;
	unsigned short conn_write;
	unsigned short conn_sync;
	unsigned short threads;
	
	unsigned int   pid;
	unsigned int   uptime;
	
	unsigned char  bit;
	
	unsigned int   last_dump;

    int logver;
    int logline;
}MemLinkStatSys;

typedef MemLinkStatSys	HashTableStatSys;

#endif

/**
 * @}
 */
