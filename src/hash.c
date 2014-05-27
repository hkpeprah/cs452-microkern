/*
 * hash.c - Hash functions
 * source: http://stackoverflow.com/questions/7666509/
 */
#include <hash.h>


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
    uint32_t i = 0;
    for (i = 0; i < table->size; ++i) {
        table->assigned[i] = 0;
    }
}


void resize_ht(HashTable *table, uint32_t new_size, int32_t *data, uint32_t *assigned) {
    table->size = new_size;
    table->data = data;
    table->assigned = assigned;
    init_ht(table);
}


unsigned int insert_ht(HashTable *table, char *key, int val) {
    uint32_t hash;

    hash = hash_djb2(key) % table->size;
    if (!table->assigned[hash]) {
        table->assigned[hash] = 1;
        table->data[hash] = val;
        return 1;
    }

    return 0;
}


int lookup_ht(HashTable *table, char *key) {
    uint32_t hash;

    hash = hash_djb2(key) % table->size;
    if (hash < table->size && table->assigned[hash]) {
        return table->data[hash];
    }

    return 0;
}
