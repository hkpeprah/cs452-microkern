#ifndef __SERVER__
#define __SERVER__

#define REGISTER  0
#define WHOIS     1


typedef struct {
    int type;
    char *name;
    int tid;
} Lookup;


void NameServer();
int RegisterAs(char*);
int WhoIs(char*);

#endif /* __SERVER__ */
