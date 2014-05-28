#ifndef __HASH__
#define __HASH__
#include <stdlib.h>

#define H_LEN  64


typedef struct {
    unsigned int size;
    int data[H_LEN];
    int assigned[H_LEN];
} HashTable;


void init_ht(HashTable*);
unsigned int insert_ht(HashTable*, char*, int);
int lookup_ht(HashTable*, char*);
unsigned int hash_djb2(char*);

#endif /* __HASH__ */
