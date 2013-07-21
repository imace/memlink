#ifndef MEMLINK_DUMPFILE_H
#define MEMLINK_DUMPFILE_H

#include <stdio.h>
#include <limits.h>
#include "hashtable.h"

#define DUMP_FILE_NAME "dump.dat"
#define DUMP_FORMAT_VERSION 1

int  dumpfile(HashTable *ht);
int  dumpfile_load(HashTable *ht, char *filename, int localdump);
void dumpfile_call_loop(int fd, short event, void *arg);
int  dumpfile_call();
int  dumpfile_logver(char *filename, unsigned int *logver, unsigned int *logpos);
int  dumpfile_latest(char *filename);
int  dumpfile_reserve(int num);

#endif
