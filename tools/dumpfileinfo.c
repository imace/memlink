#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>

int
dumpfile_info(char *filename)
{
    FILE    *fp;

    fp = fopen(filename, "r");
    if (NULL == fp) {
        printf("open file \"%s\" error: %s\n", filename, strerror(errno));
        return -1;
    }

    int             ret;
    unsigned short  formatver;
    unsigned int    dumpver;
    unsigned int    logver;

    if (fread(&formatver, sizeof(short), 1, fp) < sizeof(short)) {
        printf("read format error: %s\n", strerror(errno));
        return -1;
    }
    if (fread(&dumpver, sizeof(int), 1, fp) < sizeof(int)) {
        printf("read dump version error: %s\n", strerror(errno));
        return -1;
    }
    if (fread(&logver, sizeof(int), 1, fp) < sizeof(int)) {
        printf("read log version error: %s\n", strerror(errno));
        return -1;
    }

    printf("format:\t%d\ndump version:\t%d\nlog version:\t%d\n", formatver, dumpver, logver);

    fclose(fp);

    return 0;
}

int 
show_help()
{
    printf("usage:\n\tdumpfileinfo [options]\noptions:\n\t--help, -h\tshow help infomation.\n\t--file, -f\tdump.dat file path.\n\n");
    return 0;
}

int 
main(int argc, char *argv[])
{
    int     optret;
    int     optidx = 0;
    char    *optstr = "f:h";
    struct option myoptions[] = {{"file", 1, NULL, 'f'}, 
                                 {"help", 0, NULL, 'h'},
                                 {NULL, 0, NULL, 0}};

    char    dumpfile[PATH_MAX] = {0};

    if (argc < 2) {
        show_help();
        return 0;
    }

    while(optret = getopt_long(argc, argv, optstr, myoptions, &optidx)) {
        if (0 > optret) {
            break;
        }

        switch (optret) {
        case 'f':
            snprintf(dumpfile, PATH_MAX, "%s", optarg);
            break;
        case 'h':
            show_help();
            return 0;
            break;
        default:
            break;
        }
    }

    /*
    if (dumpfile[0] == 0) {
        strcpy(dumpfile, "data/dump.dat");
    }*/
    printf("dumpfile: %s\n", dumpfile);

    dumpfile_info(dumpfile);

    return 0;
}


