#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "memlink_client.h"
#include "logfile.h"
#include "utils.h"

int main(int argc, char *argv[])
{
    MemLink *m;
    logfile_create("stdout", 3);

    m = memlink_create("127.0.0.1", 11001, 11002, 0);
    if (NULL == m) {
        printf("memlink_create error!\n");
    }

	struct timeval start, end;
	int i, ret;
	int count = 100000;

	gettimeofday(&start, NULL);
	for (i = 0; i < count; i++) {
		ret = memlink_cmd_ping(m);	
		if (ret != MEMLINK_OK) {
			printf("memlink ping error! %d\n", ret);
			return -1;
		}
	}
	gettimeofday(&end, NULL);

	double speed;
	speed = ((double)count / timediff(&start, &end)) * 1000000;
	printf("time: %d, speed: %.2f\n", timediff(&start, &end), speed);

    memlink_destroy(m);

    return 0;
}

