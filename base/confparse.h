#ifndef BASE_CONFPARSE_H
#define BASE_CONFPARSE_H

#include <stdio.h>
#include <stdint.h>

#define CONF_INT		1
#define CONF_FLOAT		2
#define CONF_STRING		3	
#define CONF_BOOL		4	
#define CONF_ENUM		5	
#define CONF_USER		6


typedef struct _conf_pair
{
	char	key[128];
	int		value;
	//struct _conf_pair *next;
}ConfPair;

typedef struct _conf_pairs
{
	int count;
	int len; // used 
	ConfPair	pair[0];
}ConfPairs;

ConfPairs*	confpairs_create(int count);
int			confpairs_add(ConfPairs *, char *key, int val);
int			confpairs_find(ConfPairs *, char *key, int *v);
void		confpairs_destroy(ConfPairs *);

/*
ConfPair*	confpair_create(char *key, int val);
void		confpair_destroy(ConfPair *);
int			confpair_init(ConfPair *, char *key, int val);
*/

typedef int (*ConfUserFunc)(void *cf, char *value, int i);

typedef struct _conf_param
{
	void	 *dst;
	int		 dsti; // index for dst
	char	 name[128];
	char	 type;
	uint8_t	 arraysize;
	union{
		ConfPairs		*pairs; // enum have pair
		ConfUserFunc	userfunc;
	}param;
	struct _conf_param *next;
}ConfParam;

typedef struct _conf_parser
{
	ConfParam	*params;	
	char		filename[256];
}ConfParser;

ConfParser* confparser_create(char *filename);
void		confparser_destroy(ConfParser*);
int			confparser_add_param(ConfParser*, void *addr, char *name, 
				char type, char arraysize, void *arg);
int			confparser_parse(ConfParser*);

#endif
