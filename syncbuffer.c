/**
* synclog 缓存
* @author lanwenhong
* ingroup memlink
* @{
*/
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include "base/zzmalloc.h"
#include "base/defines.h"
#include "syncbuffer.h"
#include "common.h"
#include "base/logfile.h"

SyncBuffer*
syncbuffer_init()
{
    SyncBuffer *sbuffer;

    sbuffer = (SyncBuffer *)zz_malloc(sizeof(SyncBuffer));
    if (sbuffer == NULL)
        return NULL;

    sbuffer->buffer = (char *)zz_malloc(SYNCBUFFER_SIZE * sizeof(char));
    if (sbuffer->buffer == NULL) {
        free(sbuffer);
        return NULL;
    }
    memset(sbuffer->buffer, 0x0, SYNCBUFFER_SIZE);
    
    sbuffer->index_size = 0;
    sbuffer->data_size = 0;
    sbuffer->index_pos = 0;
    sbuffer->data_pos = SYNCBUFFER_SIZE;

    sbuffer->start_logver = 0;
    sbuffer->last_logver = 0;
    sbuffer->start_logline = 0;
    sbuffer->last_logline = 0;

    return sbuffer;

}

SyncMem*
syncmem_create()
{
    SyncMem *syncmem;

    syncmem = (SyncMem *)zz_malloc(sizeof(SyncMem));
    if (syncmem == NULL)
        return NULL;

    syncmem->main = syncbuffer_init();
    if (syncmem->main == NULL) {
        free(syncmem);
        return NULL;
    }
    syncmem->auxiliary = syncbuffer_init();
    if (syncmem->auxiliary == NULL) {
        free(syncmem);
        free(syncmem->main);
        return NULL;
    }
    int ret = pthread_mutex_init(&syncmem->lock, NULL);
    if (ret == -1) {
        DERROR("pthread_mutex_init error!\n");
        MEMLINK_EXIT;
        return NULL;
    }
    return syncmem;
}

int
syncbuffer_free(SyncBuffer *sbuffer)
{
    if (sbuffer == NULL)
        return -1;

    if (sbuffer->buffer != NULL)
        free(sbuffer->buffer);

    free(sbuffer);
    return 0;
}

int
syncmem_destroy(SyncMem *syncmem)
{
    syncbuffer_free(syncmem->main);
    syncbuffer_free(syncmem->auxiliary);
    pthread_mutex_destroy(&syncmem->lock);
    free(syncmem);

    return 0;
}

int
syncmem_print(SyncMem *mem)
{
    SyncBuffer *sbuffer;
    sbuffer = mem->main;
    int *index = (int *)sbuffer->buffer;
    int i;
    char *data;
    int logver, logline, cmdlen;
    
    DINFO("index_pos: %d\n", sbuffer->index_pos);
    for (i = 0; i < sbuffer->index_pos; i++) {
        int item;
        item = index[i];
        data = sbuffer->buffer + item;
        memcpy(&logver, data, sizeof(int));
        memcpy(&logline, data + sizeof(int), sizeof(int));
        memcpy(&cmdlen, data + sizeof(int) * 2, sizeof(int));
        //DNOTE("=====================logver: %d, logline: %d, cmdlen: %d\n", logver, logline, cmdlen);
    }

    return 0;
}

int
syncmem_write(SyncMem *smem, char *cmd, int len, int logver, int logline)
{
    char *index;
    char *data;
    unsigned char exchange = 0;
    SyncBuffer *sbuffer;
    
    if (smem == NULL || smem->main == NULL)
        return -1;

    sbuffer = smem->main;
    index = sbuffer->buffer + sbuffer->index_size;
    data = sbuffer->buffer + sbuffer->data_pos; 
    
    int checklen = len + sizeof(int) * 3; // logver, logline, index
    DINFO("checklen: %d\n", checklen);
    DINFO("SYNCBUFFER_SIZE - sbuffer->index_size - sbuffer->data_size: %d\n", SYNCBUFFER_SIZE - sbuffer->index_size - sbuffer->data_size);
    DINFO("last_logver: %d, last_logline: %d, start_logver:%d, start_logline: %d\n", sbuffer->last_logver, sbuffer->last_logline, sbuffer->start_logver, sbuffer->start_logline);
    if (SYNCBUFFER_SIZE - sbuffer->index_size - sbuffer->data_size < checklen) {
        DINFO("main buffer is full, need exchange\n");
        //copy第一块中后一半的数据到新的块中
        int  start_index = (sbuffer->index_pos - 1) / 2;
        int  end_index   = sbuffer->index_pos - 1;
        int  *xindex = (int *)sbuffer->buffer;
        char *data_end = sbuffer->buffer + xindex[start_index];
        char *data_start = sbuffer->buffer + xindex[end_index];
        int  end_cmd_len = 0;
        int  copylen = 0;
        int  ns_logver, ns_logline, nl_logver, nl_logline;
        
        DINFO("start_index: %d, end_index: %d\n", start_index, end_index);
        DINFO("xindex[start_index]: %d, xindex[end_index]: %d\n", xindex[start_index], xindex[end_index]);
        memcpy(&end_cmd_len, data_end + sizeof(int) * 2, sizeof(int));
        DINFO("end_cmd_len: %d\n", end_cmd_len);
        copylen = end_cmd_len + sizeof(int) * 3;//加上最后一条命令的大小
        copylen += (xindex[start_index] - xindex[end_index]);
        DINFO("copylen: %d\n", copylen);
        memcpy(&ns_logver,  data_end, sizeof(int));
        memcpy(&ns_logline, data_end + sizeof(int), sizeof(int));
        memcpy(&nl_logver, data_start, sizeof(int));
        memcpy(&nl_logline, data_start + sizeof(int), sizeof(int));
         
        //准备copy到新块
        int move_size = (SYNCBUFFER_SIZE) - (xindex[start_index] + end_cmd_len + sizeof(int) * 3); 
        DINFO("move_size: %d\n", move_size);
        char *newfrom;
        char *newsbuf = smem->auxiliary->buffer;
        
        //copy数据区
        newfrom = newsbuf + SYNCBUFFER_SIZE - copylen;
        memcpy(newfrom, data_start, copylen);
        //copy索引区
        int i;
        int *newindex = (int *)newsbuf;
        int j = 0;
        
        for (i = start_index; i <= end_index; i++) {
            newindex[j] = xindex[i] + move_size;
            j++;
        }
        DINFO("=======j: %d\n", j);
        //交换两块缓冲区
        pthread_mutex_lock(&smem->lock);
        SyncBuffer *tmp;

        tmp = smem->main;
        smem->main = smem->auxiliary;
        smem->auxiliary = tmp;
        //构造新的统计结构
        smem->main->index_pos = j;
        smem->main->data_pos = SYNCBUFFER_SIZE - copylen;
        smem->main->index_size = j * sizeof(int);
        smem->main->data_size = copylen;
        smem->main->start_logver = ns_logver;
        smem->main->start_logline = ns_logline;
        smem->main->last_logver = nl_logver;
        smem->main->last_logline = nl_logline;
        
        exchange = 1;
        pthread_mutex_unlock(&smem->lock);
    } 

    if (exchange == 1) {
        sbuffer = smem->main;
        index = sbuffer->buffer + sbuffer->index_size;
        data = sbuffer->buffer + sbuffer->data_pos; 
    }
    char *where = data - len - sizeof(int) * 2;
    
    memcpy(where, &logver, sizeof(int));
    memcpy(where + sizeof(int), &logline, sizeof(int));
    memcpy(where + sizeof(int) * 2, cmd, len);

    int pos = sbuffer->data_pos - len - sizeof(int) * 2;
    DINFO("sbuffer->data_pos: %d,pos: %d\n", sbuffer->data_pos, pos);
    memcpy(index, &pos, sizeof(int));
    
    if (sbuffer->index_pos == 0) {
        sbuffer->start_logver = logver;
        sbuffer->start_logline = logline;
        sbuffer->last_logver = logver;
        sbuffer->last_logline = logline;
    } else {
        sbuffer->last_logver = logver;
        sbuffer->last_logline = logline;
    }
    DINFO("logver: %d, logline: %d\n", logver, logline);
    DINFO("last_logver: %d, last_logline: %d, start_logver:%d, start_logline: %d\n", sbuffer->last_logver, sbuffer->last_logline, sbuffer->start_logver, sbuffer->start_logline);
    sbuffer->data_pos = pos;
    sbuffer->index_pos += 1;
    sbuffer->data_size += len + sizeof(int) * 2;
    sbuffer->index_size += sizeof(int);
    DINFO("index_pos: %d, data_pos: %d, index_size: %d, data_size: %d\n", sbuffer->index_pos,
        sbuffer->data_pos, sbuffer->index_size, sbuffer->data_size);
    
    int *check = (int *)sbuffer->buffer;
    DINFO("check[index_pos - 1]: %d\n", check[sbuffer->index_pos - 1]);
    //syncmem_print(smem);
    
    return 0;
}
/*
int
syncbuffer_find_indexpos(SyncMem *smem, int logver, int logline)
{

    if (smem == NULL || smem->main == NULL)
        return -1;

    SyncBuffer *sbuffer = smem->main;
    
    DINFO("logver: %d, logline: %d, start_logver:%d, start_logline: %d\n", logver, logline, sbuffer->start_logver, sbuffer->start_logline);
    if (sbuffer == NULL)
        return -2;
    
    if (logver < sbuffer->start_logver) {
        return -3;
    } else if (logver >= sbuffer->start_logver && logver <= sbuffer->last_logver && 
        (logline >= sbuffer->start_logline || logline <= sbuffer->last_logline)) {
        int pos;

        pos = (logver - sbuffer->start_logver) * SYNCLOG_INDEXNUM + logline - sbuffer->start_logline;
        DINFO("==logver: %d, sbuffer->start_logver: %d\n", logver, sbuffer->start_logver);
        DINFO("==pos: %d\n", pos);

        if (pos < 0 || pos > sbuffer->index_pos) {
            DERROR("syncbuffer pos error: %d\n", pos);
            return -6;
        }
        int *index = (int *)sbuffer->buffer;
        int item = index[pos];
        char *data = sbuffer->buffer + item;
        int dlogver, dlogline;
        
        memcpy(&dlogver, data, sizeof(int));
        memcpy(&dlogline, data + sizeof(int), sizeof(int));
        DINFO("==logver: %d, logline: %d, dlogver: %d, dlogline: %d\n", logver, logline, dlogver, dlogline);
        return pos;
        
    } else {
        return -4;
    }
    return -5;
}*/

int
syncbuffer_find_indexpos(SyncMem *smem, int logver, int logline)
{
    uint64_t start_num = 0;
    uint64_t end_num = 0;
    uint64_t cur_num = 0;

    if (smem == NULL || smem->main == NULL)
        return -1;

    SyncBuffer *sbuffer = smem->main;
    DINFO("logver: %d, logline: %d, start_logver:%d, start_logline: %d\n", logver, logline, sbuffer->start_logver, sbuffer->start_logline);
    
    start_num = sbuffer->start_logver;
    start_num = start_num << 32;
    start_num |= sbuffer->start_logline;

    end_num = sbuffer->last_logver;
    end_num = end_num << 32;
    end_num |= sbuffer->last_logline;

    cur_num = logver;
    cur_num = cur_num << 32;
    cur_num |= logline;

    if (cur_num >= start_num && cur_num <= end_num) {
        int pos;

        pos = (logver - sbuffer->start_logver) * SYNCLOG_INDEXNUM + logline - sbuffer->start_logline;
        DINFO("==logver: %d, sbuffer->start_logver: %d\n", logver, sbuffer->start_logver);
        DINFO("==pos: %d\n", pos);

        if (pos < 0 || pos > sbuffer->index_pos) {
            DERROR("syncbuffer pos error: %d\n", pos);
            return -6;
        }
        int *index = (int *)sbuffer->buffer;
        int item = index[pos];
        char *data = sbuffer->buffer + item;
        int dlogver, dlogline;

        memcpy(&dlogver, data, sizeof(int));
        memcpy(&dlogline, data + sizeof(int), sizeof(int));
        DINFO("==logver: %d, logline: %d, dlogver: %d, dlogline: %d\n", logver, logline, dlogver, dlogline);
        return pos;

    }
    return -2;
}

int
syncmem_read(SyncMem *smem, int logver, int logline, 
    int *last_logver, int *last_logline, char *data, int len, char need_skip_one)
{
    int pos;
    int ret = 0;

    pthread_mutex_lock(&smem->lock);
    pos = syncbuffer_find_indexpos(smem, logver, logline);
    DINFO("find in syncbuffer pos: %d\n", pos);
    if (pos < 0) {
        //可能读取之前，两款缓冲已经发生了交换
        DINFO("can not find logver: %d, logline: %d in buffer from index: %d\n", logver, logline, 0);
        ret = -1;
        goto syncbuffer_read_end;
    }
    DINFO("syncbuffer_find_indexpos return: %d\n", pos);
    if (pos == smem->main->index_pos - 1) {
        //没有任何新数据产生
        ret = 1;
        DINFO("not have new data in buffer\n");
        goto syncbuffer_read_end;
    } else {
        int item;
        int i;
        SyncBuffer *sbuffer = smem->main;
        int *index = (int *)sbuffer->buffer;
        int count = 0;
        char *to = data + sizeof(int);
        char *from;
        int copylen = 0;
        int cmdlen = 0;
        int nlogver = 0, nlogline = 0;
        
        /*
        if (logver == 1 && logline == 0)
            i = pos;
        else
            i = pos + 1;*/

        //DERROR("smem->need_skip_one: %d\n", need_skip_one); 
        if (need_skip_one == FALSE) {
            i = pos;
            need_skip_one = TRUE;
        } else {
            i = pos + 1;
        }
        
        for (; i < sbuffer->index_pos; i++) {
            item = index[i]; 
            
            from = sbuffer->buffer + item;
            //检查copy的数据长度，包括logver, logline, cmdlen等
            copylen = 0;
            copylen += sizeof(int) * 3;
            memcpy(&cmdlen, from + sizeof(int) * 2, sizeof(int));
            copylen += cmdlen;

            DINFO("copylen: %d\n", copylen);

            //copy logver, logline, cmdlen
            if (count + copylen > len - sizeof(int)) {
                break;
            }
            memcpy(to, from, copylen);
            to += copylen;
            count += copylen;
            memcpy(&nlogver, from, sizeof(int));
            memcpy(&nlogline, from + sizeof(int), sizeof(int));
        }
        //copy数据包头    
        memcpy(data, &count, sizeof(int));

        //记录最后一条命令的位置， logver, logline
        //*newpos = npos;
        if (nlogver == 0 && nlogline == 0) {
            DERROR("data may be error at pos: %d\n", pos);
        }
        *last_logver = nlogver;
        *last_logline = nlogline;
        DINFO("nlogver: %d, nlogline: %d, i: %d\n", nlogver, nlogline, i);
    }
syncbuffer_read_end:
    pthread_mutex_unlock(&smem->lock);

    return ret;
}

int
syncmem_reset(SyncMem *smem, int logver, int logline, int bcount)
{
    uint64_t end_num = 0;
    uint64_t cur_num = 0;


    if (smem == NULL && smem->main == NULL)
        return -1;

    pthread_mutex_lock(&smem->lock);

    SyncBuffer *sbuffer = smem->main;
    DINFO("logver: %d, logline: %d, start_logver:%d, start_logline: %d\n", logver, logline, sbuffer->start_logver, sbuffer->start_logline);

    end_num = sbuffer->last_logver;
    end_num = end_num << 32;
    end_num |= sbuffer->last_logline;

    cur_num = logver;
    cur_num = cur_num << 32;
    cur_num |= logline;
    
    if (cur_num == end_num && sbuffer->index_pos > bcount) {
        int *index = (int *)sbuffer->buffer;
        int nums = bcount;
        int pos = sbuffer->index_pos - 1;
        while (nums > 0) {
            index[pos] = 0;
            pos--;
            
            char *data = sbuffer->buffer + index[pos];
            int newlastver;
            int newlastline;
            memcpy(&newlastver, data, sizeof(int));
            memcpy(&newlastline, data + sizeof(int), sizeof(int));
            sbuffer->last_logver = newlastver;
            sbuffer->last_logline = newlastline;
            nums--;
        }
    }
    pthread_mutex_unlock(&smem->lock);
    return 0;
}
/**
* @}
*/
