/*
 * hash.c - Hash functions
 * source: http://stackoverflow.com/questions/7666509/
 */
#include <hash.h>
#include <syscall.h>


unsigned int hash_djb2(char *str) {
    /*
     * djb2 hash function
     */
    int c = 0;
    unsigned int hash = 5381;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}


void init_ht(HashTable *table) {
    unsigned int i;
    table->size = H_LEN;
    for (i = 0; i < table->size; ++i) {
        table->assigned[i] = false;
    }
}


bool exists_ht(HashTable *table, char *key) {
    unsigned int hash;

    hash = hash_djb2(key) % table->size;
    return table->assigned[hash];
}


bool insert_ht(HashTable *table, char *key, int val) {
    unsigned int hash;

    hash = hash_djb2(key) % table->size;
    if (table->assigned[hash] == false) {
        table->assigned[hash] = 1;
        table->data[hash] = val;
        return true;
    }

    Panic("HashTable: Collision on key %s", key);
    return false;
}


int lookup_ht(HashTable *table, char *key) {
    /* looks up value in hash table, returns -1 if not found */
    unsigned int hash;

    hash = hash_djb2(key) % table->size;
    if (hash < table->size && table->assigned[hash]) {
        return table->data[hash];
    }

    return MINSINT;
}


void delete_ht(HashTable *table, char *key) {
    unsigned int hash;

    hash = hash_djb2(key) % table->size;
    table->assigned[hash] = 0;
}
