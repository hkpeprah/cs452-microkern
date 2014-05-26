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


unsigned int hash_rotation(char *str) {
    int result = 0x55555555;

    while (*input) {
        result ^= *input++;
        result = rol(result, 5);
    }

    return result;
}
