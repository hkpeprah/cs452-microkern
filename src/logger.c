#include <logger.h>
#include <mem.h>
#include <vargs.h>
#include <stdio.h>
#include <stdlib.h>
#include <bwio.h>

static char *logp;
static uint32_t tail;

void initLogger() {
    logp = (char*) (getMem()->addr - MEM_BLOCK_SIZE + 4);
    tail = 0;
}

void printLogger(int start) {
    while(start < tail) {
        bwputc(COM2, logp[start++]);
    }
}


void log_f(const char *fmt, ...) {
    int len;
    char buffer[256];
    va_list va;

    va_start(va, fmt);
    len = format(fmt, va, buffer);
    va_end(va);

    memcpy(&(logp[tail]), buffer, len);
    tail += len;
}

