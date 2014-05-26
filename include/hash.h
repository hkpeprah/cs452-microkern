#ifndef __HASH__
#define __HASH__
#include <stdlib.h>

#define H_LEN  48


typedef struct {
    uint32_t size;
    int32_t data[H_LEN];
    uint32_t assigned[H_LEN];
} HashTable;


void init_ht(HashTable*);
unsigned int insert_ht(HashTable*, char*, int);
int lookup_ht(HashTable*, char*);
unsigned int hash_djb2(char*);
unsigned int hash_rotation(char*);

#endif /* __HASH__ */
