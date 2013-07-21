/**
 * 内存分配与调试
 * @file zzmalloc.c
 * @author zhaowei
 * @ingroup base
 * @{
 */
#include <stdlib.h>
#include <string.h>
#include "logfile.h"
#include "zzmalloc.h"
#include "utils.h"
#ifdef TCMALLOC
#include <google/tcmalloc.h>
#endif

void*
zz_malloc_dbg(size_t size)
{
    //DNOTE("malloc size:%u\n", (unsigned int)size);
    void *ptr;
    ptr = malloc(size + 12);
    if (NULL == ptr) {
        DERROR("malloc error!\n");
        exit(EXIT_FAILURE);
    }
    char  *b = (char*)ptr;

    *((int*)b) = size;
    *((int*)(b + 4)) = 0x55555555;
    *((int*)(b + 8 + size)) = 0x55555555;
        
    return b + 8;
}

void*
zz_malloc_default(size_t size)
{
    //DNOTE("malloc size:%u\n", (unsigned int)size);
    void *ptr;
#ifdef TCMALLOC
    ptr = tc_malloc(size);
#else
    ptr = malloc(size);
#endif
    if (NULL == ptr) {
        DERROR("malloc error!\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}


void
zz_free_default(void *ptr)
{
#ifdef TCMALLOC
    tc_free(ptr);
#else
    free(ptr);
#endif
}


void
zz_check_dbg(void *ptr, char *file, int line)
{
    if (NULL == ptr) {
        DERROR("check NULL, file:%s, line:%d\n", file, line);
        //exit(EXIT_FAILURE);
        abort();
    }
    char *b = ptr - 8;
    int  size = *((int*)b);

    if (*((int*)(b + 4)) != 0x55555555 || *((int*)(b + 8 + size)) != 0x55555555) {
#ifndef NOLOG
        char buf1[128] = {0};
        char buf2[128] = {0};
#endif
        DERROR("check error! %p, size:%d, file:%s, line:%d, %s, %s\n", ptr, size, file, line, 
                    formatb(ptr-4, 4, buf1, 128), formatb(ptr+size+8, 4, buf2, 128));
        //exit(EXIT_FAILURE);
        abort();
    }
}

void
zz_free_dbg(void *ptr, char *file, int line)
{
    if (NULL == ptr) {
        DERROR("free NULL, file:%s, line:%d\n", file, line);
        abort();
    }
    char *b = ptr - 8;
    zz_check_dbg(ptr, file, line);
    free(b);
}

char*
zz_strdup(char *s)
{
    int len = strlen(s);

    char *ss = (char*)zz_malloc(len + 1);
    strcpy(ss, s);

    return ss;
}

/**
 * @}
 */
