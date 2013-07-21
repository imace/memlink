#include "confparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "zzmalloc.h"
#include "defines.h"
#include "logfile.h"

ConfPairs*    
confpairs_create(int count)
{
    int size = sizeof(ConfPairs) + sizeof(ConfPair)*count;
    ConfPairs *pairs = zz_malloc(size);
    memset(pairs, 0, size);
    pairs->count = count;
    pairs->len = 0;

    return pairs;
}

int
confpairs_add(ConfPairs *pairs, char *key, int val)
{
    if (pairs->len >= pairs->count)
        return -1;
    ConfPair *p = pairs->pair + pairs->len;
    strncpy(p->key, key, 127);
    p->value = val;
    pairs->len++;

    return 0;
}

int 
confpairs_find(ConfPairs *pairs, char *key, int *v)
{
    int i;
    ConfPair *p = pairs->pair;
    for (i = 0; i < pairs->len; i++) {
        //DERROR("key:%s, pair->key:%s\n", key, p->key);
        if (strcmp(key, p->key) == 0) {
            *v = p->value;
            return TRUE;
        }
        p++;
    }
    return FALSE;
}

void
confpairs_destroy(ConfPairs *pairs)
{
    zz_free(pairs); 
}

/*
ConfPair*    
confpair_create(char *key, int val)
{
    ConfPair *pair = zz_malloc(sizeof(ConfPair));
    memset(pair, 0, sizeof(ConfPair));

    strncpy(pair->key, key, 127);
    pair->value = val;
    return pair;
}
void
confpair_destroy(ConfPair *pair)
{
    zz_free(pair); 
}

int            
confpair_init(ConfPair *pair, char *key, int val)
{
    strncpy(pair->key, key, 127);
    pair->value = val;
    pair->next = NULL;
    return 0; 
}*/

ConfParser* 
confparser_create(char *filename)
{
    ConfParser *parser = zz_malloc(sizeof(ConfParser));
    memset(parser, 0, sizeof(ConfParser));
     
    strncpy(parser->filename, filename, 255);

    return parser;
}

void        
confparser_destroy(ConfParser* parser)
{
    ConfParam *param = parser->params;
    ConfParam *tmp;
    while (param) {
        tmp = param->next;
        zz_free(param);
        param = tmp;
    }
    zz_free(parser);
}

int    
confparser_add_param(ConfParser* parser, void *addr, char *name,
                char type, char arraysize, void *arg)
{
    ConfParam  *param = zz_malloc(sizeof(ConfParam));
    memset(param, 0, sizeof(ConfParam));

    param->dst = addr;
    strncpy(param->name, name, 127);
    param->type = type;
    param->arraysize = arraysize;
    //param->pairs = pair;
    if (type == CONF_USER) {
        param->param.userfunc = arg;
    }else{
        param->param.pairs = arg;
    }
    param->next = NULL;

    if (parser->params == NULL) {
        parser->params = param;
        return 0;
    }
    ConfParam *prev = NULL;
    ConfParam *last = parser->params;
    while (last) {
        prev = last;
        last = last->next;
    }
    prev->next = param;
    
    return 0;
}


static void
clean_end(char *start)
{
    char *end = start;  
    while (*end != 0) {
        if (*end == '\r' || *end == '\n') {
            end -= 1;
            while (end > start && isblank(*end)) {
                end--;
            }   
            *(end + 1) = 0;
            break;
        }   
        end += 1;
    } 
}

static char*
find_array_item(char *start, char **last) 
{
    char *ret, *p; 

    if (*last) {
        ret = *last;
    }else{
        ret = start;
    }

    p = ret;
    
    while (*p) {
        if (*p == ',' || *p == ';') {
            *p = 0;
            *last = p + 1;
            return ret;
        }
        p++;
    }
    if (ret != p) {
        *last = p;
        return ret;
    }
    return NULL;
}

static int
bool_check(char *vstart)
{
    if (strcmp(vstart, "yes") == 0 || strcmp(vstart, "true") == 0) {
        return 1;
    }else if (strcmp(vstart, "no") == 0 || strcmp(vstart, "false") == 0) {
        return 0;
    }else{
        return -1;
    }
}

static int
enum_check(ConfParam *param, char *vstart, int *v)
{
    return confpairs_find(param->param.pairs, vstart, v);
}

int    
confparser_parse(ConfParser* parser)
{
    FILE *f;

    f = fopen(parser->filename, "r");
    if (NULL == f)
        return -1;
    
    char buf[4096];
    char *pstart;
    int  lineno = 0;
    char key[100] = {0};
    while (fgets(buf, 4096, f) != NULL) {
        pstart = buf;
        lineno++;

        while (isblank(*pstart)) pstart++;
        if (pstart[0] == '#' || pstart[0] == '\r' || pstart[0] == '\n') {
            continue;
        }

        if (*pstart == '=') {
            DERROR("config file parse error! not key at line:%d\n", lineno);
            fclose(f);
            return -1;
        }

        char *sp = strchr(pstart, '=');
        if (sp == NULL) {
            if (*key == '\0') {
                DERROR("config file parse error! \"=\" not found at line:%d\n", lineno);
                fclose(f);
                return -1;
            }
        } else {
            char *end = sp - 1;
            while (end > pstart && isblank(*end)) {
                end--;
            }
            *(end + 1) = 0;
            strcpy(key, pstart);

            pstart = sp + 1;
            while (isblank(*pstart)) pstart++;
            if (*pstart == '\r' || *pstart == '\n')
                continue;
        }
        clean_end(pstart);
            

        
        ConfParam *param = parser->params;
        char *tmp = NULL, *item = NULL;
        while (param) {
            if (strcmp(param->name, key) == 0) {
            //    lastname = param->name;
                if (param->arraysize && param->dsti >= param->arraysize) {
                    DERROR("config parser error, too many items: %s line:%d\n", param->name, lineno);
                    fclose(f);
                    return -1;
                }
                switch(param->type) {
                case CONF_INT:
                    if (param->arraysize) {
                        while ((item = find_array_item(pstart, &tmp)) != NULL) {
                            ((int*)param->dst)[param->dsti] = atoi(item);
                            //DERROR("%d\n", ((int*)param->dst)[param->dsti]);
                            param->dsti++;
                        }
                    }else{
                        *(int*)param->dst = atoi(pstart);
                    }
                    break;
                case CONF_FLOAT:
                    if (param->arraysize) {
                        while ((item = find_array_item(pstart, &tmp)) != NULL) {
                            ((float*)param->dst)[param->dsti] = atof(item);
                            param->dsti++;
                        }
                    }else{
                        *(float*)param->dst = atof(pstart);
                    }
                    break;
                case CONF_STRING: 
                    if (param->arraysize) {
                        while ((item = find_array_item(pstart, &tmp)) != NULL) {
                            snprintf(((char**)param->dst)[param->dsti], 1024, "%s", item);
                            param->dsti++;
                        }
                    }else{
                        snprintf((char*)param->dst, 1024, "%s", pstart);
                    }
                    break;
                case CONF_BOOL: {
                    int v = 0;    
                    if (param->arraysize) {
                        while ((item = find_array_item(pstart, &tmp)) != NULL) {
                            v = bool_check(item);
                            if (v < 0) {
                                DERROR("conf error, %s must yes/no true/false at line:%d\n", 
                                            param->name, lineno);
                                fclose(f);
                                return -1;
                            }

                            ((int*)param->dst)[param->dsti] = v;
                            param->dsti++;
                        }
                    }else{
                        v = bool_check(pstart);
                        if (v < 0) {
                            DERROR("conf error, %s must yes/no true/false at line:%d\n", 
                                        param->name, lineno);
                            fclose(f);
                            return -1;
                        }
                        *(int*)param->dst = v;
                    }
                    break;
                }
                case CONF_ENUM: { // 枚举只能是整型
                    int v = 0;
                    if (param->arraysize) {
                        while ((item = find_array_item(pstart, &tmp)) != NULL) {
                            if (enum_check(param, pstart, &v) == FALSE) {
                                DERROR("conf error, %s value error at line:%d\n", param->name, lineno);
                                fclose(f);
                                return -1;
                            }
                            ((int*)param->dst)[param->dsti] = v;
                            param->dsti++;
                        }
                    }else{
                        if (enum_check(param, pstart, &v) == FALSE) {
                            DERROR("conf error, %s value error at line:%d\n", param->name, lineno);
                            fclose(f);
                            return -1;
                        }
                        *(int*)param->dst = v;
                    }
                    break;
                }
                case CONF_USER: {
                    if (param->arraysize) {
                        while ((item = find_array_item(pstart, &tmp)) != NULL) {
                            if (param->param.userfunc(param->dst, item, param->dsti) == FALSE) {
                                DERROR("conf error, %s value error at line:%d\n", param->name, lineno);
                                fclose(f);
                                return -1;
                            }
                            //((int*)param->dst)[param->dsti] = v;
                            param->dsti++;
                        }
                    }else{
                        if (param->param.userfunc(param->dst, pstart, 0) == FALSE) {
                            DERROR("conf error, %s value error at line:%d\n", param->name, lineno);
                            fclose(f);
                            return -1;
                        }
                        //*(int*)param->dst = v;
                    }

                    break;
                }
                default:
                    DERROR("conf param type error at %s with type:%d\n", param->name, param->type);
                    fclose(f);
                    return -1;
                }
                break;
            }
            param = param->next;
        }
    }

    fclose(f);
    return 0;
}



