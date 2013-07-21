/**
 * 哈希表内部数据块处理
 * @file datablock.c
 * @author zhaowei
 * @ingroup memlink
 * @{
 */

#include <math.h>
#include "datablock.h"
#include "hashtable.h"
#include "logfile.h"
#include "common.h"
#include "utils.h"
#include "zzmalloc.h"
#include "serial.h"
#include "runtime.h"

/**
 * 检查dataitem_check的返回值是否符合kind
 * @param ret  dataitem_check返回值
 * @param kind 预期的kind
 * @return 正确返回MEMLINK_TRUE，否则返回MEMLINK_FALSE
 */
inline int
dataitem_check_kind(int ret, int kind)
{
    if (ret == kind)
        return MEMLINK_TRUE;

    if (kind == MEMLINK_VALUE_ALL && ret != MEMLINK_VALUE_REMOVED) 
        return MEMLINK_TRUE;
    
    return MEMLINK_FALSE;
}


/**
 * 检查数据项中是否有数据
 * 真实删除位为0表示无效数据，为1表示存在有效数据
 * @param node
 * @param itemdata
 * @param kind
 */
inline int 
dataitem_have_data(Table *tb, HashNode *node, char *itemdata, unsigned char kind)
{
    char *attrdata  = itemdata + tb->valuesize;
    
    if (kind == MEMLINK_VALUE_VISIBLE) { // find visible
        if ((*attrdata & (unsigned char)0x03) == 1) {
            return MEMLINK_TRUE;
        }
    }else if (kind == MEMLINK_VALUE_TAGDEL) { // find tagdel
        if ((*attrdata & (unsigned char)0x03) == 3) {
            return MEMLINK_TRUE;
        }
    }else if (kind == MEMLINK_VALUE_ALL) { // find visible and tagdel
        if ((*attrdata & (unsigned char)0x01) == 1) {
            return MEMLINK_TRUE;
        }
    }else if (kind == MEMLINK_VALUE_REMOVED) {
        if ((*attrdata & (unsigned char)0x01) == 0) {
            return MEMLINK_TRUE;
        }
    }
    return MEMLINK_FALSE;
}

inline int
dataitem_check(char *itemdata, int valuesize)
{
    char *attrdata  = itemdata + valuesize;
    int  delm = *attrdata & (unsigned char)0x01;
    int  tagm = *attrdata & (unsigned char)0x02;
   
    if (delm == 0)
        return MEMLINK_VALUE_REMOVED;
    if (tagm == 2)
        return MEMLINK_VALUE_TAGDEL;

    return MEMLINK_VALUE_VISIBLE;
}

/**
 * check value type
 */
inline int
dataitem_check_data(Table *tb, HashNode *node, char *itemdata)
{
    return dataitem_check(itemdata, tb->valuesize);
}


int
dataitem_skip2pos(Table *tb, HashNode *node, DataBlock *dbk, int skip, unsigned char kind)
{
    int datalen = tb->valuesize + tb->attrsize;
    int i;
    char *data;
    
    //DINFO("skip2pos, skip:%d, data count:%d\n", skip, dbk->data_count);
    //if (skip < 0 || skip > g_cf->block_data_count[g_cf->block_data_count_items - 1]) { // last
    if (skip < 0 || skip >= dbk->data_count) { // last
        return -1;
    }

    data = dbk->data;
    for (i = 0; i < dbk->data_count; i++) {
        if (skip == 0)
            return i;
        if (dataitem_have_data(tb, node, data, kind)) {
            skip--;
        }
        data += datalen;
    }
    
    if (skip == 0)
        return i;
    return -1;  
}

/**
 * find one data in link of datablock
 * @param node
 * @param value
 * @param dbk   返回一个包含此value的DataBlock
 * @return      返回此value在DataBlock中的地址
 */
char*
dataitem_lookup(Table *tb, HashNode *node, void *value, DataBlock **dbk)
{
    int i;
    int datalen = tb->valuesize + tb->attrsize;
    DataBlock *root = node->data;

    while (root) {
        char *data = root->data;
        //DINFO("root: %p, data: %p, next: %p\n", root, data, root->next);
        for (i = 0; i < root->data_count; i++) {
            if (dataitem_have_data(tb, node, data, 0) && memcmp(value, data, tb->valuesize) == 0) {
                if (dbk) {
                    *dbk = root;
                }
                return data;
            }
            data += datalen;
        }
        root = root->next;
    }
    return NULL;
}

/**
 * 在DataBlock链中查找value
 * @param node  value相关的HashNode
 * @param value 要查找的value
 * @param dbk   value所在DataBlock
 * @return      value在DataBlock中的位置
 */
int
dataitem_lookup_pos(Table *tb, HashNode *node, void *value, DataBlock **dbk)
{
    int i;
    int datalen = tb->valuesize + tb->attrsize;
    DataBlock *root = node->data;

    while (root) {
        char *data = root->data;
        //DINFO("root: %p, data: %p, next: %p\n", root, data, root->next);
        for (i = 0; i < root->data_count; i++) {
            if (dataitem_have_data(tb, node, data, 0) && memcmp(value, data, tb->valuesize) == 0) {
                if (dbk) {
                    *dbk = root;
                }
                return i;
            }
            data += datalen;
        }
        root = root->next;
    }
    return MEMLINK_ERR_NOVAL;
}

inline int 
sortlist_valuecmp(unsigned char type, void *v1, void *v2, int size)
{
    //DINFO("value type:%d\n", type);
    switch(type) {
        case MEMLINK_VALUE_INT:
            return *(int*)v1 - *(int*)v2; 
        case MEMLINK_VALUE_UINT:
            return *(unsigned int*)v1 - *(unsigned int*)v2; 
        case MEMLINK_VALUE_LONG:
            return *(long*)v1 - *(long*)v2; 
        case MEMLINK_VALUE_ULONG:
            return *(unsigned long*)v1 - *(unsigned long*)v2; 
        case MEMLINK_VALUE_FLOAT: {
            float v = *(float*)v1 - *(float*)v2; 
            if ((v > 0 && v < 0.00001) || (v < 0 && v > 0.00001))
                return 0;
            return (int)v; 
        }
        case MEMLINK_VALUE_DOUBLE: {
            double v = *(double*)v1 - *(double*)v2; 
            if ((v > 0 && v < 0.00001) || (v < 0 && v > 0.00001))
                return 0;
            return (int)v;
        }
        case MEMLINK_VALUE_STRING:
            //DINFO("compare string: %s, %s, size:%d\n", (char*)v1, (char*)v2, size);
            return strncmp(v1, v2, size);
        default:
            return memcmp(v1, v2, size);
    }
}

/**
 * 和DataBlockz中第一个数据比较
 */
static inline int
sortlist_datablock_check(Table *tb, HashNode *node, DataBlock *dbk, void *value, int kind, 
                        int datalen, int direction, int *pos)
{
    char *data;
    int i, ret;
    //char buf[128];

    if (direction == MEMLINK_FIND_ASC) {
        data = dbk->data;
        for (i = 0; i < dbk->data_count; i++) {
            //DINFO("check first to last i:%d, node value:%d, value:%d\n", i, *(int*)data, *(int*)value);
            if (dataitem_have_data(tb, node, data, kind)) {
                *pos = i;
                ret = sortlist_valuecmp(tb->valuetype, value, data, tb->valuesize);
                //DINFO("ret: %d\n", ret);
                //memcpy(buf, data, node->valuesize);
                //DINFO("1 valuecmp: %s %s, ret:%d\n", (char*)value, buf, ret);
                //DINFO("1 valuecmp: %s %s, ret:%d\n", (char*)value, formath(value, node->valuesize, buf, 128), ret);
                return ret;
            }
            data += datalen;
        }
    }else{
        data = dbk->data + (dbk->data_count - 1) * datalen;
        for (i = dbk->data_count - 1; i >= 0; i--) {
            //DINFO("check last to first i:%d\n", i);
            if (dataitem_have_data(tb, node, data, kind)) {
                *pos = i;
                ret = sortlist_valuecmp(tb->valuetype, value, data, tb->valuesize);
                //DINFO("ret: %d\n", ret);
                //memcpy(buf, data, node->valuesize);
                //DINFO("1 valuecmp: %s %s, ret:%d\n", (char*)value, buf, ret);
                //DINFO("1 valuecmp: %s %s, ret:%d\n", (char*)value, formath(value, node->valuesize, buf, 128), ret);
                return ret;
            }
            data -= datalen;
        }
    }
    DINFO("check return noval.\n");
    return MEMLINK_ERR_NOVAL;
}

/**
 * check all value in datablock.
 * @param node
 * @param dbk
 * @param value
 * @param kind
 * @param datalen
 * @param direction
 * @param cmptype
 */
static inline int
sortlist_datablock_check_all(Table *tb, HashNode *node, DataBlock *dbk, void *value, 
                    int kind, int datalen, int direction, int cmptype)
{
    char *data;
    int i, ret;

    if (direction == MEMLINK_FIND_ASC) {
        data = dbk->data;
        for (i = 0; i < dbk->data_count; i++) {
            if (dataitem_have_data(tb, node, data, kind)) {
                ret = sortlist_valuecmp(tb->valuetype, value, data, tb->valuesize);
                if (cmptype == MEMLINK_CMP_RANGE) {
                    if (ret <= 0)
                        return i;
                }else{ // equal
                    if (ret < 0) {
                        return MEMLINK_ERR_NOVAL_ALL;
                    }
                    if (ret == 0)
                        return i;
                }
            }
            data += datalen;
        }
    }else{
        data = dbk->data + (dbk->data_count - 1)* datalen;
        for (i = dbk->data_count - 1; i >= 0; i--) {
            if (dataitem_have_data(tb, node, data, kind)) {
                ret = sortlist_valuecmp(tb->valuetype, value, data, tb->valuesize);
                if (cmptype == MEMLINK_CMP_RANGE) {
                    if (ret >= 0) {
                        return i + 1;
                    }
                }else{ // equal
                    if (ret > 0) {
                        return MEMLINK_ERR_NOVAL_ALL;
                    }
                    if (ret == 0)
                        return i;
                }
            }
            data -= datalen;
        }
    }
    return MEMLINK_ERR_NOVAL;
}

int
datablock_suitable_size(int blocksize)
{
    int i;

    for (i = 0; i < g_cf->block_data_count_items; i++) {
        if (blocksize <= g_cf->block_data_count[i])
            return g_cf->block_data_count[i];
    }
    return g_cf->block_data_count[g_cf->block_data_count_items - 1];
}

int
datablock_last_valid_pos(Table *tb, HashNode *node, DataBlock *dbk, int kind, int datalen)
{
    char* data = dbk->data + (dbk->data_count - 1) * datalen;
    int i;
    for (i = dbk->data_count - 1; i >= 0; i--) {
        if (dataitem_have_data(tb, node, data, kind)) {
            return i + 1;
        }
        data -= datalen;
    }
    return 0;
}

DataBlock* 
datablock_step_skip(DataBlock *bk, int step)
{
    int i;
    DataBlock *lastbk;

    for (i = 0; i < step; i++) {
        lastbk = bk;
        bk = bk->next;
        if (bk == NULL) {
            return lastbk;
        }
    }
    return bk; 
}


/**
 * find a value position
 * @param node  node with the list
 * @param step  skip blocks when check
 * @param value check value
 * @param kind  type of value: visible/tagdel/all
 * @param dbk   datablock contain the value
 * @return  value position in datablock
 */
int
sortlist_lookup(Table *tb, HashNode *node, int step, void *value, int kind, DataBlock **dbk)
{
    int ret, pos = 0;
    int datalen = tb->valuesize + tb->attrsize;
    DataBlock *checkbk = node->data;

    if (NULL == node->data) {
        *dbk = NULL;
        return MEMLINK_ERR_NOVAL;
    }
    // find in datablock list
    while (checkbk) {
        DINFO("checkbk: %p, next: %p\n", checkbk, checkbk->next);
        //datablock_print(node, checkbk);
        //DINFO("++++++++++++++++++++++++++++++\n");
        ret = sortlist_datablock_check(tb, node, checkbk, value, kind, datalen, MEMLINK_FIND_ASC, &pos);
        //DINFO("ret:%d\n", ret);
        if (ret == 0) { // equal
            *dbk = checkbk;
            return pos;
        }else if (ret < 0) { // smaller
            goto sortlist_lookup_found;
        }
        DataBlock *endbk = datablock_step_skip(checkbk, step);
        if (checkbk == endbk) { // not skip
            checkbk = endbk;
            break;
        }
        checkbk = endbk;
    }

sortlist_lookup_found:
    //DINFO("reverse find datablock.\n");
    while (checkbk) {
        ret = sortlist_datablock_check_all(tb, node, checkbk, value, kind, datalen, 
                                        MEMLINK_FIND_DESC, MEMLINK_CMP_EQUAL);
        DINFO("check all: %d\n", ret);
        if (ret == MEMLINK_ERR_NOVAL_ALL) { // smaller found, but no equal
            return MEMLINK_ERR_NOVAL;
        }
        if (ret >= 0) {
            *dbk = checkbk;
            return ret;
        }
        checkbk = checkbk->prev;
    }
    
    return MEMLINK_ERR_NOVAL;
}

/**
 * find one valid position in link of datablock
 * @param node
 * @param step
 * @param value
 * @param kind
 * @param dbk
 * @return
 */
int
sortlist_lookup_valid(Table *tb, HashNode *node, int step, void *value, int kind, DataBlock **dbk)
{
    int ret, pos = 0;
    int datalen = tb->valuesize + tb->attrsize;
    DataBlock *checkbk = node->data;
    //DataBlock *startbk = node->data;

    if (NULL == node->data) {
        *dbk = NULL;
        return 0;
    }
    // smaller than first data
    if (sortlist_datablock_check(tb, node, node->data, value, kind, 
                                datalen, MEMLINK_FIND_ASC, &pos) < 0) {
        //DINFO("check 1, pos:%d\n", pos);
        *dbk = node->data;
        return pos;
    }
    // bigger than last data
    if (sortlist_datablock_check(tb, node, node->data_tail, value, kind, 
                                datalen, MEMLINK_FIND_DESC, &pos) >= 0) {
        //DINFO("check 2, pos:%d\n", pos);
        *dbk = node->data_tail;
        if (pos == node->data_tail->data_count - 1) {
            return -1;
        }
        return pos + 1;
    }

    // find in datablock list
    while (checkbk) {
        //DINFO("checkbk: %p, next: %p\n", checkbk, checkbk->next);
        ret = sortlist_datablock_check(tb, node, checkbk, value, kind, datalen, MEMLINK_FIND_ASC, &pos);
        if (ret == 0) { // equal
            *dbk = checkbk;
            return pos + 1;
        }else if (ret < 0) { // smaller
            goto sortlist_lookup_valid_found;
        }

        DataBlock *endbk = datablock_step_skip(checkbk, step);
        if (checkbk == endbk) { // not skip
            checkbk = endbk;
            break;
        }
        checkbk = endbk;
    }

sortlist_lookup_valid_found:
    //DINFO("reverse find datablock.\n");
    while (checkbk) {
        ret = sortlist_datablock_check_all(tb, node, checkbk, value, kind, datalen, 
                                        MEMLINK_FIND_DESC, MEMLINK_CMP_RANGE);
        //DINFO("check all: %d\n", ret);
        if (ret != MEMLINK_ERR_NOVAL) {
            if (ret == checkbk->data_count) {
                if (checkbk->next) {
                    *dbk = checkbk->next;
                    return 0;
                }else{ // append new block at last
                    *dbk = checkbk;
                    return -1;
                }
            }
            *dbk = checkbk;
            return ret;
        }
        checkbk = checkbk->prev;
    }
    *dbk = node->data;
    return 0;
}

/**
 * copy a value, attr to special address
 * @param node  HashNode be copied
 * @param addr  to address
 * @param value copied value
 * @param attr  copied attr, binary
 */
int
dataitem_copy(Table *tb, HashNode *node, char *addr, void *value, void *attr)
{
    //char *m = attr;
    //*m = *m | (*addr & 0x03);

    //DINFO("dataitem_copy valuesize: %d, attrsize: %d\n", node->valuesize, node->attrsize);
    memcpy(addr, value, (size_t)tb->valuesize);
    memcpy(addr + tb->valuesize, attr, tb->attrsize);
    
    return MEMLINK_OK;
}

/**
 * set attr to a value
 */
int
dataitem_copy_attr(Table *tb, HashNode *node, char *addr, char *attrflag, char *attr)
{
    //char *m = attr;
    //*m = *m | (*addr & 0x03);
    int i;
    char *attraddr = addr + tb->valuesize;
    //char buf[128];
    //DINFO("copy_attr before: %s\n", formatb(addr, node->attrsize, buf, 128)); 

    for (i = 0; i < tb->attrsize; i++) {
        attraddr[i] = (attraddr[i] & attrflag[i]) | attr[i];
    }
    //memcpy(addr + node->valuesize, attr, node->attrsize);
    //DINFO("copy_attr after: %s\n", formatb(addr, node->attrsize, buf, 128)); 

    return MEMLINK_OK;
}

/**
 * find a position with attr in datablock link
 */
int
dataitem_lookup_pos_attr(Table *tb, HashNode *node, int pos, unsigned char kind, 
                        char *attrval, char *attrflag, DataBlock **dbk, int *dbkpos)
{
    DataBlock *root = node->data;

    int n = 0, startn = 0;
    int i, k;
    int datalen = tb->valuesize + tb->attrsize;
    
    while (root) {
        startn = n;
        char *itemdata = root->data;

        for (i = 0; i < root->data_count; i++) {
            if (dataitem_have_data(tb, node, itemdata, kind)) {
                char *attrdata = itemdata + tb->valuesize;    
                
                /*char buf[64], buf2[64], buf3[64];
                DINFO("i: %d, attrdata: %s, attrflag: %s, attrval: %s\n", i, 
                        formatb(attrdata, node->attrsize, buf, 64), 
                        formatb(attrflag, node->attrsize, buf2, 64),
                        formatb(attrval, node->attrsize, buf3, 64));
                */ 
                for (k = 0; k < tb->attrsize; k++) {
                    //DINFO("check k:%d, attrdata:%x, attrflag:%x, attrval:%x\n", k, attrdata[k], attrflag[k], attrval[k]);
                    if ((attrdata[k] & attrflag[k]) != attrval[k]) {
                        break;
                    }
                }
                if (k < tb->attrsize) { // not equal
                    //DINFO("not equal.\n");
                    itemdata += datalen;
                    continue;
                }

                if (n >= pos) {
                    *dbk  = root;
                    //*addr = itemdata;
                    *dbkpos = i;
                    return startn;
                    //return i;
                }
                n += 1;
            }
            itemdata += datalen;
        }
        /*
        if (n >= pos) {
            *dbk = root;
            return startn;
        }*/

        root = root->next;
    }

    return -1;
}


/**
 * print info in a datablock
 */
int
datablock_print(Table *tb, HashNode *node, DataBlock *dbk)
{
    int  bufsize = 2048;
    char valuebuf[2048];
    char attrbuf[2048];
    int  i, ret;
    int  datalen   = tb->valuesize + tb->attrsize;
    char *itemdata = dbk->data; 
    //char format[128];
    char delinfo[32] = "";

    for (i = 0; i < dbk->data_count; i++) {
        /*if (dataitem_have_data(node, itemdata, 1)) {
            memcpy(buf2, itemdata, node->valuesize);
            buf2[node->valuesize] = 0;
            DINFO("i: %03d, value: %s, attr: %s\n", i, buf2, 
                    formath(itemdata + node->valuesize, node->attrsize, buf1, 128));
        }else if (dataitem_have_data(node, itemdata, 0)) {
            memcpy(buf2, itemdata, node->valuesize);
            buf2[node->valuesize] = 0;
            DINFO("i: %03d, value: %s, attr: %s\tdel\n", i, buf2, 
                    formath(itemdata + node->valuesize, node->attrsize, buf1, 128));
        }else{
            DINFO("i: %03d, no data, attr: %s\n", i, formath(itemdata + node->valuesize, node->attrsize, buf1, 128));
        }*/
        ret = dataitem_check_data(tb, node, itemdata);
        if (ret == MEMLINK_VALUE_TAGDEL) { // tagdel
            strcpy(delinfo, "del");
        }else if (ret == MEMLINK_VALUE_REMOVED){
            strcpy(delinfo, "null");
        }else{
            delinfo[0] = 0;
        }
           
        formath(itemdata + tb->valuesize, tb->attrsize, attrbuf, bufsize);
        switch(tb->valuetype) {
            case MEMLINK_VALUE_INT4:
                DINFO("i:%03d, value:%d, attr:%s\t%s\n", i, *(int*)itemdata, attrbuf, delinfo);
                break;
            case MEMLINK_VALUE_UINT4:
                DINFO("i:%03d, value:%u, attr:%s\t%s\n", i, *(unsigned int*)itemdata, attrbuf, delinfo);
                break;
            case MEMLINK_VALUE_INT8:
                DINFO("i:%03d, value:%lld, attr:%s\t%s\n", i, *(long long*)itemdata, attrbuf, delinfo);
                break;
            case MEMLINK_VALUE_UINT8:
                DINFO("i:%03d, value:%llu, attr:%s\t%s\n", i, *(unsigned long long*)itemdata, attrbuf, delinfo);
                break;
            case MEMLINK_VALUE_FLOAT4:
            case MEMLINK_VALUE_FLOAT8:
                DINFO("i:%03d, value:%.2f, attr:%s\t%s\n", i, *(double*)itemdata, attrbuf, delinfo);
                break;
            case MEMLINK_VALUE_STRING:
                strncpy(valuebuf, itemdata, tb->valuesize);
                valuebuf[(int)tb->valuesize] = 0;
                DINFO("i:%03d, value:%s, attr:%s\t%s\n", i, valuebuf, attrbuf, delinfo);
                break;
            case MEMLINK_VALUE_OBJ:
                formath(itemdata, tb->valuesize, valuebuf, bufsize);
                DINFO("i:%03d, value:%s, attr:%s\t%s\n", i, valuebuf, attrbuf, delinfo);
                break;
            default:
                return MEMLINK_ERR;
        }
        itemdata += datalen;
    }
    return MEMLINK_OK;
}

/**
 * delete a value in datablock
 */
int
datablock_del(Table *tb, HashNode *node, DataBlock *dbk, char *data)
{
    char *mdata = data + tb->valuesize;
    unsigned char v = *mdata & 0x02;

    *mdata &= 0xfe;
    if (v == 2) {
        dbk->tagdel_count--;
    }else{
        dbk->visible_count--;
    }
    node->used--;

    return 0;
}

/**
 * restore a value in datablock
 */
int
datablock_del_restore(Table *tb, HashNode *node, DataBlock *dbk, char *data)
{
    char *mdata = data + tb->valuesize;
    unsigned char v = *mdata & 0x02;

    *mdata |= 0x01;
    if (v == 2) {
        dbk->tagdel_count++;
    }else{
        dbk->visible_count++;
    }
    node->used++;

    return 0;
}
/**
 * 在数据链中找到某位置所在数据块, 
 * @param node 
 * @param pos
 * @param kind
 * @param dbk 指定位置所在数据块
 */
int
datablock_lookup_pos(HashNode *node, int pos, unsigned char kind, DataBlock **dbk)
{
    DataBlock *root = node->data;

    int n = 0, startn = 0;
    
    while (root) {
        startn = n;
        //modify by lanwenhong
        if (kind == MEMLINK_VALUE_VISIBLE) {
            n += root->visible_count;
        }else if (kind == MEMLINK_VALUE_ALL) {
            n += root->tagdel_count + root->visible_count;
        }else if (kind == MEMLINK_VALUE_TAGDEL) {
            n += root->tagdel_count;
        }
    
        //if (n >= pos) {
        if (n > pos) {
            *dbk = root;
            return startn;
        }
        root = root->next;
    }

    return -1;
}


/**
 * find a position in datablock link
 */
int
datablock_lookup_valid_pos(HashNode *node, int pos, unsigned char kind, DataBlock **dbk)
{
    DataBlock *root = node->data;
    int n = 0, startn = 0;
    
    while (root) {
        startn = n;
        if (kind == MEMLINK_VALUE_VISIBLE) {
            n += root->visible_count;    
        }else if (kind == MEMLINK_VALUE_ALL) {
            n += root->tagdel_count + root->visible_count;
        }else if (kind == MEMLINK_VALUE_TAGDEL) {
            n += root->tagdel_count;
        }
        if (n > pos) {
            *dbk = root;
            return startn;
        }
        if (root->next == NULL) {
            *dbk = root;
            return startn;
        }
        root = root->next;
    }
    // no datablock in node
    return -1;
}

/**
 * create a new small datablock and add  value
 */
/*
DataBlock*
datablock_new_copy_small(HashNode *node, void *value, void *attr)
{
    int datalen = node->valuesize + node->attrsize;

    DataBlock *newbk = mempool_get(g_runtime->mpool, sizeof(DataBlock) + datalen);
    newbk->data_count = 1;
    newbk->next  = NULL;
    DINFO("create small newbk:%p\n", newbk);
    
    if (NULL != value) {
        dataitem_copy(node, newbk->data, value, attr);
        newbk->visible_count = 1;
    }
    //node->data = newbk;
    //node->all += newbk->data_count;
    
    return newbk;
}
*/

// check the position in datablock is null
int
datablock_check_null(Table *tb, HashNode *node, DataBlock *dbk, int skipn, void *value, void *attr)
{
    int datalen     = tb->valuesize + tb->attrsize;
    int count       = dbk->visible_count + dbk->tagdel_count;
    char *fromdata  = dbk->data;
    int i, n = 0;
    
    if (dbk->next == NULL && skipn > count) {
        //DINFO("change skipn %d to:%d\n", skipn, count);
        skipn = count;
    }

    for (i = 0; i < dbk->data_count; i++) {
        if (n == skipn) {
            if (dataitem_have_data(tb, node, fromdata, MEMLINK_VALUE_ALL) == MEMLINK_FALSE) {
                dataitem_copy(tb, node, fromdata, value, attr);
                dbk->visible_count++;
                node->used++;
                return 1;
            }
            return 0;
        }
        if (dataitem_have_data(tb, node, fromdata, MEMLINK_VALUE_ALL) == MEMLINK_TRUE) {
            n++;
        }

        fromdata += datalen;
    }

    return 0;
}

int
datablock_check_null_pos(Table *tb, HashNode *node, DataBlock *dbk, int pos, void *value, void *attr)
{
    if (pos < 0 || pos > dbk->data_count) {
        return 0;
    }
    int datalen     = tb->valuesize + tb->attrsize;
    char *fromdata  = dbk->data + datalen * pos;

    if (dataitem_have_data(tb, node, fromdata, MEMLINK_VALUE_ALL) == MEMLINK_FALSE) {
        dataitem_copy(tb, node, fromdata, value, attr);
        dbk->visible_count++;
        node->used++;
        return 1;
    }
    return 0;
}

/**
 * create a new datablock, and copy part of data in old datablock by skip number
 * @param node
 * @param dbk
 * @param skipn
 * @param value
 * @param attr
 */
DataBlock*
datablock_new_copy(Table *tb, HashNode *node, DataBlock *dbk, int skipn, void *value, void *attr)
{
    int datalen = tb->valuesize + tb->attrsize;

    if (dbk == NULL) {
        DataBlock *newbk = mempool_get2(g_runtime->mpool, 1, datalen);
        if (value != NULL) {
            dataitem_copy(tb, node, newbk->data, value, attr);
            newbk->visible_count++;
        }
        return newbk;
    }

    int dbksize = dbk->visible_count + dbk->tagdel_count;
    int newsize = datablock_suitable_size(dbksize + 1);
    DataBlock *newbk = mempool_get2(g_runtime->mpool, newsize, datalen);

    DINFO("create newbk:%p, dbk:%p\n", newbk, dbk);
    int  n = 0;
    char *todata     = newbk->data;
    char *end_todata = newbk->data + newbk->data_count * datalen;
    char *fromdata   = dbk->data;
    
    int i, ret;
    for (i = 0; i < dbk->data_count; i++) {
        ret = dataitem_check_data(tb, node, fromdata);
        if (ret != MEMLINK_VALUE_REMOVED) {
            if (todata >= end_todata) {
                break;
            }
            if (n == skipn) {
                dataitem_copy(tb, node, todata, value, attr);
                todata += datalen;
                n++;
                newbk->visible_count++;

                if (todata >= end_todata) {
                    break;
                }
            }
            memcpy(todata, fromdata, datalen);    
            todata += datalen;
            n++;

            if (ret == MEMLINK_VALUE_VISIBLE) {
                newbk->visible_count++;
            }else{
                newbk->tagdel_count++;
            }
        }
        fromdata += datalen;
    }

    if (n <= skipn && todata < end_todata) {
        dataitem_copy(tb, node, todata, value, attr);
        newbk->visible_count++;
    }
    newbk->next = dbk->next;
    newbk->prev = dbk->prev;

    return newbk;
}

/**
 * create a new datablock, and copy part of data in old datablock by pos
 * @param node hashnode with the list
 * @param dbk
 * @param pos
 * @param value
 * @param attr
 ( @return new block
 */
DataBlock*
datablock_new_copy_pos(Table *tb, HashNode *node, DataBlock *dbk, int pos, void *value, void *attr)
{
    int datalen = tb->valuesize + tb->attrsize;
    if (dbk == NULL) { // create new datablock
        DataBlock *newbk = mempool_get2(g_runtime->mpool, 1, datalen); 
        if (value != NULL) {
            DINFO("copy value 1. %s\n", (char*)value);
            dataitem_copy(tb, node, newbk->data, value, attr);
            newbk->visible_count++;
        }
        return newbk;
    }
    zz_check(dbk);
    //DINFO("pos:%d, data_count:%d\n", pos, dbk->data_count);
    int dbksize = dbk->visible_count + dbk->tagdel_count;
    if (pos < 0) { // append
        if (dbksize < dbk->data_count) {
            int  i;
            char *posdata = dbk->data + (dbk->data_count - 1) * datalen;
            for (i = dbk->data_count - 1; i >= 0; i--) {
                if (dataitem_check_data(tb, node, posdata) != MEMLINK_VALUE_REMOVED) {
                    /*if (i == dbk->data_count - 1) { // datablock last have data, break
                        break;
                    }else*/ if (i < dbk->data_count - 1) {
                        DINFO("copy value 2.1 %s\n", (char*)value);
                        posdata += datalen;
                        dataitem_copy(tb, node, posdata, value, attr);
                        dbk->visible_count++;
                        return dbk;
                    }
                }
                posdata -= datalen;
            }
        }
    }else if (pos < dbk->data_count) {
        char *posdata = dbk->data + pos * datalen;
        if (dataitem_check_data(tb, node, posdata) == MEMLINK_VALUE_REMOVED) { // copy data
            DINFO("copy value 2.2 %s\n", (char*)value);
            dataitem_copy(tb, node, posdata, value, attr);
            dbk->visible_count++;
            return dbk;
        }
    }

    int  n = 0;
    int  newsize     = datablock_suitable_size(dbksize + 1);
    DataBlock *newbk = mempool_get2(g_runtime->mpool, newsize, datalen);
    char *todata     = newbk->data;
    char *end_todata = newbk->data + newbk->data_count * datalen;
    char *fromdata   = dbk->data;
    int  i, ret;
    int  iscopy = 0;

    for (i = 0; i < dbk->data_count; i++) {
        if (i == pos) {
            DINFO("copy value 3. %d, %s\n", i, (char*)value);
            dataitem_copy(tb, node, todata, value, attr);
            todata += datalen;
            n++;
            newbk->visible_count++;
            iscopy = 1;
            if (todata >= end_todata) {
                break;
            }
        }
        ret = dataitem_check_data(tb, node, fromdata);
        if (ret != MEMLINK_VALUE_REMOVED) {
            if (todata >= end_todata) {
                break;
            }
            memcpy(todata, fromdata, datalen);    
            todata += datalen;
            n++;
            if (ret == MEMLINK_VALUE_VISIBLE) {
                newbk->visible_count++;
            }else{
                newbk->tagdel_count++;
            }
        }
        fromdata += datalen;
    }

    if (iscopy == 0 && todata < end_todata) {
        DINFO("copy value 4. %d, %s\n", i, (char*)value);
        dataitem_copy(tb, node, todata, value, attr);
        newbk->visible_count++;
    }
    newbk->next = dbk->next;
    newbk->prev = dbk->prev;
    return newbk;
}


/**
 * copy data in one datablock to another
 */
inline int
datablock_copy(DataBlock *tobk, DataBlock *frombk, int datalen)
{
    memcpy(tobk, frombk, sizeof(DataBlock) + tobk->data_count * datalen);
    return MEMLINK_OK; 
}

/*
 * copy data used from one datablock to another
 */
inline int
datablock_copy_used(Table *tb, HashNode *node, DataBlock *tobk, int topos, DataBlock *frombk)
{
    int  datalen   = tb->valuesize + tb->attrsize;
    char *fromdata = frombk->data;
    char *todata   = tobk->data + topos * datalen;
    //char *todata_end = tobk->data + tobk->data_count * datalen;
    int  i, ret;
    
    for (i = 0; i < frombk->data_count; i++) {
        ret = dataitem_check_data(tb, node, fromdata);
        if (ret != MEMLINK_VALUE_REMOVED) {
            memcpy(todata, fromdata, datalen);
            todata += datalen;
            topos++;
            if (ret == MEMLINK_VALUE_VISIBLE) {
                tobk->visible_count++;
            }else{
                tobk->tagdel_count++;
            }
        }
        fromdata += datalen;
    }
    return topos; 
}

inline int
datablock_copy_used_blocks(Table *tb, HashNode *node, DataBlock *tobk, int topos, 
                           DataBlock *frombk, int blockcount)
{
    int i;
    for (i = 0; i < blockcount; i++) {
        //DINFO("copy datablock values:%p\n", frombk);
        topos = datablock_copy_used(tb, node, tobk, topos, frombk);
        frombk = frombk->next;
        if (frombk == NULL) {
            //DERROR("datablock frombk is NULL. i:%d, blocks:%d\n", i, blockcount);
            break;
        }
    }
    return topos;
}

int
datablock_resize(Table *tb, HashNode *node, DataBlock *dbk)
{
    int blockmax = g_cf->block_data_count[g_cf->block_data_count_items - 1];
    int datalen  = tb->valuesize + tb->attrsize;  
    int dbksize  = dbk->visible_count + dbk->tagdel_count;
    int prevsize = 0;
    int nextsize = 0;

    int prevdcnt = 0;
    int nextdcnt = 0;
    int dbkdcnt  = dbk->data_count;
    
    //DINFO("============== resize dbk:%p, vs:%d, ts:%d\n", dbk, dbk->visible_count, dbk->tagdel_count);
    if (dbk->prev) {
        prevsize = dbk->prev->visible_count + dbk->prev->tagdel_count;
        prevdcnt = dbk->prev->data_count;
    }
    if (dbk->next) {
        nextsize = dbk->next->visible_count + dbk->next->tagdel_count;
        nextdcnt = dbk->next->data_count;
    }
    //DINFO("prev:%p, current:%p, next:%p\n", dbk->prev, dbk, dbk->next);
    //DINFO("presize:%d, dbksize:%d, nextsize:%d\n", prevsize, dbksize, nextsize);

    DataBlock *start = NULL, *end = NULL;
    int  num = 0;
    int  newsize = datablock_suitable_size(dbksize);


    //DINFO("newsize:%d\n", newsize);
    if (prevsize > 0 && nextsize > 0 && prevsize + dbksize + nextsize < blockmax) {
        newsize = datablock_suitable_size(prevsize + dbksize + nextsize);
        start = dbk->prev;
        end   = dbk->next;
        num   = 3;
        node->all = node->all - prevdcnt - dbkdcnt - nextdcnt + newsize;
        DINFO("merge 3 block, newsize:%d\n", newsize);
    }else if (prevsize > 0 && prevsize + dbksize < blockmax) {
        newsize = datablock_suitable_size(prevsize + dbksize);
        start = dbk->prev;
        end   = dbk;
        num   = 2;
        node->all = node->all - prevdcnt - dbkdcnt + newsize;
        DINFO("merge prev, prevsize:%d, newsize:%d\n", prevsize, newsize);
    }else if (nextsize > 0 && dbksize + nextsize < blockmax) {
        newsize = datablock_suitable_size(dbksize + nextsize);
        start = dbk;
        end   = dbk->next;
        num   = 2;
        node->all = node->all - dbkdcnt - nextdcnt + newsize;
        DINFO("merge next: nextsize:%d, newsize:%d\n", nextsize, newsize);
    }else if (newsize < dbk->data_count) {
        int rate = (int)ceil(g_cf->block_data_reduce * dbk->data_count);
        int checksize = 1;
        int i;

        //checksize = datablock_suitable_size(dbkdcnt - 1);
        for (i = 0; i < g_cf->block_data_count_items; i++) {
            if (g_cf->block_data_count[i] < dbkdcnt)
                checksize = g_cf->block_data_count[i];
        }
        DINFO("checksize: %d\n", checksize);
        if (checksize - dbksize >= rate) {
            DINFO("block reduce to small, oldsize:%d, newsize:%d\n", dbk->data_count, newsize);
            start = dbk;
            end   = dbk;
            num   = 1;
            node->all = node->all - dbkdcnt + newsize;
        }
    }

    //DINFO("start:%p end:%p num:%d\n", start, end, num); 
    if (start) {
        //DINFO("start->next:%p\n", start->next);
        DataBlock *newbk = mempool_get2(g_runtime->mpool, newsize, datalen);
        //DINFO("resize newbk:%p, count:%d\n", newbk, newbk->data_count);
        //DINFO("1 newbk prev:%p, next:%p\n", newbk->prev, newbk->next);
        datablock_copy_used_blocks(tb, node, newbk, 0, start, num);   
        //DINFO("2 newbk prev:%p, next:%p, ret:%d\n", newbk->prev, newbk->next, ret);

        int i;
        DataBlock *tmp;
        /*tmp = start;
        for (i = 0; i < num; i++) {
            zz_check(tmp);
            tmp = tmp->next;
        }*/

        //datablock_print(node, newbk);
        datablock_link_prev(node, start, newbk);
        datablock_link_next(node, end, newbk);

        /*tmp = start;
        for (i = 0; i < num; i++) {
            zz_check(tmp);
            tmp = tmp->next;
        }*/

        for (i = 0; i < num; i++) {
            //DINFO("mem put:%p, i:%d, num:%d\n", start, i, num);
            tmp = start->next; 
            mempool_put2(g_runtime->mpool, start, datalen);
            start = tmp;
        }
        //DINFO("after resize ...\n");
        //datablock_print(node, newbk);
    }

    return MEMLINK_OK;
}


/**
 * free datablock
 */
int
datablock_free(DataBlock *headbk, DataBlock *tobk, int datalen)
{
    DataBlock   *tmp;

    while (headbk && headbk != tobk) {
        tmp = headbk->next;
        mempool_put(g_runtime->mpool, headbk, sizeof(DataBlock) + headbk->data_count * datalen);
        headbk = tmp;
    }
    return MEMLINK_OK;
}

/**
 * free datablock from backend, by loop dbk->prev pointer
 */
int
datablock_free_inverse(DataBlock *startbk, DataBlock *endbk, int datalen)
{
    DataBlock   *tmp;

    while (startbk && startbk != endbk) {
        tmp = startbk->prev;
        mempool_put(g_runtime->mpool, startbk, sizeof(DataBlock) + startbk->data_count * datalen);
        startbk = tmp;
    }
    return MEMLINK_OK;
}

int
attr_array2_binary_flag(unsigned char *attrformat, unsigned int *attrarray, int attrnum, 
                        int attrsize, char *attrval, char *attrflag)
{
    int j;
    for (j = 0; j < attrnum; j++) {
        if (attrarray[j] != UINT_MAX)
            break;
    }
    //DINFO("j: %d, attrnum: %d\n", j, attrnum);
    
    int attrlen;
    if (j < attrnum) {
        attrlen = attr_array2binary(attrformat, attrarray, attrnum, attrval);
        if (attrlen <= 0) {
            DINFO("attr_string2array error\n");
            return -2;
        }
        attrval[0] = attrval[0] & 0xfc;

        //char buf[128];
        //DINFO("len:%d 2binary: %s\n", attrlen, formatb(attrval, attrlen, buf, 128));
        attrlen = attr_array2flag(attrformat, attrarray, attrnum, attrflag);
        if (attrlen <= 0) {
            DINFO("attr_array2flag error\n");
            return -2;
        }
        //DINFO("len:%d 2flag: %s\n", attrlen, formatb(attrflag, attrlen, buf, 128));
        for (j = 0; j < attrsize; j++) {
            attrflag[j] = ~attrflag[j];
        }
        attrflag[0] = attrflag[0] & 0xfc;

        //DINFO("len:%d ^2flag: %s\n", attrlen, formatb(attrflag, attrlen, buf, 128));
    }else{
        attrnum = 0;
    }

    return attrnum;
}

/**
 * @}
 */

