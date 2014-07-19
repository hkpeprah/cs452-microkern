#include <term.h>
#include <string.h>
#include <train.h>
#include <stdlib.h>
#include <calibration.h>

#define DEBUG_MOVE   "\033[s\033[%d;%dr\033[%d;%dH\r\n"
#define TERM_MOVE    "\033[%d;%dr\033[%d;%dH\033[u"

static volatile int TERM_OFFSET;
static int TERM_BOTTOM;
static unsigned int SENSORS_PER_LINE;


void initDebug() {
    initCalibration();
    kprintf(HIDE_CURSOR ERASE_SCREEN CHANGE_COLOR, 0);
    kprintf(SET_COLS_132 "\033[7;%dh", TERMINAL_WIDTH);
    #if DEBUG
        unsigned int i;
        kprintf(MOVE_CURSOR, BOTTOM_HALF, 0);
        /* print seperator for debug/command halves */
        for (i = 0; i < TERMINAL_WIDTH; ++i) {
            kputstr("=");
        }
        kprintf(MOVE_CURSOR, TOP_HALF, 0);
        kprintf(SET_SCROLL, BOTTOM_HALF + 1, TERMINAL_HEIGHT);
        kprintf(MOVE_CURSOR, BOTTOM_HALF + 1, 0);
        TERM_OFFSET = BOTTOM_HALF;
    #else
        TERM_OFFSET = TOP_HALF;
        kprintf(MOVE_CURSOR, TOP_HALF, 0);
    #endif
    kputstr("\033[37mCS452 Real-Time Microkernel (Version 0.2.0)\r\n");
    kputstr("Copyright <c> Max Chen (mqchen), Ford Peprah (hkpeprah).  All rights reserved.\033[0m\r\n");
    kputstr("\033[32mTime:           \033[35mCPU Idle:\033[0m\r\n\r\n");
    TERM_OFFSET += 5;
}


#if DEBUG
static void debugformatted(char *fmt, va_list va) {
    char buffer[100] = {0};

    strcpy(buffer, SAVE_CURSOR "\033[0;");
    uitoa(BOTTOM_HALF - 1, 10, &buffer[strlen(buffer)]);
    strcat(buffer, "r" "\033[");
    uitoa(BOTTOM_HALF - 1, 10, &buffer[strlen(buffer)]);
    strcat(buffer, ";0H\r\n");
    strcat(buffer, fmt);
    strcat(buffer, "\033[");
    uitoa(TERM_BOTTOM, 10, &buffer[strlen(buffer)]);
    strcat(buffer, ";");
    uitoa(TERMINAL_HEIGHT, 10, &buffer[strlen(buffer)]);
    strcat(buffer, "r" RESTORE_CURSOR);

    printformatted(IO, buffer, va);
}
#endif


void debug(char *fmt, ...) {
    #if DEBUG
        va_list va;
        va_start(va, fmt);
        debugformatted(fmt, va);
        va_end(va);
    #endif
}


void debugc(char *fmt, unsigned int color, ...) {
    #if DEBUG
        char buffer[100] = {0};
        va_list va;
        va_start(va, color);

        strcpy(buffer, "\033[");
        uitoa(color, 10, &buffer[strlen(buffer)]);
        strcat(buffer, "m");
        strcat(buffer, fmt);
        strcat(buffer, "\033[0m");

        debugformatted(buffer, va);
        va_end(va);
    #endif
}


void printSwitch(unsigned int id, char state) {
    unsigned int lines;

    lines = TERM_OFFSET + 1;
    if (id >= MULTI_SWITCH_OFFSET + TRAIN_SWITCH_COUNT - 4) {
        id -= MULTI_SWITCH_OFFSET;
    }

    lines += ((id - 1) / SENSORS_PER_LINE);
    id = (id - 1) % SENSORS_PER_LINE;
    printf(SAVE_CURSOR MOVE_CURSOR "%c" RESTORE_CURSOR, lines, 6 + (10 * id), toUpperCase(state));
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

    printf(SAVE_CURSOR "\033[%d;0H" "%s" RESTORE_CURSOR, TERM_BOTTOM - 2, buffer);
    index = (index + 4) % 25;
}


void displayInfo() {
    Switch_t *swtch;
    unsigned int i, count, lines;

    SENSORS_PER_LINE = RIGHT_HALF / 10;
    lines = SENSORS_PER_LINE;
    lines = (TRAIN_SWITCH_COUNT / lines) + (TRAIN_SWITCH_COUNT % lines);
    count = TRAIN_SWITCH_COUNT;
    kdebug("Debugging Enabled...\r\nNumber of Debug Lines: %d", lines);
    kprintf("====Switches===" MOVE_TO_COL CHANGE_COLOR "Train Calibration:" CHANGE_COLOR "\r\n" MOVE_TO_COL
            "| Train | Velocity | Landmark | Distance (mm) | Next Landmark | ETA (ticks) | ATA (ticks) |   Resv   "
            MOVE_TO_COL, RIGHT_HALF + 5, YELLOW, 0, RIGHT_HALF + 5, 0);
    while (count > 0) {
        for (i = 0; i < MIN(count, SENSORS_PER_LINE); ++i) {
            if (count <= 4) {
                swtch = getSwitch(MULTI_SWITCH_OFFSET + (TRAIN_SWITCH_COUNT - count + i + 1));
            } else {
                swtch = getSwitch(TRAIN_SWITCH_COUNT - count + i + 1);
                if (swtch->id < 10) {
                    kputstr("00");
                } else if (swtch->id < 100) {
                    kputstr("0");
                }
            }
            kprintf("%d: %c    ", swtch->id, SWITCH_CHAR(swtch->state));
        }
        count -= MIN(SENSORS_PER_LINE, count);
        kputstr(NEW_LINE);
    }
    kputstr("\r\n====Sensors====\r\n\r\n\r\n");
    kprintf(SAVE_CURSOR SET_SCROLL RESTORE_CURSOR, TERM_OFFSET + lines + 4, TERMINAL_HEIGHT);
    TERM_BOTTOM = TERM_OFFSET + lines + 4;
}


void updateTime(unsigned int count, unsigned int cpu) {
    unsigned int minutes;
    unsigned int seconds;
    unsigned int t_seconds;

    minutes = count / 6000;
    seconds = (count / 100) % 60;
    t_seconds = count % 100;
    printf(SAVE_CURSOR "\033[%d;7H" "%d%d:%d%d:%d%d \033[11C%d%% " RESTORE_CURSOR,
           TERM_OFFSET - 2, minutes / 10, minutes % 10, seconds / 10, seconds % 10, t_seconds / 10, t_seconds % 10, cpu);
}


unsigned int getTermOffset() {
    return TERM_OFFSET;
}
