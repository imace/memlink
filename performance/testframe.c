#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>
#include <getopt.h>
#include <memlink_client.h>
#include "testframe.h"
#include "logfile.h"
#include "utils.h"


#define STAT_MIDDLE		1


extern int clear_key(char *key);
extern int create_key(char *key);

TestConfig *g_tcf;

/*int compare_int(const void *a, const void *b) 
{
    return *(int*)a - *(int*)b;
}*/

int getmem(int pid)
{
    int     ret = 0;
    FILE    *fp;
    char    path[1024];
    char    line[1024];

    sprintf(path, "/proc/%d/status", pid);

    fp = fopen(path, "r");
    if (NULL == fp) {
        DERROR("open file %s error! %s\n", path, strerror(errno));
        return -1;
    }
    
    while (1) {
        if (fgets(line, 1024, fp) == NULL) {
            break;
        }
        
        if (strncmp(line, "VmRSS", 5) == 0) {
            char buf[64] = {0};
            char *tmp = line + 7;

            while (*tmp == ' ' || *tmp == '\t')
                tmp++;

            int i = 0;
            while (isdigit(*tmp)) {
                buf[i] = *tmp;
                i++;
                tmp++;
            }

            ret = atoi(buf);
            break;
        }
    }

    fclose(fp);

    return ret;
}

int alltest_insert()
{
    int i;
    int *insertret; //[g_tcf->insert_test_count] = {0};
    int n;
    int ret;
    int f;

	insertret = (int*)alloca(g_tcf->insert_test_count * sizeof(int));
	memset(insertret, 0, g_tcf->insert_test_count * sizeof(int));

    // test insert
    for (f = 0; f < 2; f++) {
        for (i = 0; i < g_tcf->testnum_count - 2; i++) {
            for (n = 0; n < g_tcf->insert_test_count; n++) {
                DINFO("====== insert %d test: %d ======\n", g_tcf->testnum[i], n);
                insert_func func = g_tcf->ifuncs[f];
                ret = func(g_tcf->testnum[i], 1);
                insertret[n] = ret;
                clear_key("haha");
            }
		
			int		sum = 0;
			float	av;	

#ifdef STAT_MIDDLE
			if (g_tcf->insert_test_count > 2) {
				qsort(insertret, g_tcf->insert_test_count, sizeof(int), compare_int);
				for (n = 0; n < g_tcf->insert_test_count; n++) {
					printf("n: %d, %d\t", n, insertret[n]);
				}
				printf("\n");

				for (n = 1; n < g_tcf->insert_test_count - 1; n++) {
					sum += insertret[n];
				}
				av = (float)sum / (g_tcf->insert_test_count - 2);
			}else{
#endif
				for (n = 0; n < g_tcf->insert_test_count; n++) {
					sum += insertret[n];
				}
				av = (float)sum / g_tcf->insert_test_count;

#ifdef STAT_MIDDLE
			}
#endif
            DINFO("\33[31m====== %d sum: %d, ave: %.2f ======\33[0m\n", g_tcf->testnum[i], sum, av);
        }
    }
	
	return 0;
}

int alltest_range()
{
    int i;
    int n;
    int ret;
    int f;
    int *rangeret; //[g_tcf->range_test_count] = {0};
    int j = 0, k = 0;

	rangeret = (int*)alloca(g_tcf->range_test_count * sizeof(int));
	memset(rangeret, 0, g_tcf->range_test_count * sizeof(int));

    /*       
	DINFO("range insert: %d\n", g_tcf->testnum[g_tcf->testnum_count - 1]);
	insert_func insertlong = g_tcf->ifuncs[0];
	ret = insertlong(g_tcf->testnum[g_tcf->testnum_count - 1], 1);
	*/

    // test range
    for (f = 0; f < 2; f++) {
        for (i = 0; i < g_tcf->testnum_count; i++) {
            for (j = 0; j < 2; j++) {
                for (k = 0; k < g_tcf->range_test_range_count; k++) {
                    int startpos, slen;

                    if (j == 0) {
                        startpos = 0;
                        slen = g_tcf->range_test_range[k];
                    }else{
                        startpos = g_tcf->testnum[i] - g_tcf->range_test_range[k];
                        slen = g_tcf->range_test_range[k];
                    }
                    for (n = 0; n < g_tcf->range_test_count; n++) {
                        DINFO("====== range %d, from: %d, len:%d, test: %d ======\n", g_tcf->testnum[i], startpos, slen, n);
                        //ret = test_range_long(startpos, slen, 1000);
                        range_func func = g_tcf->rfuncs[f];
                        ret = func(startpos, slen, g_tcf->range_sample_count);

                        //DINFO("====== range %d result: %d ======\n", testnum[i], ret);
                        rangeret[n] = ret;
                    }

					int		sum = 0;
					float	av;	

#ifdef STAT_MIDDLE
					if (g_tcf->range_test_count > 2) {
						qsort(rangeret, g_tcf->range_test_count, sizeof(int), compare_int);

						for (n = 0; n < g_tcf->range_test_count; n++) {
							printf("n: %d, %d\t", n, rangeret[n]);
						}
						printf("\n");

						for (n = 1; n < g_tcf->range_test_count - 1; n++) {
							sum += rangeret[n];
						}
						av = (float)sum / (g_tcf->range_test_count - 2);
					}else{
#endif
						for (n = 0; n < g_tcf->range_test_count; n++) {
							sum += rangeret[n];
						}
						av = (float)sum / g_tcf->range_test_count;
#ifdef STAT_MIDDLE
					}
#endif

                    DINFO("\33[31m====== range:%d  sum: %d, ave: %.2f ======\33[0m\n", g_tcf->testnum[i], sum, av);
                }
            }
            //clear_key("haha");
        }
    }

    return 0;
}

int thread_create_count = 0;
pthread_mutex_t	lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t	cond = PTHREAD_COND_INITIALIZER;

void* thread_start (void *args)
{
	ThreadArgs	*a = (ThreadArgs*)args;
	int			ret;

	pthread_mutex_lock(&lock);
	while (thread_create_count < TEST_THREAD_NUM) {
		pthread_cond_wait(&cond, &lock);
        DINFO("cond wait return ...\n");
	}

	pthread_mutex_unlock(&lock);
	
	DINFO("thread run %u\n", (unsigned int)pthread_self());

	if (a->type == 1) {  //insert
		//DINFO("insert args: %d\n", a->count);
		insert_func	func = a->func;	
		ret = func(a->count, 0);
	}else{ // range
		//DINFO("range args: %d, %d, %d\n", a->startpos, a->slen, a->count);
		range_func	func = a->func;	
		ret = func(a->startpos, a->slen, a->count);
	}

    free(args);

    return (void*)ret;
}

int alltest_thread_insert()
{
    //int testnum[TESTN] = {10000, 100000, 1000000, 10000000};
    int i;
    int *insertret; // [INSERT_TESTS] = {0};
    int n;
    int ret;
    int f;
    int t;
    //insert_func ifuncs[2] = {test_insert_long, test_insert_short};
    //range_func  rfuncs[2] = {test_range_long, test_range_short};

	insertret = (int*)alloca(g_tcf->insert_test_count * sizeof(int));
	memset(insertret, 0, g_tcf->insert_test_count * sizeof(int));

	/*
    int startmem  = getmem(getpid());
    int insertmem[TESTN] = {0};
	*/
    pthread_t   threads[TEST_THREAD_NUM];
    struct timeval  start, end;

    g_tcf->isthread = 1;

    // test insert
    for (f = 0; f < 2; f++) {
        for (i = 0; i < g_tcf->testnum_count; i++) {
            for (n = 0; n < g_tcf->insert_test_count; n++) {
                DINFO("====== insert %d test: %d ======\n", g_tcf->testnum[i], n);
                create_key("haha");
				
                int thnum = g_tcf->testnum[i] / g_tcf->threads;
                for (t = 0; t < g_tcf->threads; t++) {
					ThreadArgs *ta = (ThreadArgs*)malloc(sizeof(ThreadArgs));
					memset(ta, 0, sizeof(ThreadArgs));
					ta->type = 1;
					ta->func = g_tcf->ifuncs[f];
					ta->count = thnum;

                    ret = pthread_create(&threads[t], NULL, thread_start, ta);
                    if (ret != 0) {
                        DERROR("pthread_create error! %s\n", strerror(errno));
                        return -1;
                    }
					thread_create_count += 1;
                }

                gettimeofday(&start, NULL);
                ret = pthread_cond_broadcast(&cond);
                if (ret != 0) {
                    DERROR("pthread_cond_signal error: %s\n", strerror(errno));
                }

                for (t = 0; t < TEST_THREAD_NUM; t++) {
                    pthread_join(threads[t], NULL);
                }
                gettimeofday(&end, NULL);
                
                unsigned int tmd = timediff(&start, &end);
                double speed = ((double)g_tcf->testnum[i] / tmd) * 1000000;
                if (f == 0) {
                    DINFO("thread insert long use time: %u, speed: %.2f\n", tmd, speed);
                }else{
                    DINFO("thread insert short use time: %u, speed: %.2f\n", tmd, speed);
                }
                insertret[n] = speed;
                clear_key("haha");
            }

			int		sum = 0;
			float	av;	

#ifdef STAT_MIDDLE
			if (g_tcf->insert_test_count > 2) {
				qsort(insertret, g_tcf->insert_test_count, sizeof(int), compare_int);
				for (n = 0; n < g_tcf->insert_test_count; n++) {
					printf("n: %d, %d\t", n, insertret[n]);
				}
				printf("\n");

				sum = 0;
				for (n = 1; n < g_tcf->insert_test_count - 1; n++) {
					sum += insertret[n];
				}

				av = (float)sum / (g_tcf->insert_test_count - 2);
			}else{
#endif
				for (n = 0; n < g_tcf->insert_test_count; n++) {
					sum += insertret[n];
				}
				av = (float)sum / g_tcf->insert_test_count;
#ifdef STAT_MIDDLE
			}
#endif
            DINFO("\33[31m====== insert %d sum: %d, ave: %.2f ======\33[0m\n", g_tcf->testnum[i], sum, av);
        }
    }

	return 0;

}

int alltest_thread_range()
{
    int i;
    int n;
    int ret;
    int f;
    int t;
    int *rangeret; //[RANGE_TESTS] = {0};
    int j = 0, k = 0;
    int range_test_num  = 10 * g_tcf->range_sample_count;

	rangeret = (int*)alloca(g_tcf->range_test_count * sizeof(int));
	memset(rangeret, 0, g_tcf->range_test_count * sizeof(int));

    pthread_t   threads[TEST_THREAD_NUM];
    struct timeval  start, end;

    g_tcf->isthread = 1;

	g_tcf->range_test_count = 1;

	/*
	DINFO("range insert: %d\n", g_tcf->testnum[g_tcf->testnum_count - 1]);
	insert_func insertlong = g_tcf->ifuncs[0];
	ret = insertlong(g_tcf->testnum[g_tcf->testnum_count - 1], 1);
	*/
    // test range
    for (f = 0; f < 2; f++) {
        for (i = 0; i < g_tcf->testnum_count; i++) {
            //DINFO("====== insert %d ======\n", g_tcf->testnum[i]);
			//insert_func insertlong = g_tcf->ifuncs[0];
            //ret = insertlong(g_tcf->testnum[i], 1);
            for (j = 0; j < 2; j++) {
                for (k = 0; k < g_tcf->range_test_range_count; k++) {
                    int startpos, slen;

                    if (j == 0) {
                        startpos = 0;
                        slen = g_tcf->range_test_range[k];
                    }else{
                        startpos = g_tcf->testnum[i] - g_tcf->range_test_range[k];
                        slen = g_tcf->range_test_range[k];
                    }
                    
                    int thnum = range_test_num / g_tcf->threads;
					//DINFO("thnum: %d\n", thnum);
                    for (n = 0; n < g_tcf->range_test_count; n++) {
                        for (t = 0; t < g_tcf->threads; t++) {
                            ThreadArgs *ta = (ThreadArgs*)malloc(sizeof(ThreadArgs));
                            memset(ta, 0, sizeof(ThreadArgs));
                            ta->type = 2;
							ta->startpos = startpos;
							ta->slen = slen;
                            ta->func = g_tcf->rfuncs[f];
                            ta->count = thnum;

                            ret = pthread_create(&threads[t], NULL, thread_start, ta);
                            if (ret != 0) {
                                DERROR("pthread_create error! %s\n", strerror(errno));
                                return -1;
                            }
					        thread_create_count += 1;
                        }
                            
                        gettimeofday(&start, NULL);
                        ret = pthread_cond_broadcast(&cond);
                        if (ret != 0) {
                            DERROR("pthread_cond_signal error: %s\n", strerror(errno));
                        }

                        for (t = 0; t < g_tcf->threads; t++) {
                            pthread_join(threads[t], NULL);
                        }
                        gettimeofday(&end, NULL);
                       
                        unsigned int tmd = timediff(&start, &end);
                        double speed = ((double)range_test_num / tmd) * 1000000;
                        if (f == 0) {
                            DINFO("\33[1mthread range long %d from:%d len:%d time:%u speed:%.2f\33[0m\n", g_tcf->testnum[i], startpos, slen, tmd, speed);
                        }else{
                            DINFO("\33[1mthread range short %d from:%d len:%d time:%u speed:%.2f\33[0m\n", g_tcf->testnum[i], startpos, slen, tmd, speed);
                        }
                        rangeret[n] = speed;
                    }
                    
					int		sum = 0;
					float	av;	

#ifdef STAT_MIDDLE
					if (g_tcf->range_test_count > 2) {
						qsort(rangeret, g_tcf->range_test_count, sizeof(int), compare_int);
						for (n = 0; n < g_tcf->range_test_count; n++) {
							printf("n: %d, %d\t", n, rangeret[n]);
						}
						printf("\n");

						sum = 0;
						for (n = 1; n < g_tcf->range_test_count - 1; n++) {
							sum += rangeret[n];
						}

						av = (float)sum / (g_tcf->range_test_count - 2);
					}else{
#endif
						for (n = 0; n < g_tcf->range_test_count; n++) {
							sum += rangeret[n];
						}
						av = (float)sum / g_tcf->range_test_count;

#ifdef STAT_MIDDLE
					}
#endif

                    DINFO("\33[31m====== range %d,%d sum: %d, speed: %.2f ======\33[0m\n", startpos, slen, sum, av);
                }
            }
            //clear_key("haha");
        }
    }

    return 0;
}

int show_help()
{
	printf("usage:\n\ttest [options]\n");
    printf("options:\n");
    printf("\t--help,-h\tshow help information\n");
    printf("\t--thread,-t\tthread count for test\n");
    printf("\t--count,-n\tinsert number\n");
    printf("\t--frompos,-f\trange from position\n");
    printf("\t--len,-l\trange length\n");
    printf("\t--alltest,-a\tuse alltest. will test insert/range in 1w,10w,100w,1000w\n");
    printf("\t--do,-d\t\tinsert or range\n");
    printf("\t--longconn,-c\tuse long connection for test. default 1\n");
    printf("\n\n");

	exit(0);
}


int main(int argc, char *argv[])
{
#ifdef DEBUG
	logfile_create("stdout", 4);
#endif

	int     optret;
    int     optidx = 0;
    char    *optstr = "ht:n:f:l:a:d:c:";
    struct option myoptions[] = {{"help", 0, NULL, 'h'},
                                 {"thread", 0, NULL, 't'},
                                 {"count", 0, NULL, 'n'},
                                 {"frompos", 0, NULL, 'f'},
                                 {"len", 0, NULL, 'l'},
                                 {"alltest", 0, NULL, 'a'},
                                 {"do", 0, NULL, 'd'},
                                 {"longconn", 0, NULL, 'c'},
                                 {NULL, 0, NULL, 0}};


    if (argc < 2) {
        show_help();
        return 0;
    }
	
	TestConfig tcf;

	memset(&tcf, 0, sizeof(TestConfig));

	/*
	tcf.ifuncs[0] = test_insert_long;
	tcf.ifuncs[1] = test_insert_short;
	tcf.rfuncs[0] = test_range_long;
	tcf.rfuncs[1] = test_range_short;
	*/

	testconfig_init(&tcf);
	
	tcf.insert_test_count = 1;
	tcf.range_test_count  = 2;

	tcf.testnum[0] = 10000;
	tcf.testnum[1] = 100000;
	tcf.testnum[2] = 1000000;
	tcf.testnum[3] = 10000000;
	tcf.testnum_count = 4;

	tcf.range_test_range[0] = 100;
	tcf.range_test_range[1] = 200;
	tcf.range_test_range[2] = 1000;
	tcf.range_test_range_count = 3;

	tcf.longcon = 1;
	tcf.range_sample_count = 1;

	g_tcf = &tcf;

	char dostr[32] = {0};

    while(optret = getopt_long(argc, argv, optstr, myoptions, &optidx)) {
        if (0 > optret) {
            break;
        }

        switch (optret) {
        case 'f':
			g_tcf->frompos = atoi(optarg);	
            break;
		case 't':
			g_tcf->threads = atoi(optarg);
			break;
		case 'n':
			g_tcf->count   = atoi(optarg);
			break;
		case 'c':
			g_tcf->longcon = atoi(optarg);
			break;
		case 'l':
			g_tcf->len     = atoi(optarg);
			break;		
		case 'a':
			g_tcf->isalltest = atoi(optarg);
			if (g_tcf->isalltest != 0) {
				g_tcf->isalltest = 1;
			}
			break;	
		case 'd':
			snprintf(dostr, 32, "%s", optarg);
			break;
        case 'h':
            show_help();
            //return 0;
            break;
        default:
			printf("error option: %c\n", optret);
			exit(-1);
            break;
        }
    }

    if (g_tcf->isalltest == 1) {
		if (g_tcf->threads > 0) {
			if (strcmp(dostr, "insert") == 0) {
				alltest_thread_insert(g_tcf->threads);
			}else if (strcmp(dostr, "range") == 0) {
				alltest_thread_range(g_tcf->threads);
			}else if (strcmp(dostr, "all") == 0) {
				alltest_thread_insert(g_tcf->threads);
				alltest_thread_range(g_tcf->threads);
			}else{
				DERROR("-d error: %s\n", dostr);
			}
		}else{
			if (strcmp(dostr, "insert") == 0) {
				alltest_insert(g_tcf->threads);
			}else if (strcmp(dostr, "range") == 0) {
				alltest_range(g_tcf->threads);
			}else if (strcmp(dostr, "all") == 0) {
				alltest_insert(g_tcf->threads);
				alltest_range(g_tcf->threads);
			}else{
				DERROR("-d error: %s\n", dostr);
			}

		}
        return 0;
    }

	if (strcmp(dostr, "insert") == 0) {
		if (g_tcf->longcon == 1) {
			test_insert_long(g_tcf->count, 1);
		}else{
			test_insert_short(g_tcf->count, 1);
		}
	}else if (strcmp(dostr, "range") == 0){
		if (g_tcf->longcon == 1) {
			test_range_long(g_tcf->frompos, g_tcf->len, 1000);
		}else{
			test_range_short(g_tcf->frompos, g_tcf->len, 1000);
		}
	}else{
		printf("-d %s error!\n", dostr);
	}

	return 0;
}





