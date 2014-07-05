#include <util.h>
#include <stdlib.h>
#include <term.h>
#include <logger.h>


static inline uint32_t min3(uint32_t a, uint32_t b, uint32_t c) {
    a = (b < a) ? b : a;
    a = (c < a) ? c : a;
    return a;
}


void initcb(CircularBuffer_t *cbuf) {
    cbuf->head = 0;
    cbuf->tail = 0;
    cbuf->remaining = CBUFFER_SIZE;
}


int length(CircularBuffer_t *cbuf) {
    return CBUFFER_SIZE - cbuf->remaining;
}


/* TODO: optimize these a-la memcpy */
int write(CircularBuffer_t *cbuf, const volatile char *buf, uint32_t len) {
    if (cbuf->remaining < len) {
        error("CircularBuffer: Insufficient space to copy message");
        return -1; // TODO: define this
    }

    cbuf->remaining -= len;

    while (len-- > 0) {
        cbuf->data[cbuf->tail++] = *buf++;
    }

#if LOG
    Log("0x%x wt, hd {%d} tl {%d}, rem = {%d}\n", cbuf, cbuf->head, cbuf->tail, cbuf->remaining);
#endif
    return len;
}


int read(CircularBuffer_t *cbuf, volatile char *buf, uint32_t len) {
    int iremaining = cbuf->remaining;

    while (len-- > 0 && cbuf->remaining < CBUFFER_SIZE) {
        *buf++ = cbuf->data[cbuf->head++];
        ++cbuf->remaining;
    }

#if LOG
    Log("0x%x rd, hd {%d} tl {%d}, rem = {%d}\n", cbuf, cbuf->head, cbuf->tail, cbuf->remaining);
#endif
    return cbuf->remaining - iremaining;
}
