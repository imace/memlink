#include "testutil.h"
#include "logfile.h"

int check_result_real(MemLink *m, char *name, char *key, char **maskstrs, 
                int from, int len, int retcount, int *retval, char *file, int line)
{
    MemLinkResult result;
    int ret = memlink_cmd_range(m, name, key, MEMLINK_VALUE_ALL, "", from, len, &result);
    if (ret != MEMLINK_OK) {
        DERROR("range error, ret:%d, key:%s, from:%d, len:%d, file:%s:%d\n", 
                    ret, key, from, len, file, line);
        return -1;
    }
    if (result.count != retcount) {
        DERROR("result count error:%d, retcount:%d, file:%s:%d\n", result.count, retcount, file, line);
        return -1;
    }
    MemLinkItem *item = result.root;
    int  i = 0;
    char val[512] = {0};

    while (item) {
        sprintf(val, "%06d", retval[i]);
        if (strcmp(val, item->value) != 0) {
            DERROR("value error, item->value:%s, check value:%s, i:%d, file:%s:%d\n", 
                        item->value, val, i, file, line);
            return -1;
        }
        if (strcmp(maskstrs[retval[i]%3], item->mask) != 0) {
            DERROR("mask error, item->mask:%s, mask:%s, i:%d, file:%s:%d\n", 
                        item->mask, maskstrs[i%3], i, file, line);
            return -1;
        }
        i++;
        item = item->next;
    }
    memlink_result_free(&result);

    return 0;
}


