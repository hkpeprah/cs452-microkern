#ifndef __NAME_SERVER__
#define __NAME_SERVER__
#include <hash.h>


typedef struct {
    HashTable ht;    /* addresses of the receipients are stored in hash table */
} NameServer;



#endif /* __NAME_SERVER__ */
