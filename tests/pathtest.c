#include <types.h>
#include <kernel.h>
#include <interrupt.h>
#include <uart.h>
#include <server.h>
#include <ts7200.h>
#include <syscall.h>
#include <k_syscall.h>
#include <logger.h>
#include <clock.h>
#include <stdio.h>
#include <term.h>
#include <utasks.h>
#include <stdlib.h>
#include <track_data.h>
#include <path.h>

void testPath() {
    int i;
    int start, finish;
    int distance;
    char ch;
    track_node track[TRACK_MAX];
    init_track(track);

    track_node *path[64] = {0};

    while(true) {
        start = 0;
        finish = 0;
        kprintf("\nstart: ");

        while ( (ch = bwgetc(COM2)) ) {
            if (ch == 0xD) {
                break;
            }
            bwputc(COM2, ch);
            start = start * 10 + ch - '0';
        }

        kprintf("\nfinish: ");

        while ( (ch = bwgetc(COM2)) ) {
            if (ch == 0xD) {
                break;
            }
            bwputc(COM2, ch);
            finish = finish * 10 + ch - '0';
        }

        kprintf("\nnavigating from %d to %d\n", start, finish);

        if (start < TRACK_MAX && finish < TRACK_MAX) {
            kprintf("\nnodes %s to %s\n", track[start].name, track[finish].name);
        } else {
            kprintf("invalid indices!\n");
            continue;
        }

        kprintf("path: 0x%x\n", path);
        distance = findPath(&track[start], &track[finish], path, 64, NULL, 0);
        kprintf("num of nodes: %d\n", distance);

        for (i = 0; i < distance; ++i) {
            kprintf("%s\n", path[i]->name);
        }

        kprintf("go again? y/n\n");
        if (bwgetc(COM2) != 'y') {
            break;
        }
    }
}


int main() {
    initUart(IO, 115200, false);
    //testHeap();
    testPath();
    return 0;
}
