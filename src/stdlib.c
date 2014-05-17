/*
 * stdlib.c - Standard Library functions
 */
#include <stdlib.h>


void *memcpy(void *destination, const void *source, size_t num) {
    char *dst = (char*)destination;
    char *src = (char*)source;

    while (num--) *dst++ = *src++;
    return destination;
}
