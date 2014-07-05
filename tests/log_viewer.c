#include <bwio.h>
#include <stdlib.h>
#include <term.h>

#define LOG_ADDR 0xa10008

int main() {
    initUart(IO, 115200, false);
    char *logp = (char*) LOG_ADDR;
    int *loglen =  (int*) (LOG_ADDR - 4);
    int i = 0;
    char c;

    bwprintf(COM2, "Log length at 0x%x with value: %d\n", loglen, *loglen);

    while(i < *loglen) {
        c = logp[i++];
        bwputc(COM2, c);
        if(c == '\n') {
            bwgetc(COM2);
        }
    }
}
