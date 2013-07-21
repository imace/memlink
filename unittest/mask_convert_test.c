#include <stdio.h>
#include <stdlib.h>
#include "logfile.h"
#include "serial.h"
#include <math.h>
#include <utils.h>
#include "hashtest.h"

int attr_string2array_test()//随机生成一个attr，0-20个项，每项的值0-256
{
	int num = 1 + my_rand(HASHTABLE_ATTR_MAX_ITEM);
	char attr[512] = {0};
	char buf[10];
	int i = 1;
	
    while(i <= num) {
		int val = my_rand(512);
		sprintf(buf, "%d", val);
		do {
			if(val > 256 && num != 1)
				break;			
			strcat(attr, buf);
		}while(0);

		if(i != num)
			strcat(attr, ":");
		i++;
	}
	unsigned int result[128];
	int ret = attr_string2array(attr, result);
	if(ret != num) {		
		DERROR("attr_string2array error. attr:%s, num:%d, ret:%d\n", attr, num, ret);
		return -1;
	}

	return 0;
}

int attr_string2binary_binary2string()
{
	int i = 0;
    int j = 0;

	for (j = 0; j < 50; j++) {
		int num = 1 + my_rand(5);
		char attrformat[512] = {0};
		unsigned char attrformatnum[100] = {0};
		char attrstr[512] = {0};

		//DINFO("num:%d\n", num);		
		int len = 2;

		for(i = 1; i <= num; i++) {
			//随机生成attrformat
			char buf[10] = {0};
			char buf2[10] = {0};
			int val = 1 + my_rand(8);

			sprintf(buf, "%d", val);
			strcat(attrformat, buf);
			if(i != num) {
				strcat(attrformat, ":");
            }
			len += val;
			attrformatnum[i-1] = (char)val;
			//随机生成attrstr
			int max = pow(2, val) - 1;
			int k= 1 + my_rand(max);
			sprintf(buf2, "%d", k);
			do {
				if(k > 1024)
					break;			
				strcat(attrstr, buf2);
			}while(0);

			if(i != num)
				strcat(attrstr, ":");
		}		
		//DINFO("attrformat=%s, attrstr=%s\n", attrformat, attrstr);
		int n = len / 8;
		int attrlen = ((len % 8) == 0)? n : (n+1);
		char attr[512] = {0};
		int ret;
		ret = attr_string2binary(attrformatnum, attrstr, attr);		

		char buf2[128];
        //DINFO("attr:%s\n", formath(attr, attrlen, buf2, 128));
		char attrstr1[512] = {0};
		attr_binary2string(attrformatnum, num, attr, attrlen, attrstr1);		
		//DINFO("attrstr1=%s\n\n", attrstr1);		
		//DINFO("attrformat=%s, attrstr=%s, attrstr1=%s\n", attrformat, attrstr, attrstr1);

		if(0 != strcmp(attrstr, attrstr1)) {	
			DERROR("================ERROR!=============== ");
			DINFO("attr:%s\n", formath(attr, attrlen, buf2, 128));
			return -1;
		}
	}
	return 0;
}


int array2bin_test_one(unsigned char *format, unsigned int *array, char num, unsigned char *attr, unsigned char msize)
{
	int ret;
    char data[1024] = {0};

    ret = attr_array2binary(format, array, num, data);
	if (ret != msize) {
		DERROR("attr_array2binary error. ret:%d, attr size:%d\n", ret, msize);
		return -1;
	}else{
		ret = memcmp(data, attr, msize);
		if (0 != ret) {
            char buf[512] = {0};
			DERROR("attr memcmp error. %s\n", formath(data, msize, buf, 512));
			return -1;
		}else{
			//DINFO("test1 ok!\n");
        }
	}

    return 0;
}

int array2flag_test_one(unsigned char *format, unsigned int *array, char num, unsigned char *attr, unsigned char msize)
{
	int ret;
    char data[1024] = {0};

    ret = attr_array2flag(format, array, num, data);
	if (ret != msize) {
		DERROR("attr_array2flag error. ret:%d, attr size:%d\n", ret, msize);
		return -1;
	}else{
		ret = memcmp(data, attr, msize);
		if (0 != ret) {
            char buf[512] = {0};
			DERROR("attr memcmp error. %s\n", formath(data, msize, buf, 512));
			return -1;
		}else{
			//DINFO("test1 ok!\n");
        }
	}

    return 0;
}

typedef struct testitem
{
    unsigned char format[16];
    unsigned int  array[16];
    int           num;
    unsigned char attr[512];
    int           msize;
}TestItem;

int attr_array2binary_test()
{
    TestItem    testitems[100];
    
    TestItem    *item = &testitems[0];
    item->num       = 3;
    item->format[0] = 4;
    item->format[1] = 3;
    item->format[2] = 1;
    item->array[0]  = UINT_MAX;
    item->array[1]  = 2;
    item->array[2]  = 0;
    item->attr[0]   = 0x81;
    item->attr[1]   = 0;
    item->msize     = 2;

    item = &testitems[1];
    item->num       = 3;
    item->format[0] = 4;
    item->format[1] = 3;
    item->format[2] = 1;
    item->array[0]  = 5;
    item->array[1]  = 2;
    item->array[2]  = 2;
    item->attr[0]   = 0x95;
    item->attr[1]   = 0;
    item->msize     = 2;

    item = &testitems[2];
    item->num       = 3;
    item->format[0] = 4;
    item->format[1] = 3;
    item->format[2] = 8;
    item->array[0]  = 5;
    item->array[1]  = 2;
    item->array[2]  = 128;
    item->attr[0]   = 0x95;
    item->attr[1]   = 0;
    item->attr[2]   = 0x01;
    item->msize     = 3;

    item = &testitems[3];
    item->num       = 2;
    item->format[0] = 32;
    item->format[1] = 1;
    item->array[0]  = 0x23;
    item->array[1]  = 0x01;
    item->attr[0]   = 0x8d;
    item->attr[1]   = 0;
    item->attr[2]   = 0;
    item->attr[3]   = 0;
    item->attr[4]   = 0x04;
    item->msize     = 5;

    item = &testitems[4];
    item->num       = 2;
    item->format[0] = 2;
    item->format[1] = 16;
    item->array[0]  = 1;
    item->array[1]  = 0x1111;
    item->attr[0]   = 0x15;
    item->attr[1]   = 0x11;
    item->attr[2]   = 0x01;
    item->msize     = 3;

    item = &testitems[5];
    item->num       = 2;
    item->format[0] = 32;
    item->format[1] = 32;
    item->array[0]  = 0x0fffffff;
    item->array[1]  = 0x10101010;
    item->attr[0]   = 0xfd;
    item->attr[1]   = 0xff;
    item->attr[2]   = 0xff;
    item->attr[3]   = 0x3f;
    item->attr[4]   = 0x40;
    item->attr[5]   = 0x40;
    item->attr[6]   = 0x40;
    item->attr[7]   = 0x40;
    item->attr[8]   = 0x0;
    item->msize     = 9;

    item = &testitems[6];
    item->num       = 2;
    item->format[0] = 32;
    item->format[1] = 32;
    item->array[0]  = 0xffffffff;
    item->array[1]  = 0x10101010;
    item->attr[0]   = 0x01;
    item->attr[1]   = 0x0;
    item->attr[2]   = 0x0;
    item->attr[3]   = 0x0;
    item->attr[4]   = 0x40;
    item->attr[5]   = 0x40;
    item->attr[6]   = 0x40;
    item->attr[7]   = 0x40;
    item->attr[8]   = 0x0;
    item->msize     = 9;

    item = &testitems[7];
    item->num       = 1;
    item->format[0] = 32;
    item->array[0]  = 0xffffffff;
    item->attr[0]   = 0x01;
    item->attr[1]   = 0x0;
    item->attr[2]   = 0x0;
    item->attr[3]   = 0x0;
    item->attr[4]   = 0x0;
    item->msize     = 5;


    item = &testitems[8];
    item->num       = 3;
    item->format[0] = 2;
    item->format[1] = 32;
    item->format[2] = 32;
    item->array[0]  = 1;
    item->array[1]  = 0x0fffffff;
    item->array[2]  = 0x10101010;
    item->attr[0]   = 0xf5;
    item->attr[1]   = 0xff;
    item->attr[2]   = 0xff;
    item->attr[3]   = 0xff;
    item->attr[4]   = 0x0;
    item->attr[5]   = 0x01;
    item->attr[6]   = 0x01;
    item->attr[7]   = 0x01;
    item->attr[8]   = 0x01;
    item->msize     = 9;

    int i;
    for (i = 0; i < 9; i++) {
        item = &testitems[i];
        int ret = array2bin_test_one(item->format, item->array, item->num, 
                item->attr, item->msize);
        if (ret != 0) {
            DERROR("test error: %d, %d\n", i, ret);
        }
    }

	return 0;
}

int attr_array2flag_test()
{
    TestItem    testitems[100];
    
    TestItem    *item = &testitems[0];
    item->num       = 3;
    item->format[0] = 4;
    item->format[1] = 3;
    item->format[2] = 1;
    item->array[0]  = UINT_MAX;
    item->array[1]  = 2;
    item->array[2]  = 0;
    item->attr[0]   = 0x3f;
    item->attr[1]   = 0;
    item->msize     = 2;

    item = &testitems[1];
    item->num       = 3;
    item->format[0] = 4;
    item->format[1] = 3;
    item->format[2] = 1;
    item->array[0]  = 5;
    item->array[1]  = 2;
    item->array[2]  = 2;
    item->attr[0]   = 0x03;
    item->attr[1]   = 0;
    item->msize     = 2;

    item = &testitems[2];
    item->num       = 3;
    item->format[0] = 4;
    item->format[1] = 3;
    item->format[2] = 8;
    item->array[0]  = 5;
    item->array[1]  = 2;
    item->array[2]  = 128;
    item->attr[0]   = 0x03;
    item->attr[1]   = 0;
    item->attr[2]   = 0;
    item->msize     = 3;

    item = &testitems[3];
    item->num       = 2;
    item->format[0] = 32;
    item->format[1] = 1;
    item->array[0]  = 0x23;
    item->array[1]  = 0x01;
    item->attr[0]   = 0x03;
    item->attr[1]   = 0;
    item->attr[2]   = 0;
    item->attr[3]   = 0;
    item->attr[4]   = 0;
    item->msize     = 5;

    item = &testitems[4];
    item->num       = 2;
    item->format[0] = 2;
    item->format[1] = 16;
    item->array[0]  = 1;
    item->array[1]  = 0x1111;
    item->attr[0]   = 0x03;
    item->attr[1]   = 0;
    item->attr[2]   = 0;
    item->msize     = 3;

    item = &testitems[5];
    item->num       = 2;
    item->format[0] = 32;
    item->format[1] = 32;
    item->array[0]  = 0x0fffffff;
    item->array[1]  = 0x10101010;
    item->attr[0]   = 0x03;
    item->attr[1]   = 0;
    item->attr[2]   = 0;
    item->attr[3]   = 0;
    item->attr[4]   = 0;
    item->attr[5]   = 0;
    item->attr[6]   = 0;
    item->attr[7]   = 0;
    item->attr[8]   = 0;
    item->msize     = 9;

    item = &testitems[6];
    item->num       = 2;
    item->format[0] = 32;
    item->format[1] = 32;
    item->array[0]  = 0xffffffff;
    item->array[1]  = 0x10101010;
    item->attr[0]   = 0xff;
    item->attr[1]   = 0xff;
    item->attr[2]   = 0xff;
    item->attr[3]   = 0xff;
    item->attr[4]   = 0x03;
    item->attr[5]   = 0;
    item->attr[6]   = 0;
    item->attr[7]   = 0;
    item->attr[8]   = 0;
    item->msize     = 9;

    item = &testitems[7];
    item->num       = 1;
    item->format[0] = 32;
    item->array[0]  = 0xffffffff;
    item->attr[0]   = 0xff;
    item->attr[1]   = 0xff;
    item->attr[2]   = 0xff;
    item->attr[3]   = 0xff;
    item->attr[4]   = 0x03;
    item->msize     = 5;


    item = &testitems[8];
    item->num       = 3;
    item->format[0] = 2;
    item->format[1] = 32;
    item->format[2] = 32;
    item->array[0]  = 1;
    item->array[1]  = 0x0fffffff;
    item->array[2]  = 0x10101010;
    item->attr[0]   = 0x03;
    item->attr[1]   = 0;
    item->attr[2]   = 0;
    item->attr[3]   = 0;
    item->attr[4]   = 0;
    item->attr[5]   = 0;
    item->attr[6]   = 0;
    item->attr[7]   = 0;
    item->attr[8]   = 0;
    item->msize     = 9;

    item = &testitems[9];
    item->num       = 3;
    item->format[0] = 4;
    item->format[1] = 2;
    item->format[2] = 1;
    item->array[0]  = 7;
    item->array[1]  = UINT_MAX;
    item->array[2]  = UINT_MAX;
    item->attr[0]   = 0x3f;
    item->attr[1]   = 0;
    item->msize     = 2;

    int i;
    for (i = 0; i < 10; i++) {
        item = &testitems[i];
        int ret = array2flag_test_one(item->format, item->array, item->num, 
                item->attr, item->msize);
        if (ret != 0) {
            DERROR("test error: %d, %d\n", i, ret);
        }
    }

	return 0;
}

int main()
{
	logfile_create("test.log", 3);
	//logfile_create("stdout", 4);

	int ret;
	int i = 0;

	for (i = 0; i < 10; i++) {
		ret = attr_string2array_test();
		if (0 != ret)
			return -1;
	}
	
	ret = attr_array2binary_test();
	if (0 != ret) {
		return -1;
    }

	ret = attr_array2flag_test();
	if (0 != ret) {
		return -1;
    }
	attr_string2binary_binary2string();
	if (0 != ret) {
		return -1;
    }

	return 0;
}
