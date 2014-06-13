/*
 * stdlib.c - Standard Library functions
 */
#include <stdlib.h>
#include <ts7200.h>


void *memcpy(void *destination, const void *source, size_t num) {
    char *dst = (char*)destination;
    char *src = (char*)source;

    while (num--) *dst++ = *src++;
    return destination;
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
    /* TODO: Flush whatever is in hold registers / read registers ? */
}
