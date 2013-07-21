/**
 * 内部哈希表
 * @file hashtable.c
 * @author zhaowei
 * @ingroup memlink
 * @{
 */
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include "zzmalloc.h"
#include "logfile.h"
#include "hashtable.h"
#include "serial.h"
#include "utils.h"
#include "datablock.h"
#include "common.h"
#include "runtime.h"


/**
 * 创建HashTable
 */
HashTable*
hashtable_create()
{
    HashTable *ht;

    ht = (HashTable*)zz_malloc(sizeof(HashTable));
    memset(ht, 0, sizeof(HashTable));    

    return ht;
}

/**
 * 释放HashTable占用的资源
 */
void
hashtable_destroy(HashTable *ht)
{
    if (NULL == ht)
        return;

    int i;
    for (i = 0; i < HASHTABLE_MAX_TABLE; i++) {
        Table *tb = ht->tables[i];
        if (tb) {
            table_destroy(tb);
        }
    }

    zz_free(ht);
}


Table* 
hashtable_find_table(HashTable *ht, char *name)
{
    int      namelen = strlen(name);
    uint32_t hash    = hashtable_table_hash(name, namelen);
    Table    *tb     = ht->tables[hash];

    while (tb) {
        if (strcmp(name, tb->name) == 0) {
            return tb;
        }
        tb = tb->next;
    }
    return NULL;
}

void
hashtable_clear_all(HashTable *ht)
{
    if (NULL == ht)
        return;

    int i;
    
    Table *tb, *tbnext;
    for (i = 0; i < HASHTABLE_MAX_TABLE; i++) {
        tb = ht->tables[i]; 
        while (tb) {
            tbnext = tb->next;
            table_destroy(tb);
            tb = tb->next;
        }
    }
}

/**
 * hash函数
 */
uint32_t
hashtable_node_hash(char *str, int len)
{
    //BKDR Hash
    uint32_t seed = 131;
    uint32_t hash = 0;
    int i;

    for (i = 0; i < len; i++) {   
        hash = hash * seed + (*str++);
    }   

    return (hash & 0x7FFFFFFF) % HASHTABLE_BUNKNUM;
}
uint32_t
hashtable_table_hash(char *str, int len)
{
    //BKDR Hash
    uint32_t seed = 131;
    uint32_t hash = 0;
    int i;

    for (i = 0; i < len; i++) {   
        hash = hash * seed + (*str++);
    }   

    return (hash & 0x7FFFFFFF) % HASHTABLE_MAX_TABLE;
}

/*int
table_name(char *keybuf, char **name, char **key)
{
    char *sp = strchr(keybuf, '.');
    if (NULL == sp) {
        *name = NULL;
        *key = keybuf;
    }else{
        *name = keybuf;
        *sp = 0;
        *key = sp+1;
    }
    if (*name == NULL || (*name)[0] == 0) {
        DNOTE("name error! keybuf:%s\n", keybuf);
        return MEMLINK_ERR_PARAM;
    }   
    if (*key == NULL || (*key)[0] == 0) {
        DNOTE("key error! keybuf:%s\n", keybuf);
        return MEMLINK_ERR_PARAM;
    }   
 
    return MEMLINK_OK;
}*/

int
check_table_key(char *name, char *key)
{
    if (name == NULL || name[0] == 0) {
        DNOTE("name error! table:%s\n", name);
        return MEMLINK_ERR_PARAM;
    }   
    if (key == NULL || key[0] == 0) {
        DNOTE("key error! key:%s\n", key);
        return MEMLINK_ERR_PARAM;
    }   
    return MEMLINK_OK;
}


/*Table*
hashtable_get_table(HashTable *ht, char *keybuf, char **name, char **key)
{
    if (table_name(keybuf, name, key) < 0) 
        return NULL;
    return  hashtable_find_table(g_runtime->ht, *name);
}*/

int 
hashtable_create_table(HashTable *ht, char *name, int valuesize, 
                       uint32_t *attrarray, uint8_t attrnum,
                       uint8_t listtype, uint8_t valuetype)
{
    if (ht->table_count >= HASHTABLE_MAX_TABLE) {
        return MEMLINK_ERR_TABLE_TOO_MANY; 
    }
    int      namelen = strlen(name);
    uint32_t hash    = hashtable_table_hash(name, namelen);
    Table    *tb     = ht->tables[hash];

    while (tb) {
        if (strcmp(name, tb->name) == 0) {
            return MEMLINK_ERR_ETABLE;
        }
        tb = tb->next;
    }
    
    tb = table_create(name, valuesize, attrarray, attrnum, listtype, valuetype);
    if (NULL == tb) {
        return MEMLINK_ERR;
    }
    ht->tables[hash] = tb;
    ht->table_count++;
    return MEMLINK_OK;
}

Table*  
table_create(char *name, int valuesize, uint32_t *attrarray, uint8_t attrnum, 
             uint8_t listtype, uint8_t valuetype)
{
    int namelen = strlen(name);
    
    if (namelen >= HASHTABLE_TABLE_NAME_SIZE) {
        //return MEMLINK_ERR_TABLE_NAME;
        return NULL;
    }

    Table *tb = (Table*)zz_malloc(sizeof(Table));
    memset(tb, 0, sizeof(Table));

    strcpy(tb->name, name);
    tb->listtype  = listtype;
    tb->valuetype = valuetype;
    tb->valuesize = valuesize;
    tb->attrnum   = attrnum;
    
    int attrsize = 2; // tagdel 1bit, real del 1bit
    int i;

    for (i=0; i<attrnum; i++) {
        attrsize += attrarray[i];
    }
    
    tb->attrsize = attrsize/8 + ((attrsize%8>0)?1:0);
    //DINFO("tb->attrsize:%d, %d, %d, %d\n", tb->attrsize, attrsize, attrsize/8, ((attrsize%8>0)?1:0));

    if (attrnum < sizeof(void*)) {
        char *a = (char*)&tb->attrformat;
        memset(a, 0, sizeof(void*));
        for (i=0; i<attrnum; i++) {
            a[i] = attrarray[i]; 
        }
    }else{
        tb->attrformat = (uint8_t*)zz_malloc(attrnum+1);
        memset(tb->attrformat, 0, attrnum+1);
        for (i=0; i<attrnum; i++) {
            tb->attrformat[i] = attrarray[i]; 
        }
    }

    HashNode**  nodes = (HashNode**)zz_malloc(sizeof(HashNode*) * HASHTABLE_BUNKNUM);
    memset(nodes, 0, sizeof(HashNode*) * HASHTABLE_BUNKNUM);
    tb->nodes = nodes;

    return tb;
}

int 
hashtable_create_node(HashTable *ht, char *tbname, char *key)
{
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        return MEMLINK_ERR_NOTABLE;
    }
    return table_create_node(tb, key);
}

void 
table_clear(Table *tb)
{
    if (NULL == tb) {
        DERROR("destroy NULL table\n");
        return;
    }

    HashNode **nodes = tb->nodes;
    int i;

    for (i=0; i<HASHTABLE_BUNKNUM; i++) {
        HashNode *node = nodes[i];
        if (node) {
            DataBlock   *dbk = node->data;
            DataBlock   *tmp; 
            while (dbk) {
                tmp = dbk;
                dbk = dbk->next;
                zz_free(tmp);
            }
            zz_free(node->key);
        }

    }
    memset(nodes, 0, sizeof(HashNode*) * HASHTABLE_BUNKNUM);
}

int 
hashnode_remove(Table *tb, HashNode *node)
{
    DataBlock    *dbk = node->data;
    DataBlock    *tmp;
    int          datalen = tb->valuesize + tb->attrsize;

    zz_free(node->key);        
    zz_free(node);
    
    while (dbk) {
        tmp = dbk;
        dbk = dbk->next;
        mempool_put(g_runtime->mpool, tmp, sizeof(DataBlock) + tmp->data_count * datalen);    
    }
    return MEMLINK_OK;
}


void 
table_destroy(Table *tb)
{
    HashNode *node, *tmp;
    int i;
    for (i = 0; i < HASHTABLE_BUNKNUM; i++) {
        node = tb->nodes[i];
        while (node) {
            tmp = node->next;
            hashnode_remove(tb, node);
            node = tmp;
        }
    }
    if (tb->attrnum >= sizeof(void*)) {
        zz_free(tb->attrformat);
    }
    zz_free(tb);
}

inline uint8_t*
table_attrformat(Table *tb)
{
    if (tb->attrnum < sizeof(void*)) {
        return (uint8_t*)&tb->attrformat;
    }else{
        return tb->attrformat;
    }
}

int
table_find_value(Table *tb, char *key, void *value, HashNode **node, 
                        DataBlock **dbk, char **data)
{
    HashNode *fnode = table_find(tb, key);

    if (NULL == fnode) {
        return MEMLINK_ERR_NOKEY;
    }
    if (node) {
        *node = fnode;
    }

    /*char buf[256] = {0};
    memcpy(buf, value, fnode->valuesize);
    DINFO("find dataitem ... node: %p, type:%d, key:%s, value:%s\n", fnode, fnode->type, key, buf);*/

    if (tb->listtype == MEMLINK_SORTLIST) {
        int ret;
        ret = sortlist_lookup(tb, fnode, MEMLINK_SORTLIST_LOOKUP_STEP, value, MEMLINK_VALUE_ALL, dbk);
        if (ret < 0)
            return MEMLINK_ERR_NOVAL;
        *data = (*dbk)->data + (tb->valuesize + tb->attrsize) * ret;
    }else{ 
        char *item = dataitem_lookup(tb, fnode, value, dbk);
        if (NULL == item) {
            //DWARNING("dataitem_lookup error: key:%s, value:%s\n", key, (char*)value);
            return MEMLINK_ERR_NOVAL;
        }
        *data = item;
    }

    return MEMLINK_OK; 
}

int
table_find_value_pos(Table *tb, char *key, void *value, HashNode **node, DataBlock **dbk)
{
    HashNode *fnode = table_find(tb, key);
    if (NULL == fnode) {
        return MEMLINK_ERR_NOKEY;
    }
    if (node) {
        *node = fnode;
    }

    if (tb->listtype == MEMLINK_SORTLIST) {
        return sortlist_lookup(tb, fnode, MEMLINK_SORTLIST_LOOKUP_STEP, value, MEMLINK_VALUE_ALL, dbk);
    }else{ 
        return dataitem_lookup_pos(tb, fnode, value, dbk);
    }
}


int
table_create_node(Table *tb, char *key)
{
    int         keylen = strlen(key);
    uint32_t    hash   = hashtable_node_hash(key, keylen);
    HashNode    *node  = tb->nodes[hash];
  
    //DINFO("hashtable_create_node call ... %s, %p, hash: %d\n", key, node, hash);
    while (node) {
        if (strcmp(node->key, key) == 0) {
            return MEMLINK_ERR_EKEY;
        }
        node = node->next;
    }
    
    node = (HashNode*)zz_malloc(sizeof(HashNode));
    memset(node, 0, sizeof(HashNode));
    
    node->key  = zz_strdup(key);
    node->data = NULL;
    node->next = tb->nodes[hash];
    tb->nodes[hash] = node;

    return MEMLINK_OK;    
}

int
hashtable_clear_key(HashTable *ht, char *tbname, char *key)
{
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        return MEMLINK_ERR_NOTABLE;
    }
    
    HashNode *node; //  = ht->bunks[hash];
    node = table_find(tb, key);
    if (NULL == node) {
        return MEMLINK_ERR_NOKEY;
    }
    
    DataBlock   *dbk = node->data;
    DataBlock   *tmp;
    int         datalen = tb->valuesize + tb->attrsize;

    node->data      = NULL;
    node->data_tail = NULL;
    node->used      = 0;
    node->all       = 0;
    
    while (dbk) {
        tmp = dbk;
        dbk = dbk->next;
        mempool_put(g_runtime->mpool, tmp, sizeof(DataBlock) + tmp->data_count * datalen);    
    }
    return MEMLINK_OK;
}

int
table_remove_key(Table *tb, char *key)
{
    int         keylen; // = strlen(key);
    uint32_t    hash; //   = hashtable_hash(key, keylen);
    HashNode    *node; //  = ht->bunks[hash];
    HashNode    *last  = NULL;

    if (NULL == key) {
        return MEMLINK_ERR_KEY;
    }
    keylen = strlen(key);
    hash   = hashtable_node_hash(key, keylen);
    node   = tb->nodes[hash];

    while (node) {
        if (strcmp(key, node->key) == 0) {
            break;
        }
        last = node;
        node = node->next;
    }
    
    if (NULL == node) {
        return MEMLINK_ERR_NOKEY;
    }

    if (last) {
        last->next = node->next;
    }else{
        tb->nodes[hash] = node->next; 
    }
    return hashnode_remove(tb, node);
}


int
hashtable_remove_table(HashTable *ht, char *tbname)
{
    int      namelen = strlen(tbname);
    uint32_t hash    = hashtable_table_hash(tbname, namelen);
    Table    *tb     = ht->tables[hash];
    Table    *last   = NULL;

    while (tb) {
        if (strcmp(tbname, tb->name) == 0) {
            break;
        }
        last = tb;
        tb = tb->next;
    }
    if (tb == NULL) {
        return MEMLINK_ERR_NOTABLE;
    }
    if (last) {
        last->next = tb->next;
    }else{
        ht->tables[hash] = tb->next; 
    }
    table_destroy(tb);
    ht->table_count--;

    return MEMLINK_OK;
}

int 
hashtable_tables(HashTable *ht, char **data)
{
    int len = 0;
    int i;
    for (i=0; i<HASHTABLE_MAX_TABLE; i++) {
        Table *t = ht->tables[i];
        while (t) {
            t = t->next;
        }
    }
    return len;
}
 
int
hashtable_remove_key(HashTable *ht, char *tbname, char *key)
{
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        return MEMLINK_ERR_NOTABLE;
    }
    return table_remove_key(tb, key);
}


/**
 * 通过key找到一个HashNode
 */
HashNode*
table_find(Table *tb, char *key)
{
    int         keylen = strlen(key);
    uint32_t    hash   = hashtable_node_hash(key, keylen);
    HashNode    *node  = tb->nodes[hash];

    while (node) {
        if (strcmp(key, node->key) == 0) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

int
hashtable_insert_binattr(HashTable *ht, char *tbname, char *key, void *value, void *attr, int pos)
{
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        return MEMLINK_ERR_NOTABLE;
    }

    int ret     = 0;
    HashNode *node = table_find(tb, key);
    if (NULL == node) {
        //DINFO("not found node for key:%s\n", key);
        //return MEMLINK_ERR_NOKEY;
        ret = table_create_node(tb, key);
        if (ret != MEMLINK_OK)
            return ret;
        node = table_find(tb, key);
    }
    DataBlock *dbk = node->data;
    int datalen = tb->valuesize + tb->attrsize;
    int dbkpos  = 0; 

    if (tb->listtype == MEMLINK_SORTLIST) {
        dbkpos = sortlist_lookup_valid(tb, node, MEMLINK_SORTLIST_LOOKUP_STEP, value, 
                                        MEMLINK_VALUE_ALL, &dbk);
        //DINFO("====== sortlist lookup: %d, %d\n", *(int*)value, dbkpos);
    }else{
        if (pos >= node->used) {
            dbk = node->data_tail;
            //dbkpos = pos;
            dbkpos = -1;
            //DNOTE("insert last skip:%d, dbkpos:%d\n", pos, dbkpos);
        }else{
            ret = datablock_lookup_valid_pos(node, pos, MEMLINK_VALUE_ALL, &dbk);
            dbkpos = dataitem_skip2pos(tb, node, dbk, pos - ret, MEMLINK_VALUE_ALL);
            //DNOTE("pos:%d, dbk:%p, dbkpos:%d, skipn:%d\n", pos, dbk, dbkpos, pos - ret);
        }
    }

    DataBlock   *newbk   = NULL;
    int         blockmax = g_cf->block_data_count[g_cf->block_data_count_items - 1];
    int         oldfull  = 0;
    
    if (dbk && dbk->data_count == blockmax && \
        dbk->visible_count + dbk->tagdel_count == dbk->data_count) {
        oldfull = 1;
    }
    //DINFO("full:%d, pos:%d, dbkpos:%d\n", oldfull, pos, dbkpos);
    //if (oldfull && (pos == 0 || pos > blockmax)) { // insert at first or last
    if (oldfull && (dbkpos < 0 || (dbk == node->data && dbkpos == 0))) {
        //DINFO("insert first or last ...\n");
        int newsize = datablock_suitable_size(1);
        newbk = mempool_get2(g_runtime->mpool, newsize, datalen);
        dataitem_copy(tb, node, newbk->data, value, attr);
        
        newbk->visible_count = 1;
        
        if (dbkpos == 0) {
            node->data  = newbk;
            dbk->prev   = newbk;
            newbk->next = dbk;
        }else{
            node->data_tail = newbk;
            dbk->next   = newbk;
            newbk->prev = dbk;
        }
        node->used++;
        node->all++;
    
        zz_check(newbk);
        return MEMLINK_OK;
    }

    node->used++;
    newbk = datablock_new_copy_pos(tb, node, dbk, dbkpos, value, attr); 
    //DINFO("1 datablock new copy pos, dbk:%p, newbk:%p\n", dbk, newbk);
    if (dbk == NULL) { // the first one
        node->data = node->data_tail = newbk;
        node->all += newbk->data_count;
        return MEMLINK_OK;
    }
    zz_check(newbk);

    if (newbk == dbk) { // not create new datablock
        return MEMLINK_OK;
    }
    
    node->all = node->all - dbk->data_count + newbk->data_count;
    //if (dbk->visible_count + dbk->tagdel_count == dbk->data_count) {
    if (oldfull) {
        DataBlock   *dbknext = dbk->next;
        DataBlock   *newbk2;
        char        *lastdata = dbk->data + (dbk->data_count - 1) * datalen;
        
        if (dbknext && dbknext->data_count == blockmax && \
                dbknext->visible_count + dbknext->tagdel_count == blockmax) {
            int newsize = datablock_suitable_size(1);
            newbk2 = mempool_get2(g_runtime->mpool, newsize, datalen);
            dataitem_copy(tb, node, newbk2->data, lastdata, lastdata + tb->valuesize);
            newbk2->visible_count = 1;

            newbk2->next  = dbknext;
            newbk2->prev  = newbk;
            newbk->next   = newbk2;
            dbknext->prev = newbk2;

            if (dbk->prev) {
                dbk->prev->next = newbk;
            }else{
                node->data = newbk;
            }
            node->all += newbk2->data_count;
            mempool_put2(g_runtime->mpool, dbk, datalen);
        }else{
            newbk2 = datablock_new_copy_pos(tb, node, dbknext, 0, lastdata, lastdata + tb->valuesize);
            //DINFO("2 datablock new copy pos, dbk:%p, newbk:%p\n", dbk, newbk);
            if (newbk2 == dbknext) {
                newbk->next   = dbknext;
                dbknext->prev = newbk;
                
                datablock_link_prev(node, dbk, newbk);

                mempool_put2(g_runtime->mpool, dbk, datalen);
            }else{
                newbk->next  = newbk2;
                newbk2->prev = newbk;

                if (dbk->prev) {
                    dbk->prev->next = newbk;
                }else{
                    node->data = newbk;
                }
                if (dbknext == NULL || dbknext->next == NULL) {
                    node->data_tail = newbk2;
                }else{
                    dbknext->next->prev = newbk2;
                }

                if (dbknext) {
                    node->all = node->all - dbknext->data_count + newbk2->data_count;
                }else{
                    node->all += newbk2->data_count;
                }
                mempool_put2(g_runtime->mpool, dbk, datalen);
                if (dbknext) {
                    mempool_put2(g_runtime->mpool, dbknext, datalen);
                }
            }
            zz_check(newbk2);
        }
        return MEMLINK_OK;
    }else{
        datablock_link_both(node, dbk, newbk);
        mempool_put2(g_runtime->mpool, dbk, datalen);
        return MEMLINK_OK;
    }
}

/**
 * add new value at a specially position
 */
int
hashtable_insert(HashTable *ht, char *tbname, char *key, void *value, uint32_t *attrarray, char attrnum, int pos)
{
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        return MEMLINK_ERR_NOTABLE;
    }

    char attr[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};
    int  ret;
   
    if (tb->attrnum != attrnum) {
        DINFO("attr error: tb->attrnum:%d, param attrnum:%d\n", tb->attrnum, attrnum);
        return MEMLINK_ERR_ATTR;
    }
    if (tb->attrnum > 0) {
        ret = attr_array2binary(table_attrformat(tb), attrarray, attrnum, attr);
        if (ret <= 0) {
            DINFO("attr_array2binary error: %d\n", ret);
            return MEMLINK_ERR_ATTR;
        }
    }else{
        attr[0] = 0x01;
    }
    
    //char buf[128];
    //DINFO("attrsize:%d attr:%s\n", tb->attrsize, formatb(attr, tb->attrsize, buf, sizeof(buf)));

    //printh(attr, node->attrsize);
    return hashtable_insert_binattr(ht, tbname, key, value, attr, pos);
}


int
hashtable_sortlist_insert_binattr(HashTable *ht, char *tbname, char *key, void *value, void *attr)
{
    return hashtable_insert_binattr(ht, tbname, key, value, attr, INT_MAX);
}

int
hashtable_sortlist_insert(HashTable *ht, char *tbname, char *key, void *value, uint32_t *attrarray, char attrnum)
{
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        return MEMLINK_ERR_NOTABLE;
    }

    char attr[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};
    int  ret;
    HashNode *node = table_find(tb, key);
    if (NULL == node) {
        DINFO("table_insert not found node for key:%s\n", key);
        return MEMLINK_ERR_NOKEY;
    }
   
    if (tb->attrnum != attrnum) {
        return MEMLINK_ERR_ATTR;
    }
    if (tb->attrnum > 0) {
        ret = attr_array2binary(table_attrformat(tb), attrarray, attrnum, attr);
        if (ret <= 0) {
            DINFO("attr_array2binary error: %d\n", ret);
            return MEMLINK_ERR_ATTR;
        }
    }else{
        attr[0] = 0x01;
    }

    return hashtable_sortlist_insert_binattr(ht, tbname, key, value, attr);
}

/**
 * move value to another position
 */
int
hashtable_move(HashTable *ht, char *tbname, char *key, void *value, int pos)
{
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        return MEMLINK_ERR_NOTABLE;
    }

    char attr[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};
    HashNode *node = NULL;
    DataBlock *dbk = NULL;
    char     *item = NULL; 
    
    char ret = table_find_value(tb, key, value, &node, &dbk, &item);
    if (ret < 0) {
        //DWARNING("hashtable_move not found value, ret:%d, key:%s\n", ret, key);
        return ret;
    }
    
    DINFO("move find dbk:%p\n", dbk);
    char *mdata = item + tb->valuesize;
    memcpy(attr, mdata, tb->attrsize);  

    datablock_del(tb, node, dbk, item);

    //hashtable_print(ht, key);
    int retv = hashtable_insert_binattr(ht, tbname, key, value, attr, pos);
    if (retv != MEMLINK_OK) {
        datablock_del_restore(tb, node, dbk, item);
        return retv;
    }
   
    // remove null block
    //DINFO("dbk:%p, count:%d, prev:%p, dbk->next:%p\n", dbk, dbk->tagdel_count + dbk->visible_count, prev, dbk->next);
    if (dbk->data_count > 0) {
        if (dbk->tagdel_count + dbk->visible_count == 0) {
            if (dbk->prev) {
                dbk->prev->next = dbk->next;
            }else{
                node->data = dbk->next;        
            }
            if (dbk->next) {
                dbk->next->prev = dbk->prev;
            }else{
                node->data_tail = dbk->prev;
            }
            DINFO("move release null block: %p\n", dbk);
            node->all -= dbk->data_count;

            //mempool_put(g_runtime->mpool, dbk, sizeof(DataBlock) + dbk->data_count * (node->valuesize + node->attrsize));
            mempool_put2(g_runtime->mpool, dbk, tb->valuesize + tb->attrsize);
        }else{
            //DINFO("try start resize.\n");
            //hashnode_check(node);
            //hashtable_print(ht, key);
            datablock_resize(tb, node, dbk);
            //DINFO("resize end.\n");
            //hashnode_check(node);
            //hashtable_print(ht, key);
        }
    }
    
    return MEMLINK_OK;
}


/**
 * remove a value, only write a flag to 0
 */
int 
hashtable_del(HashTable *ht, char *tbname, char *key, void *value)
{
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        return MEMLINK_ERR_NOTABLE;
    }

    char        *item = NULL;
    HashNode    *node = NULL;
    DataBlock   *dbk  = NULL, *prev = NULL;

    int ret = table_find_value(tb, key, value, &node, &dbk, &item);
    if (ret < 0) {
        DINFO("table_del find value error! %s\n", key);
        return ret;
    }
    char *data = item + tb->valuesize;
    uint8_t v = *data & 0x3; 

    *data &= 0xfe;
    if ( (v & 0x01) == 1) {
        if ( (v & 0x02) == 2) {
            dbk->tagdel_count --;
        }else{
            dbk->visible_count --;
        }
        node->used--;
    }
    int datalen = tb->valuesize + tb->attrsize;
    int dbksize = dbk->visible_count + dbk->tagdel_count;

    if (dbksize == 0) {
        DINFO("del release null block:%p, dcount:%d all:%d\n", dbk, dbk->data_count, node->all);
        prev = dbk->prev;
        if (prev) {
            prev->next = dbk->next;
        }else{
            node->data = dbk->next;        
        }
        if (dbk->next) {
            dbk->next->prev = prev;
        }else{
            node->data_tail = prev;
        }
        node->all -= dbk->data_count;
        mempool_put2(g_runtime->mpool, dbk, datalen);
    }else{
        //DINFO("before resize, used:%d, all:%d\n", node->used, node->all);
        datablock_resize(tb, node, dbk);
        //DINFO("after resize, used:%d, all:%d\n", node->used, node->all);
    }

    return MEMLINK_OK;
}

int 
hashtable_sortlist_mdel(HashTable *ht, char *tbname, char *key, uint8_t kind, void *valmin, void *valmax, 
                    uint32_t *attrarray, uint8_t attrnum)
{
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        return MEMLINK_ERR_NOTABLE;
    }

    char attrval[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};
    char attrflag[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};
    char        *item = NULL;
    HashNode    *node = NULL;
    DataBlock   *dbk  = NULL, *prev = NULL;
    DataBlock   *nextdbk = NULL;
    int         pos;
    int         ret;

    node = table_find(tb, key);
    if (NULL == node) {
        return MEMLINK_ERR_NOKEY;
    }
    if (attrnum > 0) {
        attrnum = attr_array2_binary_flag(table_attrformat(tb), attrarray, attrnum, tb->attrsize, attrval, attrflag);
    }

    //table_print(ht, key);
    if (((char*)valmin)[0] == 0) {
        dbk = node->data;
        pos = 0;
    }else{
        ret = sortlist_lookup(tb, node, MEMLINK_SORTLIST_LOOKUP_STEP, valmin, MEMLINK_VALUE_ALL, &dbk);
        if (ret < 0)
            return ret;
        pos = ret;
    }
    //table_print(ht, key);

    DINFO("lookup pos:%d, dbk:%p\n", pos, dbk);
    int datalen = tb->valuesize + tb->attrsize;
    int i;
    char *attr;
    while (dbk) {
        //DINFO("find in dbk:%p\n", dbk);
        //datablock_print(node, dbk);
        item = (char*)dbk->data + pos * datalen;
        for (i = pos; i < dbk->data_count; i++) { 
            if (pos > 0)
                pos = 0;

            char mybuf[128] = {0};
            memcpy(mybuf, item, tb->valuesize);
            DINFO("try del val:%s, max:%s\n", mybuf, (char*)valmax);

            ret = dataitem_check_data(tb, node, item);
            //if (ret != MEMLINK_VALUE_REMOVED) {
            if (dataitem_check_kind(ret, kind)) {
                char *attrdata = item + tb->valuesize;
                if (attrnum > 0) {
                    int k;
                    for (k = 0; k < tb->attrsize; k++) {
                        if ((attrdata[k] & attrflag[k]) != attrval[k]) {
                            break;
                        }
                    }
                    if (k < tb->attrsize) { // not equal
                        item += datalen;
                        continue;
                    }
                }

                if (((char*)valmax)[0] != 0 && 
                     sortlist_valuecmp(tb->valuetype, valmax, item, tb->valuesize) <= 0) {
        
                    char mybuf[32] = {0};
                    memcpy(mybuf, item, tb->valuesize);
                    DINFO("del end, ret:%d, val:%s, max:%s\n", ret, mybuf, (char*)valmax);

                    goto table_sortlist_mdel_over;
                }
 
                attr = item + tb->valuesize;
                *attr &= 0xfe;

                if (ret == MEMLINK_VALUE_VISIBLE) {
                    dbk->visible_count--;
                }else{
                    dbk->tagdel_count--;
                }
                node->used--;

                if (dbk->tagdel_count + dbk->visible_count == 0) {
                    DINFO("del release null block: %p\n", dbk);
                    nextdbk = dbk->next;
                    prev = dbk->prev;
                    if (prev) {
                        prev->next = dbk->next;
                    }else{
                        node->data = dbk->next;        
                    }
                    if (dbk->next) {
                        dbk->next->prev = prev;
                    }else{
                        node->data_tail = prev;
                    }
                    node->all -= dbk->data_count;
                    mempool_put(g_runtime->mpool, dbk, sizeof(DataBlock) + dbk->data_count * datalen);
                    break;
                }
            }
            item += datalen;
        }
        if (nextdbk) {
            dbk = nextdbk;
            nextdbk = NULL;
        }else{
            dbk = dbk->next;
        }
    }
table_sortlist_mdel_over:
    return MEMLINK_OK;
}

/**
 * tag delete/restore, only change the flag to 1 or 0
 */
int
hashtable_tag(HashTable *ht, char *tbname, char *key, void *value, uint8_t tag)
{
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        return MEMLINK_ERR_NOTABLE;
    }

    char        *item = NULL;
    HashNode    *node = NULL;
    DataBlock   *dbk  = NULL;

    int ret = table_find_value(tb, key, value, &node, &dbk, &item);
    if (ret < 0) {
        DINFO("table_tag not found key: %s\n", key);
        return ret;
    }
    //DINFO("tag: %d, %d\n", tag, tag<<1);
    
    char *data = item + tb->valuesize;
    uint8_t v = *data & 0x3; 

    //DINFO("tag v:%x\n", v);
    if ( (v & 0x01) == 1) { // data not real delete
        //DINFO("tag data:%x\n", *data);
        if (tag == MEMLINK_TAG_DEL) {
            *data |= 0x02; 
        }else{
            *data &= 0xfd; 
        }
    
        //DINFO("tag data:%x\n", *data);
        if ( (v & 0x02) == 2 && tag == MEMLINK_TAG_RESTORE) {
            dbk->tagdel_count --;
            dbk->visible_count ++;
        }
        if ( (v & 0x02) == 0 && tag == MEMLINK_TAG_DEL) {
            dbk->tagdel_count ++;
            dbk->visible_count --;
        }
    }else{
        return MEMLINK_ERR_REMOVED;
    }

    return MEMLINK_OK;
}

/**
 * change attr for a value
 */
int
hashtable_attr(HashTable *ht, char *tbname, char *key, void *value, uint32_t *attrarray, int attrnum)
{
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        return MEMLINK_ERR_NOTABLE;
    }

    char        *item = NULL;
    HashNode    *node = NULL;

    int  ret = table_find_value(tb, key, value, &node, NULL, &item);
    if (ret < 0) {
        return ret;
    }
    
    if (tb->attrnum != attrnum) {
        return MEMLINK_ERR_ATTR;
    }

    char attr[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};
    char attrflag[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};

    int  len = attr_array2binary(table_attrformat(tb), attrarray, attrnum, attr);
    if (len <= 0) {
        DINFO("attr array2binary error: %d\n", attrnum);
        return MEMLINK_ERR_ATTR;
    }
    //char buf[128];
    //DINFO("array2bin: %s\n", formatb(attr, len, buf, 128));

    int flen = attr_array2flag(table_attrformat(tb), attrarray, attrnum, attrflag);
    if (flen <= 0) {
        DINFO("attr array2flag error: %d\n", attrnum);
        return MEMLINK_ERR_ATTR;
    }
    //DINFO("array2flag: %s\n", formatb(attrflag, flen, buf, 128));
    dataitem_copy_attr(tb, node, item, attrflag, attr);

    return MEMLINK_OK;
}

int
hashtable_attr_inc(HashTable *ht, char *tbname, char *key, void *value, 
               uint32_t *attrarray, int attrnum)
{
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        return MEMLINK_ERR_NOTABLE;
    }

    char        *item = NULL;
    HashNode    *node = NULL;

    int  ret = table_find_value(tb, key, value, &node, NULL, &item);
    if (ret < 0) {
        return ret;
    }
    
    if (tb->attrnum != attrnum) {
        return MEMLINK_ERR_ATTR;
    }

    uint32_t attrvals[HASHTABLE_ATTR_MAX_ITEM] = {0};
    
    ret = attr_binary2array(table_attrformat(tb), attrnum, item+tb->valuesize, attrvals);
    if (ret <= 0) {
        return MEMLINK_ERR_ATTR;
    }
    int i;
    for (i = 0; i < attrnum; i++) {
        attrvals[i] += attrarray[i];
    }

    //char attrbin[node->attrsize] = {0};
    char attrbin[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};
    ret = attr_array2binary(table_attrformat(tb), attrvals, attrnum, attrbin);
    if (ret <= 0) {
        return MEMLINK_ERR_ATTR;
    }
    memcpy(item+tb->valuesize, attrbin, tb->attrsize);

    return MEMLINK_OK;
}

int
hashtable_range(HashTable *ht, char *tbname, char *key, uint8_t kind, 
            uint32_t *attrarray, int attrnum, 
            int frompos, int len, Conn *conn)
{
    int ret = MEMLINK_OK;
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        ret = MEMLINK_ERR_NOTABLE;
        goto table_range_over;
    }

    char attrval[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};
    char attrflag[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};

    HashNode    *node;
    int         startn;
    char        *wbuf = NULL;
    int         n     = 0;
    //struct timeval start, end;
    
    if (len <= 0) {
        ret =  MEMLINK_ERR_RANGE_SIZE;
        goto table_range_over;
    }

    node = table_find(tb, key);
    if (NULL == node) {
        DINFO("table_find not found node for key:%s\n", key);
        ret = MEMLINK_ERR_NOKEY;
        goto table_range_over;
    }
    if (attrnum > 0) {
        attrnum = attr_array2_binary_flag(table_attrformat(tb), attrarray, attrnum, tb->attrsize, attrval, attrflag);
    }

    int  wlen = CMD_REPLY_HEAD_LEN + 3 + tb->attrnum + (tb->valuesize + tb->attrsize) * len;
    //DINFO("range wlen: %d\n", wlen);
    wbuf = conn_write_buffer(conn, wlen);
    conn->wlen += CMD_REPLY_HEAD_LEN;
    //DINFO("valuesize:%d, attrsize:%d, attrnum:%d\n", node->valuesize, node->attrsize, node->attrnum);
    uint8_t wchar;
    wchar = tb->valuesize;
    conn_write_buffer_append(conn, &wchar, sizeof(char));
    conn_write_buffer_append(conn, &tb->attrsize, sizeof(char));
    wchar = tb->attrnum;
    conn_write_buffer_append(conn, &wchar, sizeof(char));
    if (tb->attrnum > 0) {
        conn_write_buffer_append(conn, table_attrformat(tb), tb->attrnum);
    }

    DataBlock *dbk = NULL;
    int datalen = tb->valuesize + tb->attrsize;
    int dbkpos  = 0;

    //gettimeofday(&start, NULL);
    if (attrnum > 0) {
        startn = dataitem_lookup_pos_attr(tb, node, frompos, kind, attrval, attrflag, &dbk, &dbkpos);
        DINFO("dataitem_lookup_pos_attr startn:%d, dbkpos:%d\n", startn, dbkpos);
        if (startn < 0) { // out of range
            ret = MEMLINK_OK;
            goto table_range_over;
        }
    }else{
        startn = datablock_lookup_pos(node, frompos, kind, &dbk);
        DINFO("datablock_lookup_pos startn:%d, dbk:%p\n", startn, dbk);
        if (startn < 0) { // out of range
            ret = MEMLINK_OK;
            goto table_range_over;
        }
        dbkpos = dataitem_skip2pos(tb, node, dbk, frompos - startn, kind);
    }
    DINFO("dbkpos: %d, kind:%d\n", dbkpos, kind);

    //gettimeofday(&end, NULL);
    //DNOTE("lookup: %d\n", timediff(&start, &end));
    //gettimeofday(&start, NULL);

    n = 0; 
    while (dbk) {
        //DINFO("dbk:%p, count:%d\n", dbk, dbk->data_count);
        if (dbk->data_count == 0) {
            DINFO("data_count is 0, dbk:%p\n", dbk);
            break;
        }

        char *itemdata = dbk->data + dbkpos * datalen;
        int  i;
        for (i = dbkpos; i < dbk->data_count; i++) {
            if (dbkpos > 0)
                dbkpos = 0;

            //modify by lanwenhong
            if (dataitem_have_data(tb, node, itemdata, kind)) {
                char *attrdata = itemdata + tb->valuesize;
                if (attrnum > 0) {
                    int k;
                    for (k = 0; k < tb->attrsize; k++) {
                        if ((attrdata[k] & attrflag[k]) != attrval[k]) {
                            break;
                        }
                    }
                    if (k < tb->attrsize) { // not equal
                        //DINFO("not equal.\n");
                        itemdata += datalen;
                        continue;
                    }
                }
                /*char buf[128];
                snprintf(buf, node->valuesize + 1, "%s", itemdata);
                DINFO("\tok, copy item ... i:%d, value:%s\n", i, buf);*/
                 
                conn_write_buffer_append(conn, itemdata, datalen);
                n += 1;
                if (n >= len) {
                    goto table_range_over;
                }
            }
            itemdata += datalen;
        }
        dbk = dbk->next;
    }
    ret = MEMLINK_OK;

table_range_over:
    DINFO("count: %d\n", n);
    conn_write_buffer_append(conn, &n, sizeof(int));
    //gettimeofday(&end, NULL);
    //DNOTE("over: %d\n", timediff(&start, &end));
    conn_write_buffer_head(conn, ret, conn->wlen);

    return ret;
}

int
hashtable_sortlist_range(HashTable *ht, char *tbname, char *key, uint8_t kind, 
                uint32_t *attrarray, int attrnum, void *valmin, void *valmax,
                Conn *conn)
{
    int ret = MEMLINK_OK;
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        ret = MEMLINK_ERR_NOTABLE;
        goto table_sortlist_range_over;
    }

    char attrval[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};
    char attrflag[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};

    HashNode    *node;
    node = table_find(tb, key);
    if (NULL == node) {
        DINFO("table_find not found node for key:%s\n", key);
        ret = MEMLINK_ERR_NOKEY;
        goto table_sortlist_range_over;
    }
    if (attrnum > 0) {
        attrnum = attr_array2_binary_flag(table_attrformat(tb), attrarray, attrnum, 
                                        tb->attrsize, attrval, attrflag);
    }

    int wlen = 102400;
    conn_write_buffer(conn, wlen);
    conn->wlen += CMD_REPLY_HEAD_LEN;
    conn_write_buffer_append(conn, &tb->valuesize, sizeof(char));
    conn_write_buffer_append(conn, &tb->attrsize, sizeof(char));
    uint8_t wchar;
    wchar = tb->attrnum;
    //conn_write_buffer_append(conn, &node->attrnum, sizeof(char));
    conn_write_buffer_append(conn, &wchar, sizeof(char));
    if (tb->attrnum > 0) {
        conn_write_buffer_append(conn, table_attrformat(tb), tb->attrnum);
    }

    DataBlock *dbk = NULL;
    int datalen = tb->valuesize + tb->attrsize;
    int dbkpos = 0;

    dbkpos = sortlist_lookup(tb, node, MEMLINK_SORTLIST_LOOKUP_STEP, valmin, MEMLINK_VALUE_ALL, &dbk);
    //DINFO("dbk:%p, dbkpos:%d, kind:%d\n", dbk, dbkpos, kind);
    
    int n = 0; 
    char valfirst = ((char*)valmax)[0];
    //DINFO("valfirst: %d, node type:%d\n", valfirst, node->valuetype);
    while (dbk) {
        //DINFO("dbk:%p, count:%d\n", dbk, dbk->data_count);
        if (dbk->data_count == 0) {
            DINFO("data_count is 0, dbk:%p\n", dbk);
            break;
        }
        char *itemdata = dbk->data + dbkpos * datalen;
        int  i;
        for (i = dbkpos; i < dbk->data_count; i++) {
            if (dbkpos > 0)
                dbkpos = 0;

            if (dataitem_have_data(tb, node, itemdata, kind)) {
                if (valfirst != 0 && 
                    (ret = sortlist_valuecmp(tb->valuetype, valmax, itemdata, tb->valuesize)) <= 0) {
                    /*char mybuf[32] = {0};
                    memcpy(mybuf, itemdata, node->valuesize);
                    DINFO("copy end, ret:%d, val:%s, max:%s, %d\n", ret, mybuf, (char*)valmax, 
                                strncmp(valmax, itemdata, node->valuesize));
                    */
                    ret = MEMLINK_OK;
                    goto table_sortlist_range_over;
                }
                if (attrnum > 0) {
                    char *attrdata = itemdata + tb->valuesize;
                    int k;
                    for (k = 0; k < tb->attrsize; k++) {
                        if ((attrdata[k] & attrflag[k]) != attrval[k]) {
                            break;
                        }
                    }
                    if (k < tb->attrsize) {
                        itemdata += datalen;
                        continue;
                    }
                }
                conn_write_buffer_append(conn, itemdata, datalen);
                n += 1;
            }
            itemdata += datalen;
        }
        dbk = dbk->next;
    }
    ret = MEMLINK_OK;
    DINFO("count: %d\n", n);

table_sortlist_range_over:
    conn_write_buffer_append(conn, &n, sizeof(int));
    conn_write_buffer_head(conn, ret, conn->wlen);

    return ret;
}

static int
hashnode_clean(Table *tb, HashNode *node)
{
    if (NULL == node)
        return MEMLINK_ERR_NOKEY;

    DataBlock   *dbk = node->data;
    int         dlen = tb->valuesize + tb->attrsize;

    if (NULL == dbk)
        return MEMLINK_OK;
        
    DINFO("=== table clean:%s used:%d all:%d ===\n", node->key, node->used, node->all);
    if (node->used == 0) { // remove all datablock
        DataBlock *tmp;
        while (dbk) {
            tmp = dbk; 
            dbk = dbk->next;
            mempool_put(g_runtime->mpool, tmp, sizeof(DataBlock) + tmp->data_count * dlen);
        }
        node->all  = 0;
        node->data = NULL;
        return MEMLINK_OK;
    }

    DataBlock   *newroot = NULL, *newlast = NULL;
    DataBlock    *linklast = NULL;
    DataBlock   *oldbk;

    char        *itemdata; // = dbk->data;
    int         i, dataall = 0;

    DataBlock *newdbk     = NULL;
    char      *newdbk_end = NULL;
    char      *newdbk_pos = NULL;
    int          count = 0;
    int          ret;
    int       blockmax = g_cf->block_data_count[g_cf->block_data_count_items - 1];

    while (dbk) {
        itemdata = dbk->data;
        for (i = 0; i < dbk->data_count; i++) {
            ret = dataitem_check_data(tb, node, itemdata);
            if (ret != MEMLINK_VALUE_REMOVED) {
                if (newdbk == NULL || newdbk_pos >= newdbk_end) {
                    newlast    = newdbk;
                    newdbk     = mempool_get2(g_runtime->mpool, blockmax, dlen);
                    newdbk_end = newdbk->data + blockmax * dlen;
                    newdbk_pos = newdbk->data;

                    newdbk->next = NULL;
                    newdbk->prev = newlast;

                    if (newlast) {
                        newlast->next = newdbk; 
                    }else{
                        newroot = newdbk;
                    }
                    dataall += blockmax;
                    count += 1;
                }
                /*char buf[16] = {0};
                memcpy(buf, itemdata, node->valuesize);
                DINFO("clean copy item: %s\n", buf);
                */
                memcpy(newdbk_pos, itemdata, dlen);
                newdbk_pos += dlen;
                if (ret == MEMLINK_VALUE_TAGDEL) {
                    newdbk->tagdel_count++;
                }else{
                    newdbk->visible_count++;
                }
            }
            itemdata += dlen;
        }
        if (count > 0 && count % g_cf->block_clean_num == 0) { // clean num arrivied
            newdbk->next = dbk->next;
            if (dbk->next) { 
                dbk->next->prev = newdbk;
            }
            if (linklast == NULL) { // first
                oldbk = node->data;
                node->data = newroot;
            }else{ // skip the last block, link to linklast->prev
                //oldbk = linklast->next; 
                oldbk = linklast; 
                linklast->prev->next = newroot;
                newroot->prev = linklast->prev;
            }
            linklast = newdbk;
            
            datablock_free(oldbk, dbk, dlen);

            // copy last datablock content to new link
            DataBlock *newdbk2  = mempool_get2(g_runtime->mpool, blockmax, dlen);
            datablock_copy(newdbk2, newdbk, dlen);
            newdbk_pos = newdbk2->data + (newdbk2->tagdel_count + newdbk2->visible_count) * dlen;
            newdbk_end = newdbk2->data + blockmax * dlen;

            newdbk  = newdbk2;
            newroot = newdbk2;
            newlast = NULL;
        }
        dbk = dbk->next;
    }

    if (linklast == NULL) {
        oldbk = node->data;
        node->data = newroot;
    }else{
        //oldbk = linklast->next;
        oldbk = linklast;
        linklast->prev->next = newroot;
        newroot->prev = linklast->prev;
    }
    node->data_tail = newdbk;

    datablock_free(oldbk, dbk, dlen);
    node->all = dataall;

    return MEMLINK_OK;


}
int
hashtable_clean_all(HashTable *ht)
{
    Table *tb = NULL;
    HashNode *node;
    int i, k;
    
    for (k = 0; k < HASHTABLE_MAX_TABLE; k++) { 
        tb = ht->tables[k];
        while (tb) {
            for (i = 0; i < HASHTABLE_BUNKNUM; i++) {
                node = tb->nodes[i];
                while (node) {
                    hashnode_clean(tb, node);
                    node = node->next;
                }
            }
            tb = tb->next;
        }
    }
    return MEMLINK_OK;
}

int
hashtable_clean(HashTable *ht, char *tbname, char *key)
{
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        return MEMLINK_ERR_NOTABLE;
    }
    
    if (key || key[0] == 0) {
        HashNode *node = table_find(tb, key); 
        return hashnode_clean(tb, node);
    }else{
        HashNode *node;
        int i;
        for (i = 0; i < HASHTABLE_BUNKNUM; i++) {
            node = tb->nodes[i];
            while (node) {
                hashnode_clean(tb, node);
                node = node->next;
            }
        }
    }
    return 0;
} 

int
hashtable_stat_table(HashTable *ht, char *tbname, HashTableStatSys *stat)
{
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        return MEMLINK_ERR_NOTABLE;
    }

    HashNode    *node;
    int i;
    int keys    = 0;//统计memlink中key的数量
    int blocks  = 0;//统计memlink中分配的存储块数量
    int dataall = 0;//统计memlink中小块的数量
    int datau   = 0;//统计memlink中value的数量
    int memu    = 0;//统计memlink中hash talbe占用的内存总量
    int datalen = tb->attrsize + tb->valuesize;

    //DINFO("sizeof DataBlock:%d, HashNode:%d\n", sizeof(DataBlock), sizeof(HashNode));
    for (i = 0; i < HASHTABLE_BUNKNUM; i++) {
        node = tb->nodes[i];
        if (node) {
            memu += strlen(node->key) + 1;
            if (tb->attrnum >= sizeof(void*)) {
                memu += tb->attrnum + 1;
            }
        }
        while (node) {
            keys++;    
            datau   += node->used;
            dataall += node->all;
            DataBlock *dbk = node->data;
            
            while (dbk != NULL) {
                blocks++;
                dbk = dbk->next;
                
                memu += sizeof(DataBlock);
                memu += datalen * dbk->data_count;
            }
            
            memu += sizeof(HashNode);    
            node = node->next;
        }
    }
    //MemPool *mp = g_runtime->mpool;

    stat->keys   = keys;
    stat->values = datau;
    stat->blocks = blocks;
    stat->data_all = dataall;
    stat->ht_mem   = memu;

    return MEMLINK_OK;
}


int
hashtable_stat_sys(HashTable *ht, HashTableStatSys *stat)
{
    Table       *tb = NULL;
    HashNode    *node;
    int i, k;
    int keys    = 0;//统计memlink中key的数量
    int blocks  = 0;//统计memlink中分配的存储块数量
    int dataall = 0;//统计memlink中小块的数量
    int datau   = 0;//统计memlink中value的数量
    int memu    = 0;//统计memlink中hash talbe占用的内存总量

    //DINFO("sizeof DataBlock:%d, HashNode:%d\n", sizeof(DataBlock), sizeof(HashNode));
    for (k = 0; k < HASHTABLE_MAX_TABLE; k++) {
        tb = ht->tables[k]; 
        if (tb == NULL) 
            continue;
        while (tb) {
            for (i = 0; i < HASHTABLE_BUNKNUM; i++) {
                node = tb->nodes[i];
                if (node) {
                    memu += strlen(node->key) + 1 + tb->attrnum;
                }
                while (node) {
                    keys++;    
                    datau   += node->used;
                    dataall += node->all;
                    memu += sizeof(HashNode);    
                    node = node->next;
                }
            }
            tb = tb->next;
        }
    }
    MemPool *mp = g_runtime->mpool;
    for (i = 0; i < mp->used; i++) {
        DNOTE("i: %d, mp->freemem[i].total: %d, mp->freemem[i].block_count: %d\n", i, mp->freemem[i].total, mp->freemem[i].block_count);
        DINFO("mp->freemem[%d].memsize: %d\n", i, mp->freemem[i].memsize);
        //memu += mp->freemem[i].ht_use * mp->freemem[i].memsize;
        memu += (mp->freemem[i].total - mp->freemem[i].block_count) * mp->freemem[i].memsize;
        blocks += (mp->freemem[i].total - mp->freemem[i].block_count);
    }

    stat->keys   = keys;
    stat->values = datau;
    stat->blocks = blocks;
    stat->data_all = dataall;
    stat->ht_mem   = memu;
    //stat->pool_blocks  = g_runtime->mpool->blocks;

    return MEMLINK_OK;
}

int
hashtable_stat(HashTable *ht, char *tbname, char *key, HashTableStat *stat)
{
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        return MEMLINK_ERR_NOTABLE;
    }

    HashNode    *node;
    node = table_find(tb, key);
    if (NULL == node) {
        DWARNING("table_stat not found key: %s\n", key);
        return MEMLINK_ERR_NOKEY;
    }

    stat->valuesize = tb->valuesize;
    stat->attrsize  = tb->attrsize;
    stat->data      = node->all;
    stat->data_used = node->used;
    stat->mem       = 0;
    
    int blocks   = 0;
    int blockmem = 0;
    int blockall = 0;

    DataBlock *dbk = node->data;
    while(dbk) {
        blockmem += sizeof(DataBlock) + (tb->attrsize + tb->valuesize) * dbk->data_count;
        blocks++;
        blockall += dbk->data_count;
        dbk = dbk->next;
    }
    stat->blocks = blocks;
    stat->mem = blockmem + sizeof(HashNode);

    if (node->all != blockall) {
        DERROR("stat all error, node->all:%d, blockall:%d\n", node->all, blockall);
    }

    DINFO("valuesize:%d, attrsize:%d, blocks:%d, data:%d, data_used:%d, mem:%d\n", 
            stat->valuesize, stat->attrsize, stat->blocks, stat->data, stat->data_used,
            stat->mem);

    return MEMLINK_OK;
}

int
hashtable_count(HashTable *ht, char *tbname, char *key, uint32_t *attrarray, int attrnum, 
                int *visible_count, int *tagdel_count)
{
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        return MEMLINK_ERR_NOTABLE;
    }

    char attrval[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};
    char attrflag[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};
    HashNode    *node;
    int         vcount = 0, mcount = 0;

    node = table_find(tb, key);
    if (NULL == node) {
        DINFO("table_count not found node for key:%s\n", key);
        return MEMLINK_ERR_NOKEY;
    }

    if (attrnum > 0) {
        attrnum = attr_array2_binary_flag(table_attrformat(tb), attrarray, attrnum, tb->attrsize, attrval, attrflag);
    }

    if (attrnum > 0) {
        int i, ret, k;
        int datalen = tb->valuesize + tb->attrsize;
        DataBlock *dbk = node->data;
        while (dbk) {
            if (dbk->data_count == 0) {
                break;
            }
            char *itemdata = dbk->data;
            for (i = 0; i < dbk->data_count; i++) {
                //DINFO("dbk:%p node:%p, itemdata:%p\n", dbk, node, itemdata);
                ret = dataitem_check_data(tb, node, itemdata);
                if (ret != MEMLINK_VALUE_REMOVED) {
                    char *attrdata = itemdata + tb->valuesize;
                    // check attr equal
                    for (k = 0; k < tb->attrsize; k++) {
                        if ((attrdata[k] & attrflag[k]) != attrval[k]) {
                            break;
                        }
                    }
                    if (k < tb->attrsize) { // not equal
                        itemdata += datalen;
                        continue;
                    }
                    if (ret == MEMLINK_VALUE_VISIBLE) {
                        vcount ++;
                    }else{
                        mcount ++;
                    }
                }
                itemdata += datalen;
            }
            dbk = dbk->next;
        }
    }else{
        DataBlock *dbk = node->data;
        while (dbk) {
            vcount += dbk->visible_count;
            mcount += dbk->tagdel_count;
            dbk = dbk->next;
        }
    }

    *visible_count = vcount;
    *tagdel_count  = mcount;

    return MEMLINK_OK;
}

int
hashtable_sortlist_count(HashTable *ht, char *tbname, char *key, uint32_t *attrarray, int attrnum, 
                     void* valmin, void *valmax, int *visible_count, int *tagdel_count)
{
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        return MEMLINK_ERR_NOTABLE;
    }

    char attrval[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};
    char attrflag[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};
    HashNode    *node;
    DataBlock   *dbk = NULL;
    int         vcount = 0, mcount = 0;
    int         ret, pos;

    node = table_find(tb, key);
    if (NULL == node) {
        DINFO("table_count not found node for key:%s\n", key);
        return MEMLINK_ERR_NOKEY;
    }
    
    if (((char*)valmin)[0] == 0) {
        dbk = node->data;
        pos = 0;
    }else{
        ret = sortlist_lookup(tb, node, MEMLINK_SORTLIST_LOOKUP_STEP, valmin, MEMLINK_VALUE_ALL, &dbk);
        if (ret < 0)
            return ret;
        pos = ret;
    }
    
    if (attrnum > 0) {
        attrnum = attr_array2_binary_flag(table_attrformat(tb), attrarray, attrnum, tb->attrsize, attrval, attrflag);
    }
    
    //table_print(ht, key);
    char firstval = ((char*)valmax)[0];
    DINFO("max first value:%d, dbk:%p pos:%d\n", firstval, dbk, pos);
    int i, k;
    int datalen = tb->valuesize + tb->attrsize;
    while (dbk) {
        if (dbk->data_count == 0) {
            break;
        }
        char *itemdata = dbk->data + pos * datalen;
        for (i = pos; i < dbk->data_count; i++) {
            //DINFO("dbk:%p node:%p, itemdata:%p\n", dbk, node, itemdata);
            if (pos > 0)
                pos = 0;
            ret = dataitem_check_data(tb, node, itemdata);
            if (ret != MEMLINK_VALUE_REMOVED) {
                if (firstval != 0 && 
                    sortlist_valuecmp(tb->valuetype, valmax, itemdata, tb->valuesize) <= 0) {
                    /*char mybuf[128] = {0};
                    memcpy(mybuf, itemdata, node->valuesize);
                    DINFO("count end, ret:%d, val:%s, max:%s\n", ret, mybuf, (char*)valmax);
                    */ 
                    goto table_sortlist_count_over;
                }
                
                if (attrnum > 0) {
                    char *attrdata = itemdata + tb->valuesize;
                    for (k = 0; k < tb->attrsize; k++) {
                        if ((attrdata[k] & attrflag[k]) != attrval[k]) {
                            break;
                        }
                    }
                    if (k < tb->attrsize) { // not equal
                        itemdata += datalen;
                        continue;
                    }
                }
                if (ret == MEMLINK_VALUE_VISIBLE) {
                    vcount ++;
                }else{
                    mcount ++;
                }
            }
            itemdata += datalen;
        }
        dbk = dbk->next;
    }

table_sortlist_count_over:
    *visible_count = vcount;
    *tagdel_count  = mcount;

    return MEMLINK_OK;
}

int 
hashtable_lpush(HashTable *ht, char *tbname, char *key, void *value, uint32_t *attrarray, char attrnum)
{
    return hashtable_insert(ht, tbname, key, value, attrarray, attrnum, 0);
}

int 
hashtable_rpush(HashTable *ht, char *tbname, char *key, void *value, uint32_t *attrarray, char attrnum)
{
    return hashtable_insert(ht, tbname, key, value, attrarray, attrnum, INT_MAX);
}

// pop value from head. queue not support tagdel, attr
int
hashtable_lpop(HashTable *ht, char *tbname, char *key, int num, Conn *conn)
{
    int ret = MEMLINK_OK;
    int idx = 0;
    char *wbuf = NULL;

    if (num <= 0) {
        ret =  MEMLINK_ERR_PARAM;
        goto table_lpop_end;
    }

    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        ret = MEMLINK_ERR_NOTABLE;
        goto table_lpop_end;
    }

    HashNode    *node;
    node = table_find(tb, key);
    if (NULL == node) {
        DINFO("table_find not found node for key:%s\n", key);
        ret = MEMLINK_ERR_NOKEY;
        goto table_lpop_end;
    }

    int  wlen = CMD_REPLY_HEAD_LEN + 3 + tb->attrnum + (tb->valuesize + tb->attrsize) * num;
    DINFO("range wlen: %d\n", wlen);
    if (conn) {
        //DINFO("conn is true.\n");
        wbuf = conn_write_buffer(conn, wlen);
    }
    idx += CMD_REPLY_HEAD_LEN;

    DataBlock *dbk = node->data;
    DataBlock *newdbk = NULL;
    DataBlock *startdbk = dbk, *enddbk = NULL;
    char      *itemdata;
    int datalen = tb->valuesize + tb->attrsize;
    int n     = 0; 
    int i;
    int rmn   = 0;  // delete value count in whole block

    if (conn) {
        memcpy(wbuf + idx, &tb->valuesize, sizeof(char));
        idx += sizeof(char);
        memcpy(wbuf + idx, &tb->attrsize, sizeof(char));
        idx += sizeof(char);
        uint8_t wchar;
        wchar = tb->attrnum;
        //memcpy(wbuf + idx, &node->attrnum, sizeof(char));
        memcpy(wbuf + idx, &wchar, sizeof(char));
        idx += sizeof(char);
        if (tb->attrnum > 0) {
            memcpy(wbuf + idx, table_attrformat(tb), tb->attrnum);
        }
        idx += tb->attrnum;
    }else{
        idx += 3 + tb->attrnum;
    }

    while (dbk) {
        itemdata = dbk->data;
        int dbkcpnv = 0;
        for (i = 0; i < dbk->data_count; i++) {
            ret = dataitem_check_data(tb, node, itemdata);
            if (ret == MEMLINK_VALUE_VISIBLE) {
                /*char buf[128];
                snprintf(buf, node->valuesize + 1, "%s", itemdata);
                DINFO("\tok, copy item ... i:%d, value:%s\n", i, buf);
                */    
                if (conn) {
                    memcpy(wbuf + idx, itemdata, datalen);
                }
                idx += datalen;
                n += 1;
                dbkcpnv++;
                if (n >= num) { // copy complete!
                    if (i < dbk->data_count - 1) {
                        newdbk = mempool_get(g_runtime->mpool, sizeof(DataBlock) + dbk->data_count * datalen);
                        newdbk->data_count    = dbk->data_count;
                        newdbk->visible_count = dbk->visible_count - dbkcpnv; 
                        char *todata = newdbk->data + datalen * (i + 1);
                        node->data   = newdbk;
                        newdbk->next = dbk->next;
                        if (dbk->next) {
                            dbk->next->prev = newdbk;
                        }
                        DINFO("copy todata from: %d, count:%d\n", i, dbk->data_count - i - 1);
                        memcpy(todata, itemdata + datalen, (dbk->data_count - i - 1) * datalen);
                        //enddbk = dbk;
                    }else{ // current datablock is all poped
                        rmn += dbk->data_count;
                        node->data = dbk->next;
                        if (dbk->next) {
                            dbk->next->prev = NULL;
                        }
                    }
                    enddbk = dbk->next; 

                    if (dbk->next == NULL) {
                        node->data_tail = newdbk;
                    }
                    node->all  -= rmn;
                    node->used -= n;
                    ret = MEMLINK_OK;
                    goto table_lpop_over;
                }
            }
            itemdata += datalen;
        }
        rmn += dbk->data_count;
        dbk = dbk->next;
    }
    enddbk     = NULL;
    node->all  = 0;
    node->used = 0;
    node->data = node->data_tail = NULL;
    ret = MEMLINK_OK;
    
table_lpop_over:
    DINFO("count: %d\n", n);
    if (n > 0) {
        datablock_free(startdbk, enddbk, datalen);
    }

table_lpop_end:
    if (conn) {
        memcpy(wbuf + idx, &n, sizeof(int));
        idx += sizeof(int);
        conn_write_buffer_head(conn, ret, idx);
        //char bufw[4096] = {0};
        //DINFO("bufw:%s\n", formath(conn->wbuf, conn->wlen, bufw, 4096));
    }

    return ret;
}

int
hashtable_rpop(HashTable *ht, char *tbname, char *key, int num, Conn *conn)
{
    int ret = MEMLINK_OK;
    int idx = 0;
    char *wbuf = NULL;
    
    if (num <= 0) {
        ret =  MEMLINK_ERR_PARAM;
        goto table_rpop_end;
    }

    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        ret = MEMLINK_ERR_NOTABLE;
        goto table_rpop_end;
    }

    HashNode    *node;
    node = table_find(tb, key);
    if (NULL == node) {
        DINFO("table_find not found node for key:%s\n", key);
        ret = MEMLINK_ERR_NOKEY;
        goto table_rpop_end;
    }

    int  wlen = CMD_REPLY_HEAD_LEN + 3 + tb->attrnum + (tb->valuesize + tb->attrsize) * num;
    DINFO("range wlen: %d\n", wlen);
    if (conn) {
        //DINFO("conn is true.\n");
        wbuf = conn_write_buffer(conn, wlen);
    }
    idx += CMD_REPLY_HEAD_LEN;

    DataBlock *dbk = node->data_tail;
    DataBlock *newdbk = NULL;
    DataBlock *startdbk = dbk, *enddbk = NULL;
    char      *itemdata;
    int datalen = tb->valuesize + tb->attrsize;
    int n     = 0; 
    int i;
    int rmn   = 0;  // delete value count in whole block

    if (conn) {
        memcpy(wbuf + idx, &tb->valuesize, sizeof(char));
        idx += sizeof(char);
        memcpy(wbuf + idx, &tb->attrsize, sizeof(char));
        idx += sizeof(char);
        uint8_t wchar;
        wchar = tb->attrnum;
        //memcpy(wbuf + idx, &node->attrnum, sizeof(char));
        memcpy(wbuf + idx, &wchar, sizeof(char));
        idx += sizeof(char);
        if (tb->attrnum > 0) {
            memcpy(wbuf + idx, table_attrformat(tb), tb->attrnum);
            idx += tb->attrnum;
        }
    }else{
        idx += 3 + tb->attrnum;
    }

    while (dbk) {
        itemdata = dbk->data + ((dbk->data_count - 1) * datalen);
        int dbkcpnv = 0;
        //for (i = 0; i < dbk->data_count; i++) {
        for (i = dbk->data_count - 1; i >= 0; i--) {
            ret = dataitem_check_data(tb, node, itemdata);
            if (ret == MEMLINK_VALUE_VISIBLE) {
                /*char buf[128];
                snprintf(buf, node->valuesize + 1, "%s", itemdata);
                DINFO("\tok, copy item ... i:%d, value:%s\n", i, buf);*/
                    
                if (conn) {
                    memcpy(wbuf + idx, itemdata, datalen);
                }
                idx += datalen;
                n += 1;
                dbkcpnv++;
                if (n >= num) { // copy complete!
                    if (i > 0) {
                        newdbk = mempool_get(g_runtime->mpool, sizeof(DataBlock) + dbk->data_count * datalen);
                        newdbk->data_count    = dbk->data_count;
                        newdbk->visible_count = dbk->visible_count - dbkcpnv; 
                        node->data_tail = newdbk;
                        newdbk->prev = dbk->prev;
                        if (dbk->prev) {
                            dbk->prev->next = newdbk;
                        }
                        DINFO("copy todata count:%d\n", i);
                        memcpy(newdbk->data, dbk->data, i * datalen);
                    }else{ // current datablock is all poped
                        rmn += dbk->data_count;
                        node->data_tail = dbk->prev;
                        if (dbk->prev) {
                            dbk->prev->next = NULL;
                        }
                    }
                    enddbk = dbk->prev; 

                    if (dbk->prev == NULL) {
                        node->data = NULL;
                    }
                    node->all  -= rmn;
                    node->used -= n;
                    ret = MEMLINK_OK;
                    goto table_rpop_over;
                }
            }
            itemdata -= datalen;
        }
        rmn += dbk->data_count;
        dbk = dbk->prev;
    }
    enddbk     = NULL;
    node->all  = 0;
    node->used = 0;
    node->data = node->data_tail = NULL;
    ret = MEMLINK_OK;
    
table_rpop_over:
    DINFO("count: %d\n", n);
    if (n > 0) {
        datablock_free_inverse(startdbk, enddbk, datalen);
    }

table_rpop_end:
    if (conn) {
        memcpy(wbuf + idx, &n, sizeof(int));
        idx += sizeof(int);

        conn_write_buffer_head(conn, ret, idx);
        //char bufw[4096] = {0};
        //DINFO("bufw:%s\n", formath(conn->wbuf, conn->wlen, bufw, 4096));
    }
    return ret;
}

int 
table_check(Table *tb, char *key)
{
    HashNode    *node;

    node = table_find(tb, key);
    if (NULL == node) {
        DERROR("not found key: %s\n", key);
        return MEMLINK_ERR_NOKEY;
    }

    return hashnode_check(tb, node);
}

int 
hashnode_check(Table *tb, HashNode *node)
{
    DINFO("------ check HashNode key:%s, valuesize:%d, attrsize:%d, attrnum:%d, all:%d, used:%d\n",
                node->key, tb->valuesize, tb->attrsize, tb->attrnum, node->all, node->used);

    DataBlock *dbk  = node->data;
    DataBlock *prev = NULL;
    int blocks = 0;
    
    zz_check(node);
    zz_check(node->key);

    while (dbk) {
        zz_check(dbk);

        blocks += 1;
        DINFO("====== dbk %p visible:%d, tagdel:%d, block:%d, count:%d ======\n", dbk, 
                        dbk->visible_count, dbk->tagdel_count, blocks, dbk->data_count);
        if (prev != dbk->prev) {
            DERROR("prev link error! blocks:%d, %p, prev:%p, dbk->prev:%p\n", 
                        blocks, dbk, prev, dbk->prev);
        }

        prev = dbk;
        dbk = dbk->next;
    }
    DINFO("------ blocks: %d\n", blocks);

    return MEMLINK_OK;
}


int 
table_print(Table *tb, char *key)
{
    HashNode    *node;

    node = table_find(tb, key);
    if (NULL == node) {
        DINFO("not found key: %s\n", key);
        return MEMLINK_ERR_NOKEY;
    }

    DINFO("------ HashNode key:%s, valuesize:%d, attrsize:%d, attrnum:%d, all:%d, used:%d\n",
                node->key, tb->valuesize, tb->attrsize, tb->attrnum, node->all, node->used);

    int i;
    unsigned char *attrformat = table_attrformat(tb);
    for (i = 0; i < tb->attrnum; i++) {
        DINFO("attrformat: %d, %d\n", i, attrformat[i]);
    }

    DataBlock *dbk  = node->data;
    DataBlock *prev = NULL;
    int blocks = 0;
    
    zz_check(node);
    zz_check(node->key);

    while (dbk) {
        zz_check(dbk);

        blocks += 1;
        DINFO("====== dbk %p visible:%d, tagdel:%d, block:%d, count:%d ======\n", dbk, 
                        dbk->visible_count, dbk->tagdel_count, blocks, dbk->data_count);
        datablock_print(tb, node, dbk);
        
        if (prev != dbk->prev) {
            DERROR("prev link error! blocks:%d, %p, prev:%p, dbk->prev:%p\n", 
                        blocks, dbk, prev, dbk->prev);
        }

        prev = dbk;
        dbk = dbk->next;
    }
    DINFO("------ blocks: %d\n", blocks);

    return MEMLINK_OK;
}

//add by lanwenhong
int 
hashtable_del_by_attr(HashTable *ht, char *tbname, char *key, uint32_t *attrarray, int attrnum)
{
    Table *tb = hashtable_find_table(ht, tbname);
    if (NULL == tb) {
        return MEMLINK_ERR_NOTABLE;
    }

    int i, k;
    int count = 0;
    int datalen = 0;
    char attrval[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};
    char attrflag[HASHTABLE_ATTR_MAX_ITEM * HASHTABLE_ATTR_MAX_BYTE] = {0};
    HashNode *node;
    
    node = table_find(tb, key);
    if (NULL == node) {
        DINFO("table_del_by_attr not found node for key:%s\n", key);
        return MEMLINK_ERR_NOKEY;
    }
    
    if (tb->attrnum != attrnum) {
        return MEMLINK_ERR_ATTR;
    }
    datalen = tb->valuesize + tb->attrsize;
    if (attrnum > 0) {
        attr_array2_binary_flag(table_attrformat(tb), attrarray, attrnum, tb->attrsize, attrval, attrflag);
    }
    DataBlock *root = node->data;

    while (root) {
        char *itemdata = root->data;

        for (i = 0; i < root->data_count; i++) {
            if (dataitem_have_data(tb, node, itemdata, 0)) {
                char *attrdata = itemdata + tb->valuesize;

                for (k = 0; k < tb->attrsize; k++) {
                    if ((attrdata[k] & attrflag[k]) != attrval[k]) {
                        break;
                    }
                }
                //not equal
                if (k < tb->attrsize) {
                    itemdata += datalen;
                    continue;
                } else {//equal
                    if ((attrdata[0] & 0x03) == 1) {
                        root->visible_count--;
                    }
                    if ((attrdata[0] & 0x03) == 3) {
                        root->tagdel_count--;
                    }
                    attrdata[0] = attrdata[0] & 0x00;
                    count++;
                }
            }
            itemdata += datalen;
        }
        root = root->next;
    }
    node->used -= count;

    return count;
}
/**
 * @}
 */
