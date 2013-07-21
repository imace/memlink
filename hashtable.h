#ifndef MEMLINK_HASHTABLE_H
#define MEMLINK_HASHTABLE_H

#include <stdio.h>
#include "mem.h"
#include "common.h"
#include "conn.h"

typedef struct _memlink_hashnode
{
    char              *key;
    DataBlock         *data; // DataBlock link
    DataBlock         *data_tail; // DataBlock link tail
    struct _memlink_hashnode  *next;
    uint32_t      used; // used data item
    uint32_t      all;  // all data item;
}HashNode;

typedef struct _memlink_table
{
	char	 name[HASHTABLE_TABLE_NAME_SIZE];
	uint8_t	 listtype;	// list type: list/queue/sortlist
	uint8_t	 valuetype;  // value type for sortlist
	uint16_t valuesize;
	uint8_t	 sortfield;  // which sort? 0:value, 1-255:attr[0-254]
	uint8_t	 attrnum;    // number of attribute format
	uint8_t	 attrsize;   // byte of attribute
	uint8_t	 *attrformat; // attribute format, eg: 3:4:5 => [3, 4, 5]
	HashNode **nodes;
	struct _memlink_table *next;
}Table;


// table name =>Table + key => Node
typedef struct _memlink_hashtable
{
	Table	*tables[HASHTABLE_MAX_TABLE];
	int		table_count;
}HashTable;

int			check_table_key(char *name, char *key);

Table*		table_create(char *name, int valuesize, uint32_t *attrarray, uint8_t attrnum,
						 uint8_t listtype, uint8_t valuetype);
void		table_clear(Table *tb);
void		table_destroy(Table *tb);
uint8_t*	table_attrformat(Table *tb);
int         table_find_value(Table *tb, char *key, void *value, 
                             HashNode **node, DataBlock **dbk, char **data);
int         table_find_value_pos(Table *tb, char *key, void *value, 
                             HashNode **node, DataBlock **dbk);
HashNode*   table_find(Table *tb, char *key);
int         table_print(Table *tb, char *key);
int         table_check(Table *tb, char *key);
int			table_create_node(Table *tb, char *key);
int         hashnode_check(Table*, HashNode *node);


HashTable*  hashtable_create();
void        hashtable_destroy(HashTable *ht);
Table*		hashtable_find_table(HashTable *ht, char *name);
//Table*		hashtable_get_table(HashTable *ht, char *keybuf, char **name, char **key);
int			hashtable_create_table(HashTable *ht, char *name, int valuesize, 
								uint32_t *attrarray, uint8_t attrnum,
								uint8_t listtype, uint8_t valuetype);
int			hashtable_create_node(HashTable *ht, char *tbname, char *key);
void		hashtable_clear_all(HashTable *ht);
uint32_t	hashtable_node_hash(char *key, int len);
uint32_t	hashtable_table_hash(char *key, int len);


int			hashtable_tables(HashTable *ht, char **data);
int			hashtable_remove_table(HashTable *ht, char *tbname);
int			hashtable_remove_key(HashTable *ht, char *tbname, char *key);
int			hashtable_clear_key(HashTable *ht, char *tbname, char *key);

int         hashtable_insert(HashTable *ht, char *tbname, char *key, void *value, 
                         uint32_t *attrarray, char attrnum, int pos);
int         hashtable_insert_binattr(HashTable *ht, char *tbname, char *key, void *value, 
                                 void *attr, int pos);
int         hashtable_move(HashTable *ht, char *tbname, char *key, void *value, int pos);
int         hashtable_del(HashTable *ht, char *tbname, char *key, void *value);
int         hashtable_tag(HashTable *ht, char *tbname, char *key, void *value, uint8_t tag);
int         hashtable_attr(HashTable *ht, char *tbname, char *key, void *value, 
						   uint32_t *attrarray, int attrnum);
int         hashtable_attr_inc(HashTable *ht, char *tbname, char *key, void *value, 
						   uint32_t *attrarray, int attrnum);
//int         hashtable_attr_dec(HashTable *ht, char *tbname, char *key, void *value, uint32_t *attrarray, int attrnum);
int         hashtable_range(HashTable *ht, char *tbname, char *key, 
						uint8_t kind, uint32_t *attrarray, int attrnum, 
                        int frompos, int len, Conn *conn);
int         hashtable_clean(HashTable *ht, char *tbname, char *key);
int			hashtable_clean_all(HashTable *ht);
int         hashtable_stat(HashTable *ht, char *tbname, char *key, HashTableStat *stat);
int			hashtable_stat_table(HashTable *ht, char *tbname, HashTableStatSys *stat);
int			hashtable_stat_sys(HashTable *ht, HashTableStatSys *stat);
int         hashtable_count(HashTable *ht, char *tbname, char *key, uint32_t *attrarray, int attrnum, 
                        int *visible_count, int *tagdel_count);
int         hashtable_lpush(HashTable *ht, char *tbname, char *key, 
							void *value, uint32_t *attrarray, char attrnum);
int         hashtable_rpush(HashTable *ht, char *tbname, char *key, 
							void *value, uint32_t *attrarray, char attrnum);
int         hashtable_lpop(HashTable *ht, char *tbname, char *key, int num, Conn *conn);
int         hashtable_rpop(HashTable *ht, char *tbname, char *key, int num, Conn *conn);
int         hashtable_del_by_attr(HashTable *ht, char *tbname, char *key, uint32_t *attrarray, int attrnum);


// for sortlist
int         hashtable_sortlist_mdel(HashTable *ht, char *tbname, char *key, 
								uint8_t kind, void *valmin, void *valmax, 
                                uint32_t *attrarray, uint8_t attrnum);
int         hashtable_sortlist_count(HashTable *ht, char *tbname, char *key, 
								uint32_t *attrarray, int attrnum, 
                                void* valmin, void *valmax, int *visible_count, int *tagdel_count);
int         hashtable_sortlist_range(HashTable *ht, char *tbname, char *key, uint8_t kind, 
				                uint32_t *attrarray, int attrnum, void *valmin,
                                void *valmax, Conn *conn);
int			hashtable_sortlist_insert_binattr(HashTable *ht, char *tbname, char *key, void *value, void *attr);
int			hashtable_sortlist_insert(HashTable *ht, char *tbname, char *key, void *value, uint32_t *attrarray, char attrnum);

#endif
