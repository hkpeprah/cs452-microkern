#ifndef __UTASKS__
#define __UTASKS__
#include <types.h>
#include <task.h>

#define SIGNUP    0
#define WHOIS     1
#define WIN       2
#define LOSE      3
#define TIE       4
#define ROCK      5
#define PAPER     6
#define SCISSORS  7
#define PLAY      8
#define QUIT      9


typedef struct {
    int status;
} GameResult;


typedef struct {
    int type;
    int d0;
    int d1;
} GameRequest;


typedef struct {
    int type;
    char *name;
    int tid;
} Lookup;


void firstTask();
void nameServer();
void server();
void client();
int RegisterAs(char*, int);
int WhoIs(char*);

#endif /* __UTASKS__ */
