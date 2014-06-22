#ifndef __SERVER__
#define __SERVER__

#define REGISTER  0
#define WHOIS     1
#define DELETE    2


typedef struct {
    int type : 8;
    char *name;
    int tid;
} Lookup;


void NameServer();
int RegisterAs(char*);
int WhoIs(char*);
int UnRegister(char*);

#endif /* __SERVER__ */
