#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "pack.h"
#include "logfile.h"
#include "utils.h"

int test_int()
{
    int ret;
    char buf[1024] = {0};
    // int
    int a1 = 10; 
    int a2 = 0;

    ret = pack(buf, 1024, "i", a1); 
    DASSERT(ret == 4);
    
    ret = unpack(buf, ret, "i", &a2);
    DASSERT(ret == 4);
    DASSERT(a2 == a1);
    
    ret = pack(buf, 1024, "$4i", a1);
    DASSERT(ret == 8);
    a2 = 0;    
    ret = unpack(buf+4, ret, "i", &a2);
    DASSERT(ret == 4);
    DASSERT(a2 == a1);
    
    // int array
    int array1[3] = {1, 2, 3};
    int array2[3] = {0};
    int alen = 0;
    int arraysize = sizeof(int) * 3;

    ret = pack(buf, 1024, "i3", array1);
    DASSERT(ret == 13);

    ret = unpack(buf, ret, "i3", &alen, array2);
    DASSERT(ret == 13);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    memset(array2, 0, arraysize);
    //alen = 3;

    ret = pack(buf, 1024, "i3:4", array1);
    DASSERT(ret = 16);
    alen = 0; 
    ret = unpack(buf, ret, "i3:4", &alen, array2);
    DASSERT(ret == 16);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    memset(array2, 0, 12);
    alen = 3;

    ret = pack(buf, 1024, "I", alen, array1);
    DASSERT(ret == 13);
    alen = 0;
    ret = unpack(buf, ret, "I", &alen, array2);
    DASSERT(ret == 13);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    memset(array2, 0, 12);
    alen = 3;

    ret = pack(buf, 1024, "I:4", alen, array1);
    DASSERT(ret = 16);
    alen = 0; 
    ret = unpack(buf, ret, "I:4", &alen, array2);
    DASSERT(ret == 16);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);
    
    return 0;
}

int test_long() 
{
    // long
    int ret;
    char buf[1024] = {0};
    int64_t a1 = 10; 
    int64_t a2 = 0;

    ret = pack(buf, 1024, "l", a1); 
    DASSERT(ret == sizeof(a1));
    
    ret = unpack(buf, ret, "l", &a2);
    DASSERT(ret == sizeof(a1));
    DASSERT(a2 == a1);
    
    ret = pack(buf, 1024, "$4l", a1);
    DASSERT(ret == sizeof(a1) + 4);
    a2 = 0;    
    ret = unpack(buf+4, ret, "l", &a2);
    DASSERT(ret == sizeof(a1));
    DASSERT(a2 == a1);
    
    // int array
    int64_t array1[3] = {1, 2, 3};
    int64_t array2[3] = {0};
    int alen = 0;
    int arraysize = sizeof(int64_t) * 3;

    ret = pack(buf, 1024, "l3", array1);
    DASSERT(ret == 1 + arraysize);

    ret = unpack(buf, ret, "l3", &alen, array2);
    //DINFO("ret:%d, arraysize:%d\n", ret, arraysize);
    DASSERT(ret == 1 + arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    memset(array2, 0, arraysize);
    //alen = 3;

    ret = pack(buf, 1024, "l3:4", array1);
    DASSERT(ret = 4 + arraysize);
    alen = 0; 
    ret = unpack(buf, ret, "l3:4", &alen, array2);
    DASSERT(ret == 4 + arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    memset(array2, 0, arraysize);
    alen = 3;

    ret = pack(buf, 1024, "L", alen, array1);
    DASSERT(ret == 1+arraysize);
    alen = 0;
    ret = unpack(buf, ret, "L", &alen, array2);
    DASSERT(ret == 1+arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    memset(array2, 0, arraysize);
    alen = 3;

    ret = pack(buf, 1024, "L:4", alen, array1);
    DASSERT(ret = 4+arraysize);
    alen = 0; 
    ret = unpack(buf, ret, "L:4", &alen, array2);
    DASSERT(ret == 4+arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    return 0;
}

int test_float() 
{
    // long
    int ret;
    char buf[1024] = {0};
    float a1 = 10; 
    float a2 = 0;

    ret = pack(buf, 1024, "f", a1); 
    DASSERT(ret == sizeof(a1));
    
    ret = unpack(buf, ret, "f", &a2);
    DASSERT(ret == sizeof(a1));
    DASSERT(a2 == a1);
    
    ret = pack(buf, 1024, "$4f", a1);
    DASSERT(ret == sizeof(a1) + 4);
    a2 = 0;    
    ret = unpack(buf+4, ret, "f", &a2);
    DASSERT(ret == sizeof(a1));
    DASSERT(a2 == a1);
    
    // int array
    float array1[3] = {1.1, 2.2, 3.3};
    float array2[3] = {0};
    int alen = 0;
    int arraysize = sizeof(float) * 3;

    ret = pack(buf, 1024, "f3", array1);
    DASSERT(ret == 1 + arraysize);

    ret = unpack(buf, ret, "f3", &alen, array2);
    //DINFO("ret:%d, arraysize:%d\n", ret, arraysize);
    DASSERT(ret == 1 + arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    memset(array2, 0, arraysize);
    //alen = 3;

    ret = pack(buf, 1024, "f3:4", array1);
    DASSERT(ret = 4 + arraysize);
    alen = 0; 
    ret = unpack(buf, ret, "f3:4", &alen, array2);
    DASSERT(ret == 4 + arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    memset(array2, 0, arraysize);
    alen = 3;

    ret = pack(buf, 1024, "F", alen, array1);
    DASSERT(ret == 1+arraysize);
    alen = 0;
    ret = unpack(buf, ret, "F", &alen, array2);
    DASSERT(ret == 1+arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    memset(array2, 0, arraysize);
    alen = 3;

    ret = pack(buf, 1024, "F:4", alen, array1);
    DASSERT(ret = 4+arraysize);
    alen = 0; 
    ret = unpack(buf, ret, "F:4", &alen, array2);
    DASSERT(ret == 4+arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    return 0;
}

int test_char() 
{
    int ret;
    char buf[1024] = {0};
    char a1 = 10; 
    char a2 = 0;

    ret = pack(buf, 1024, "c", a1); 
    DASSERT(ret == sizeof(a1));
    
    ret = unpack(buf, ret, "c", &a2);
    DASSERT(ret == sizeof(a1));
    DASSERT(a2 == a1);
    
    ret = pack(buf, 1024, "$4c", a1);
    DASSERT(ret == sizeof(a1) + 4);
    a2 = 0;    
    ret = unpack(buf+4, ret, "c", &a2);
    DASSERT(ret == sizeof(a1));
    DASSERT(a2 == a1);
    
    // int array
    char array1[3] = {1, 2, 3};
    char array2[3] = {0};
    int alen = 0;
    int arraysize = sizeof(char) * 3;

    ret = pack(buf, 1024, "c3", array1);
    DASSERT(ret == 1 + arraysize);

    ret = unpack(buf, ret, "c3", &alen, array2);
    //DINFO("ret:%d, arraysize:%d\n", ret, arraysize);
    DASSERT(ret == 1 + arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    memset(array2, 0, arraysize);
    //alen = 3;

    ret = pack(buf, 1024, "c3:4", array1);
    DASSERT(ret = 4 + arraysize);
    alen = 0; 
    ret = unpack(buf, ret, "c3:4", &alen, array2);
    DASSERT(ret == 4 + arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    memset(array2, 0, arraysize);
    alen = 3;

    ret = pack(buf, 1024, "C", alen, array1);
    DASSERT(ret == 1+arraysize);
    alen = 0;
    ret = unpack(buf, ret, "C", &alen, array2);
    DASSERT(ret == 1+arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    memset(array2, 0, arraysize);
    alen = 3;

    ret = pack(buf, 1024, "C:4", alen, array1);
    DASSERT(ret = 4+arraysize);
    alen = 0; 
    ret = unpack(buf, ret, "C:4", &alen, array2);
    DASSERT(ret == 4+arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    return 0;
}

int test_short() 
{
    int ret;
    char buf[1024] = {0};
    int16_t a1 = 10; 
    int16_t a2 = 0;

    ret = pack(buf, 1024, "h", a1); 
    DASSERT(ret == sizeof(a1));
    
    ret = unpack(buf, ret, "h", &a2);
    DASSERT(ret == sizeof(a1));
    DASSERT(a2 == a1);
    
    ret = pack(buf, 1024, "$4h", a1);
    DASSERT(ret == sizeof(a1) + 4);
    a2 = 0;    
    ret = unpack(buf+4, ret, "h", &a2);
    DASSERT(ret == sizeof(a1));
    DASSERT(a2 == a1);
    
    // int array
    int16_t array1[3] = {1, 2, 3};
    int16_t array2[3] = {0};
    int alen = 0;
    int arraysize = sizeof(int16_t) * 3;

    ret = pack(buf, 1024, "h3", array1);
    DASSERT(ret == 1 + arraysize);

    ret = unpack(buf, ret, "h3", &alen, array2);
    //DINFO("ret:%d, arraysize:%d\n", ret, arraysize);
    DASSERT(ret == 1 + arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    memset(array2, 0, arraysize);
    //alen = 3;

    ret = pack(buf, 1024, "h3:4", array1);
    DASSERT(ret = 4 + arraysize);
    alen = 0; 
    ret = unpack(buf, ret, "h3:4", &alen, array2);
    DASSERT(ret == 4 + arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    memset(array2, 0, arraysize);
    alen = 3;

    ret = pack(buf, 1024, "H", alen, array1);
    DASSERT(ret == 1+arraysize);
    alen = 0;
    ret = unpack(buf, ret, "H", &alen, array2);
    DASSERT(ret == 1+arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    memset(array2, 0, arraysize);
    alen = 3;

    ret = pack(buf, 1024, "H:4", alen, array1);
    DASSERT(ret = 4+arraysize);
    alen = 0; 
    ret = unpack(buf, ret, "H:4", &alen, array2);
    DASSERT(ret == 4+arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    return 0;
}

int test_double() 
{
    int ret;
    char buf[1024] = {0};
    double a1 = 10; 
    double a2 = 0;

    ret = pack(buf, 1024, "d", a1); 
    DASSERT(ret == sizeof(a1));
    
    ret = unpack(buf, ret, "d", &a2);
    DASSERT(ret == sizeof(a1));
    DASSERT(a2 == a1);
    
    ret = pack(buf, 1024, "$4d", a1);
    DASSERT(ret == sizeof(a1) + 4);
    a2 = 0;    
    ret = unpack(buf+4, ret, "d", &a2);
    DASSERT(ret == sizeof(a1));
    DASSERT(a2 == a1);
    
    // int array
    double array1[3] = {1, 2, 3};
    double array2[3] = {0};
    int alen = 0;
    int arraysize = sizeof(double) * 3;

    ret = pack(buf, 1024, "d3", array1);
    DASSERT(ret == 1 + arraysize);

    ret = unpack(buf, ret, "d3", &alen, array2);
    //DINFO("ret:%d, arraysize:%d\n", ret, arraysize);
    DASSERT(ret == 1 + arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    memset(array2, 0, arraysize);
    //alen = 3;

    ret = pack(buf, 1024, "d3:4", array1);
    DASSERT(ret = 4 + arraysize);
    alen = 0; 
    ret = unpack(buf, ret, "d3:4", &alen, array2);
    DASSERT(ret == 4 + arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    memset(array2, 0, arraysize);
    alen = 3;

    ret = pack(buf, 1024, "D", alen, array1);
    DASSERT(ret == 1+arraysize);
    alen = 0;
    ret = unpack(buf, ret, "D", &alen, array2);
    DASSERT(ret == 1+arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    memset(array2, 0, arraysize);
    alen = 3;

    ret = pack(buf, 1024, "D:4", alen, array1);
    DASSERT(ret = 4+arraysize);
    alen = 0; 
    ret = unpack(buf, ret, "D:4", &alen, array2);
    DASSERT(ret == 4+arraysize);
    DASSERT(alen == 3);
    DASSERT(array2[0] == array1[0]);
    DASSERT(array2[1] == array1[1]);
    DASSERT(array2[2] == array1[2]);

    return 0;
}

int test_string()
{
    int ret;
    char buf[1024] = {0};
    
    char *a1 = "gogo test";
    char a2[32] = {0};

    ret = pack(buf, 1024, "s", a1);
    DASSERT(ret == strlen(a1) + 1);
    ret = unpack(buf, ret, "s", a2);
    DASSERT(ret == strlen(a1) + 1);
    DASSERT(strcmp(a1, a2) == 0);
   
    memset(a2, 0, 32);

    ret = pack(buf, 1024, "$4s", a1);
    DASSERT(ret == strlen(a1) + 1 + 4);
    ret = unpack(buf+4, ret, "s", a2);
    DASSERT(ret == strlen(a1) + 1);
    DASSERT(strcmp(a1, a2) == 0);
 
    return 0;
}






int main()
{
    logfile_create("stdout", 4);

    test_int();
    test_long();
    test_float();
    test_char();
    test_short();
    test_double();
    test_string();

    return 0;
}

int test() 
{
    char buf[1024] = {0};
    int a = 10;
    int b[3] = {1, 2, 3};
    int64_t c = 3;
    int64_t d[3] = {2, 3, 2};
    char e = 'a';
    char f[4] = "abcd";
    char *g = "haha";
    int16_t h = 32;
    int16_t i[3] = {12, 34, 56};

    int ret;

    logfile_create("stdout", 4);

    ret = pack(buf, 1024, "$4ii3ll3cc4shh3", a, b, c, d, e, f, g, h, i);
    DINFO("pack ret:%d\n", ret);
    
    printh(buf, ret);
    
    DINFO("================\n");
    ret = pack(buf, 1024, "$4iIlLcCshH", a, 3, b, c, 3, d, e, 4, f, g, h, 3, i);
    DINFO("pack ret:%d\n", ret);
    
    printh(buf, ret);

    DINFO("================\n");

    int a1 = 0;
    uint8_t b1len = 0;
    int b1[3] = {0};
    int64_t c1 = 0;
    uint8_t d1len = 0;
    int64_t d1[3] = {0};
    char e1 = 0;
    uint8_t f1len = 0;
    char f1[4] = {0};
    char g1[10] = {0};
    int16_t h1 = 0;
    uint8_t i1len = 0;
    int16_t i1[3] = {0};
    int allen;

    ret = unpack(buf, ret, "iii3ll3cc4shh3", &allen, &a1, &b1len, b1, &c1, &d1len, d1, &e1, &f1len, f1, g1, &h1, &i1len, i1); 

    DINFO("unpack ret:%d\n", ret);
    DINFO("allen:%d\n", allen); 
    DINFO("a1:%d, b1len:%d, b1[0]:%d, b1[1]:%d, b1[2]:%d\n", a1, b1len, b1[0], b1[1], b1[2]);
    DINFO("c1:%lld, d1len:%d, d1[0]:%lld, d1[1]:%lld, d1[2]:%lld\n", (long long)c1, d1len, (long long)d1[0], (long long)d1[1], (long long)d1[2]);
    DINFO("e1:%c, f1len:%d, f1[0]:%c, f1[1]:%c, f1[2]:%c, f1[3]:%c\n", e1, f1len, f1[0], f1[1], f1[2], f1[3]);
    DINFO("g1:%s\n", g1);
    DINFO("h1:%d, i1len:%d, i1[0]:%d, i1[1]:%d, i1[2]:%d\n", h1, i1len, i1[0], i1[1], i1[2]);

    DINFO("================\n");

    ret = unpack(buf, ret, "iiIlLcCshH", &allen, &a1, &b1len, b1, &c1, &d1len, d1, &e1, &f1len, f1, g1, &h1, &i1len, i1); 

    DINFO("unpack ret:%d\n", ret);
    DINFO("allen:%d\n", allen); 
    DINFO("a1:%d, b1len:%d, b1[0]:%d, b1[1]:%d, b1[2]:%d\n", a1, b1len, b1[0], b1[1], b1[2]);
    DINFO("c1:%lld, d1len:%d, d1[0]:%lld, d1[1]:%lld, d1[2]:%lld\n", (long long)c1, d1len, (long long)d1[0], (long long)d1[1], (long long)d1[2]);
    DINFO("e1:%c, f1len:%d, f1[0]:%c, f1[1]:%c, f1[2]:%c, f1[3]:%c\n", e1, f1len, f1[0], f1[1], f1[2], f1[3]);
    DINFO("g1:%s\n", g1);
    DINFO("h1:%d, i1len:%d, i1[0]:%d, i1[1]:%d, i1[2]:%d\n", h1, i1len, i1[0], i1[1], i1[2]);

    DINFO("================\n");

    int t[3] = {1, 2, 3};
    char buf2[128] = {0};

    //ret = pack(buf2, 128, "i3:4", t);
    ret = pack(buf2, 128, "I:4", 3, t);
    DINFO("pack ret:%d\n", ret);

    printh(buf2, ret);

    int t2[3] = {0};
    int tlen = 0;

    //ret = unpack(buf2, ret, "i3:4", &tlen, t2);
    ret = unpack(buf2, ret, "I:4", &tlen, t2);
    DINFO("unpack ret:%d\n", ret);
    
    DINFO("tlen:%d t2[0]:%d, t2[1]:%d, t2[2]:%d\n", tlen, t2[0], t2[1], t2[2]);
    
    DINFO("================\n");
    
    memset(buf2, 0, 128);
    float f3 = 10.3;
    float f4;

    ret = pack(buf2, 128, "f", f3);
    ret = unpack(buf2, ret, "i", &f4);
    DINFO("f4:%f\n", f4);

    return 0;
}
