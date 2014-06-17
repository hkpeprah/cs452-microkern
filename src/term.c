#include <term.h>
#include <string.h>
#include <train.h>
#include <stdlib.h>

#define DEBUG_MOVE   "\033[s\033[%d;%dr\033[%d;%dH\r\n"
#define TERM_MOVE    "\033[%d;%dr\033[%d;%dH\033[u"

static int TERM_OFFSET;


void initDebug() {
    kputstr(ERASE_SCREEN);
    kprintf(CHANGE_COLOR, 0);
    #if DEBUG
        unsigned int i;
        kprintf(MOVE_CURSOR, BOTTOM_HALF, 0);
        /* print seperator for debug/command halves */
        for (i = 0; i < TERMINAL_WIDTH; ++i) {
            kputstr("=");
        }
        kprintf(MOVE_CURSOR, TOP_HALF, 0);
        kputstr("Debugging Enabled..");
        kprintf(SET_SCROLL, BOTTOM_HALF + 1, TERMINAL_HEIGHT);
        kprintf(MOVE_CURSOR, BOTTOM_HALF + 1, 0);
        TERM_OFFSET = BOTTOM_HALF;
    #else
        TERM_OFFSET = TOP_HALF;
        kprintf(MOVE_CURSOR, TOP_HALF, 0);
    #endif
    kputstr(SAVE_CURSOR);
    kputstr("CS452 Real-Time Microkernel (Version 0.1.1001)\r\n");
    kputstr("Copyright <c> Max Chen, Ford Peprah.  All rights reserved.\r\n");
    TERM_OFFSET += 3;
}


void debug(char *fmt, ...) {
    #if DEBUG
        va_list va;
        va_start(va, fmt);
        move_to_debug();
        newline();
        printformatted(IO, fmt, va);
        return_to_term();
        va_end(va);
    #endif
}

void debugc(char *fmt, unsigned int color, ...) {
    #if DEBUG
        va_list va;
        va_start(va, color);
        change_color(color);
        move_to_debug();
        newline();
        printformatted(IO, fmt, va);
        return_to_term();
        end_color();
        va_end(va);
    #endif
}


void printSwitch(unsigned int id, char state) {
    unsigned int lines;

    lines = TERM_OFFSET + 1;
    if (id >= MULTI_SWITCH_OFFSET) {
        id -= (MULTI_SWITCH_OFFSET - 1);
        lines += 2;
    } else {
        while (id > 9) {
            id -= 9;
            lines += 1;
        }
    }

    printf("%s\033[%d;%dH%c%s", SAVE_CURSOR, lines, 6 + (10 * (id - 1)),
           toUpperCase(state), RESTORE_CURSOR);
}


void printSensor(char module, unsigned int id) {
    static char buf[26] = {0};
    static int index = 0;

    buf[index] = module;
    buf[index + 1] = (id / 10) + '0';
    buf[index + 2] = (id % 10) + '0';
    printf("%s\r\n", buf);
    printf("%s\033[%d;0H%s%s", SAVE_CURSOR, TERM_OFFSET + 5 , buf, RESTORE_CURSOR);
    index = (index + 5) % 25;
}


void displayInfo() {
    unsigned int i;
    unsigned int count;
    Switch_t *swtch;

    erase_line();
    count = TRAIN_SWITCH_COUNT;
    puts("====Switches===\r\n");
    while (count > 0) {
        for (i = 0; i < MIN(count, 9); ++i) {
            swtch = getSwitch(TRAIN_SWITCH_COUNT - count + i);
            if (swtch->id < 10) {
                puts("00");
            } else if (swtch->id < 100) {
                puts("0");
            }
            printf("%d: %c    ", swtch->id, swtch->state);
        }
        count -= MIN(9, count);
        puts(NEW_LINE);
    }
    puts("\r\n====Sensors====\r\n\r\n\r\n");
    puts(SAVE_CURSOR);
}
