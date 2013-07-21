#ifndef MEMLINK_DATABLOCK_H
#define MEMLINK_DATABLOCK_H

#include <stdio.h>
#include "hashtable.h"

int         dataitem_check_kind(int ret, int kind);
int         dataitem_have_data(Table *, HashNode *node, char *itemdata, unsigned char kind);
int         dataitem_check(char *itemdata, int valuesize);
int         dataitem_check_data(Table*, HashNode *node, char *itemdata);
char*       dataitem_lookup(Table*,HashNode *node, void *value, DataBlock **dbk);
int         dataitem_lookup_pos(Table*,HashNode *node, void *value, DataBlock **dbk);
int         dataitem_copy(Table*,HashNode *node, char *addr, void *value, void *attr);
int         dataitem_copy_attr(Table*, HashNode *node, char *addr, char *attrflag, char *attr);
int         dataitem_lookup_pos_attr(Table *tb, HashNode *node, int pos, unsigned char kind, 
						char *attrval, char *attrflag, DataBlock **dbk, int *dbkpos);
int         dataitem_skip2pos(Table *,HashNode *node, DataBlock *dbk, int skip, unsigned char kind);

int         datablock_suitable_size(int blocksize);
int         datablock_print(Table *, HashNode *node, DataBlock *dbk);
int         datablock_del(Table*, HashNode *node, DataBlock *dbk, char *data);
int         datablock_del_restore(Table*, HashNode *node, DataBlock *dbk, char *data);
int         datablock_lookup_pos(HashNode *node, int pos, unsigned char kind, DataBlock **dbk);
int         datablock_lookup_valid_pos(HashNode *node, int pos, unsigned char kind, DataBlock **dbk);
int         datablock_check_idle(HashNode *node, DataBlock *dbk, int skipn, void *value, void *attr);
int         datablock_check_null_pos(Table*, HashNode *node, DataBlock *dbk, int pos, void *value, void *attr);
DataBlock*  datablock_new_copy(Table*, HashNode *node, DataBlock *dbk, int skipn, void *value, void *attr);
DataBlock*  datablock_new_copy_pos(Table*, HashNode *node, DataBlock *dbk, int pos, void *value, void *attr);
int         datablock_copy(DataBlock *tobk, DataBlock *frombk, int datalen);
int         datablock_copy_used(Table*, HashNode *node, DataBlock *tobk, int topos, DataBlock *frombk);
int			datablock_copy_used_blocks(Table *, HashNode *node, DataBlock *tobk, int topos, 
								DataBlock *frombk, int blockcount);
int         datablock_free(DataBlock *headbk, DataBlock *tobk, int datalen);
int         datablock_free_inverse(DataBlock *startbk, DataBlock *endbk, int datalen);

int			datablock_resize(Table*, HashNode *node, DataBlock *dbk);

int         attr_array2_binary_flag(unsigned char *attrformat, unsigned int *attrarray, 
                                    int attrnum, int attrsize, char *attrval, char *attrflag);

int         sortlist_valuecmp(unsigned char type, void *v1, void *v2, int size);
int         sortlist_lookup(Table*,HashNode *node, int step, void *value, int kind, DataBlock **dbk);
int         sortlist_lookup_valid(Table*, HashNode *node, int step, void *value, int kind, DataBlock **dbk);


#define		datablock_link_prev(node,dbk,newdbk) \
			do{\
				newdbk->prev = dbk->prev;\
				if (dbk->prev) { \
					dbk->prev->next = newdbk;\
				}else{ \
					node->data = newdbk;\
				}\
			}while(0)

#define		datablock_link_next(node,dbk,newdbk) \
			do{\
				newdbk->next = dbk->next;\
				if (dbk->next) { \
					dbk->next->prev = newdbk;\
				}else{ \
					node->data_tail = newdbk;\
				}\
			}while(0)

#define		datablock_link_both(node,dbk,newdbk) \
			do{\
				newdbk->prev = dbk->prev;\
				if (dbk->prev) { \
					dbk->prev->next = newdbk;\
				}else{ \
					node->data = newdbk;\
				}\
				newdbk->next = dbk->next;\
				if (dbk->next) { \
					dbk->next->prev = newdbk;\
				}else{ \
					node->data_tail = newdbk;\
				}\
			}while(0)

#endif
