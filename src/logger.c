#include <logger.h>
#include <util.h>
#include <mem.h>
#include <vargs.h>
#include <stdio.h>
#include <stdlib.h>
#include <term.h>
#include <bwio.h>

static char *logp;
static uint32_t *tail;

void initLogger() {
    int mem = getMem()->addr - MEM_BLOCK_SIZE + 4;
    tail = (uint32_t*) mem;
    logp = (char*) (mem + 4);
    kdebug("Log at 0x%x, length at 0x%x", logp, tail);
    *tail = 0;
}

void printLog(uint32_t start, uint32_t end) {
    char c;
    while(start < MIN(end, *tail)) {
        c = logp[start++];
        bwputc(COM2, c);
        if (c == '\n') {
            bwgetc(COM2);
        }
    }
}

void dumpLog() {
    kprintf("\r\nLog at: 0x%x of length: %d\r\n", logp, *tail);
    printLog(0, *tail);
}


int sys_log(const char *buf, int len) {
    if (*tail + len > (MEM_BLOCK_SIZE - 4)) {
        return OUT_OF_SPACE;
    }

    memcpy(&(logp[*tail]), buf, len);
    *tail += len;
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
