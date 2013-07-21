#include <stdio.h>
#include <string.h>
#include "memlink_client.h"
#include "logfile.h"

int main(int argc, char *argv[])
{
    MemLink *m;
    char key[128];
    logfile_create("stdout", 4);

    m = memlink_create("127.0.0.1", 11011, 11012, 0);
    if (NULL == m) {
        DERROR("memlink_create error!\n");
        return -1;
    }
    
    int ret;
    
    sprintf(key, "%s", "haha1");

    MemLinkResult result;

    ret = memlink_cmd_range(m, key, MEMLINK_VALUE_ALL, "", 0, 1000, &result);
    if (ret != MEMLINK_OK) {
        DERROR("range  error, ret:%d\n", ret);
        return -1;
    }
    if (result.count != 1000) {
        DERROR("range result count error! %d\n", result.count);
        //return -1;
    }
    MemLinkItem *item = result.root;
    while (item) {
        DINFO("%d\n", *(int*)item->value);
        item = item->next;
    }


    memlink_destroy(m);

    return 0;
}

