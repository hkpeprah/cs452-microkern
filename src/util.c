#include <util.h>
#include <stdlib.h>
#include <term.h>

static inline uint32_t min3(uint32_t a, uint32_t b, uint32_t c) {
    a = (b < a) ? b : a;
    a = (c < a) ? c : a;
    return a;
}

static uint32_t diff(int from, int to) {
    return (from < to) ? (to - from) : (BUFFER_SIZE - from + to);
}

int length(CircularBuffer_t *cbuf) {
    return diff(cbuf->head, cbuf->tail);
}

/* TODO: optimize these a-la memcpy */
int write(CircularBuffer_t *cbuf, const char *buf, uint32_t len) {

    uint32_t remaining = diff(cbuf->tail, cbuf->head);

    if (remaining < len) {
        error("CircularBuffer: Insufficient space to copy message");
        return -1; // TODO: define this
    }


    while (len --> 0) {
        cbuf->data[cbuf->tail++] = *buf++;
    }

#if 0
    printf("after write, head = %d, tail = %d, remaining = %d\n", cbuf->head, cbuf->tail, diff(cbuf->tail, cbuf->head));
#endif

    return len;
}

int read(CircularBuffer_t *cbuf, char *buf, uint32_t len) {
    int clen = 0;

    while (len --> 0 && cbuf->head != cbuf->tail) {
        *buf++ = cbuf->data[cbuf->head++];
        ++clen;
    }

#if 0
    printf("after read, head = %d, tail = %d, remaining = %d\n", cbuf->head, cbuf->tail, diff(cbuf->tail, cbuf->head));
#endif

    return clen;
}
