#ifndef __HASH__
#define __HASH__
#include <stdlib.h>
#include <types.h>

#define H_LEN  64

typedef struct {
    char key[32];
    int val;
} HashNode;

typedef struct __HashTable {
    size_t size;
    HashNode data[H_LEN];
    bool assigned[H_LEN];
} HashTable;


void init_ht(HashTable*);
bool insert_ht(HashTable*, char*, int);
bool exists_ht(HashTable*, char*);
int lookup_ht(HashTable*, char*);
void delete_ht(HashTable*, char*);
unsigned int hash_djb2n(char*, unsigned int);
unsigned int hash_djb2(char*);

#endif /* __HASH__ */
