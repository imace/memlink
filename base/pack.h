#ifndef BASE_PACK_H
#define BASE_PACK_H

#include <stdio.h>

int pack(char *buf, int buflen, char *format, ...);
int unpack(char *buf, int buflen, char *format, ...);

#endif
