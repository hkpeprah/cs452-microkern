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
    kputstr("\033[37mCS452 Real-Time Microkernel (Version 0.1.9)\r\n");
    kputstr("Copyright <c> Max Chen (mqchen), Ford Peprah (hkpeprah).  All rights reserved.\033[0m\r\n");
    kputstr("\033[32mTime:           \033[35mCPU Idle:\033[0m\r\n\r\n");
    TERM_OFFSET += 5;
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
    if (id >= MULTI_SWITCH_OFFSET + TRAIN_SWITCH_COUNT - 4) {
        id -= (MULTI_SWITCH_OFFSET + TRAIN_SWITCH_COUNT - 4);
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
    static volatile char buffer[25] = { [0 ... 24] = ' ' };
    static volatile int index = 0;
    int i;

    buffer[25] = '\0';

    /* shift the character buffer */
    for (i = 24; i > 4; --i) {
        buffer[i] = buffer[i - 5];
    }

    buffer[0] = module;
    buffer[1] = (id / 10) + '0';
    buffer[2] = (id % 10) + '0';

    printf("\033[s\033[%d;0H%s\033[u", TERM_OFFSET + 6, buffer);
    index = (index + 4) % 25;
}


void displayInfo() {
    unsigned int i;
    unsigned int count;
    Switch_t *swtch;

    count = TRAIN_SWITCH_COUNT;
    kputstr("====Switches===\r\n");
    while (count > 0) {
        for (i = 0; i < MIN(count, 9); ++i) {
            swtch = getSwitch(TRAIN_SWITCH_COUNT - count + i);
            if (swtch->id < 10) {
                kputstr("00");
            } else if (swtch->id < 100) {
                kputstr("0");
            }
            kprintf("%d: %c    ", swtch->id, swtch->state);
        }
        count -= MIN(9, count);
        kputstr(NEW_LINE);
    }
    kputstr("\r\n====Sensors====\r\n\r\n\r\n");
    kputstr(SAVE_CURSOR);
    kprintf(SET_SCROLL, TERM_OFFSET + 9, TERMINAL_HEIGHT);
    kputstr(RESTORE_CURSOR);
}


void updateTime(unsigned int count, unsigned int cpu) {
    unsigned int minutes;
    unsigned int seconds;
    unsigned int t_seconds;

    minutes = count / 6000;
    seconds = (count / 100) % 60;
    t_seconds = count % 100;
    printf("\033[s\033[%d;7H%d%d:%d%d:%d%d \033[11C%d%%            \033[u", TERM_OFFSET - 2, minutes / 10, minutes % 10,
           seconds / 10, seconds % 10, t_seconds / 10, t_seconds % 10, cpu);
}
