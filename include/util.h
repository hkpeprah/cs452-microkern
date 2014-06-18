#ifndef __UTIL_H__
#define __UTIL_H__
#include <types.h>
#define MIN(x, y) (x < y ? x : y)
#define MAX(x, y) (x > y ? x : y)
#define BUFFER_BITS 10
#define CBUFFER_SIZE (1 << BUFFER_BITS)


typedef struct {
    char data[CBUFFER_SIZE];
    uint32_t head : BUFFER_BITS;
    uint32_t tail : BUFFER_BITS;
    uint32_t remaining : (BUFFER_BITS + 1);
} CircularBuffer_t;

void initcb(CircularBuffer_t *cbuf);
inline int length(CircularBuffer_t *cbuf);
int write(CircularBuffer_t *cbuf, const volatile char *buf, uint32_t len);
int read(CircularBuffer_t *cbuf, volatile char *buf, uint32_t len);

#endif /* __UTIL_H__ */
