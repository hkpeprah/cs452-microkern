/*
 * stdlib.c - Standard Library functions
 */
#include <stdlib.h>
#include <ts7200.h>
#include <term.h>
#include <task.h>


void *memset(void *s, int c, unsigned int n) {
    unsigned char *p = s;
    while (n --> 0) {
        *p++ = (unsigned char)c;
    }
    return s;
}


void *memcpy(void *destination, const void *source, size_t num) {
    char *dst = (char*)destination;
    char *src = (char*)source;

    while (num--) *dst++ = *src++;
    return destination;
}


static int flushUartRegister(int base) {
    unsigned char ch;
    volatile int *flags, *data;

    flags = (int*)(base + UART_FLAG_OFFSET);
    data = (int*)(base + UART_DATA_OFFSET);

    if (!(*flags & RXFF_MASK) || *flags & RXFE_MASK) {
        return -1;
    }

    ch = *data;
    return ch;
}


void flushUart(int base) {
    while (flushUartRegister(base) > 0);
}


void initUart(short uart, int speed, bool fifo) {
    int base;
    int *line;
    int *high, *low;

    base = 0;
    switch (uart) {
        case COM1:
            base = UART1_BASE;
            break;
        case COM2:
            base = UART2_BASE;
            break;
    }

    high = (int*)(base + UART_LCRM_OFFSET);
    low = (int*)(base + UART_LCRL_OFFSET);

    switch (speed) {
        /* baud divisor rate */
        case 115200:
            *high = 0x0;
            *low = 0x3;
            break;
        case 2400:
            *high = 0x0;
            *low = 0xBF;
            break;
    }

    line = (int*)(base + UART_LCRH_OFFSET);
    if (!fifo) {
        *line = ((*line) & ~FEN_MASK);
    }

    flushUart(base);
}
