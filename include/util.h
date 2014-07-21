#ifndef __UTIL_H__
#define __UTIL_H__
#include <types.h>
#include <stdlib.h>
#define BUFFER_BITS   11
#define CBUFFER_SIZE  (1 << BUFFER_BITS)

#define DECLARE_CIRCULAR_BUFFER(Type)                                                       \
    typedef struct {                                                                        \
        Type data[CBUFFER_SIZE];                                                            \
        uint32_t head : BUFFER_BITS;                                                        \
        uint32_t tail : BUFFER_BITS;                                                        \
        uint32_t remaining : (BUFFER_BITS + 1);                                             \
    } CircularBuffer_##Type;                                                                \
                                                                                            \
    static inline void initcb_##Type(CircularBuffer_##Type *cbuf) {                         \
        cbuf->head = 0;                                                                     \
        cbuf->tail = 0;                                                                     \
        cbuf->remaining = CBUFFER_SIZE;                                                     \
    }                                                                                       \
                                                                                            \
    static inline int write_##Type(CircularBuffer_##Type *cbuf, const volatile Type *buf, uint32_t len) { \
        if (cbuf->remaining < len) {                                                        \
            /* TODO: Catch and handle this occuring, especially in uart. */                 \
            Panic("CircularBuffer (%x): Insufficient space (%d) to copy message (%d).\r\n", \
                  cbuf, cbuf->remaining, len);                                              \
        }                                                                                   \
                                                                                            \
        cbuf->remaining -= len;                                                             \
        while (len-- > 0) {                                                                 \
            cbuf->data[cbuf->tail++] = *buf++;                                              \
        }                                                                                   \
        return len;                                                                         \
    }                                                                                       \
                                                                                            \
    static inline int read_##Type(CircularBuffer_##Type *cbuf, volatile Type *buf, uint32_t len) { \
        int iremaining = cbuf->remaining;                                                   \
        while (len-- > 0 && cbuf->remaining < CBUFFER_SIZE) {                               \
            *buf++ = cbuf->data[cbuf->head++];                                              \
            ++cbuf->remaining;                                                              \
        }                                                                                   \
        return cbuf->remaining - iremaining;                                                \
    }                                                                                       \
                                                                                            \
    static inline int length_##Type(CircularBuffer_##Type *cbuf) {                          \
        return CBUFFER_SIZE - cbuf->remaining;                                              \
    }                                                                                       \



inline uint32_t min3(uint32_t a, uint32_t b, uint32_t c);

#endif /* __UTIL_H__ */
