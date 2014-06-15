#include <logger.h>
#include <util.h>
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

void printLog(uint32_t start, uint32_t end) {
    while(start < MIN(end, tail)) {
        char c = logp[start++];
        bwputc(COM2, c);
    }
}

void dumpLog() {
    bwprintf(COM2, "Log at: 0x%x of length: %d\n", logp, tail);
    printLog(0, tail);
}


int sys_log(const char *buf, int len) {
    if (tail + len > (MEM_BLOCK_SIZE - 4)) {
        bwprintf(COM2, "Oh noes out of space");
        return OUT_OF_SPACE;
    }

    memcpy(&(logp[tail]), buf, len);
    tail += len;
    return 0;
}

int sys_log_f(const char *fmt, ...) {
    int len;
    char buffer[256];
    va_list va;

    va_start(va, fmt);
    len = format(fmt, va, buffer);
    va_end(va);

    return sys_log(buffer, len);
}
