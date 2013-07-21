#ifndef __MD5_H
#define __MD5_H

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

typedef struct {
    unsigned int state[4];    
    unsigned int count[2];    
    unsigned char buffer[64];    
} MD5Context;

void md5_init(MD5Context * context);
void md5_update(MD5Context * context, unsigned char * buf, int len);
void md5_final(MD5Context * context, unsigned char digest[16]);
int  md5_file (char * filename, char *bmd5, int bsize);
int  md5(char *md5str, unsigned char *str, int slen);

#endif
