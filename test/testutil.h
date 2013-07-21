#ifndef MEMLINK_TESTUTIL_H
#define MEMLINK_TESTUTIL_H

#include <stdio.h>
#include <memlink_client.h>

#define check_result(m,k,mask,f,l,c,v)  check_result_real(m,k,mask,f,l,c,v,__FILE__,__LINE__)

int check_result_real(MemLink *m, char *key, char **maskstrs, 
                int from, int len, int retcount, int *retval, char *file, int line);


#endif
