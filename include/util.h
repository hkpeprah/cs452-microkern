#ifndef __UTIL_H__
#define __UTIL_H__
#include <types.h>

#define MIN(x, y) (x < y ? x : y)
#define MAX(x, y) (x > y ? x : y)

#define BUFFER_BITS 6
#define BUFFER_SIZE (1 << BUFFER_BITS)

typedef struct {
    char data[BUFFER_SIZE];
    uint32_t head : BUFFER_BITS;
    uint32_t tail : BUFFER_BITS;
} CircularBuffer_t;

int length(CircularBuffer_t *cbuf);
int write(CircularBuffer_t *cbuf, const char *buf, uint32_t len);
int read(CircularBuffer_t *cbuf, char *buf, uint32_t len);

#endif /* __UTIL_H__ */
