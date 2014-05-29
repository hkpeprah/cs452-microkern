#ifndef __SERVER__
#define __SERVER__

#define REGISTER  0
#define WHOIS     1
#define DELETE    2

extern int nameserver_tid;


typedef struct {
    int type;
    char *name;
    int tid;
} Lookup;


void NameServer();
int RegisterAs(char*);
int WhoIs(char*);
int UnRegister(char*);

#endif /* __SERVER__ */
