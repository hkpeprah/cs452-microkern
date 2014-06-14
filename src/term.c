#include <term.h>
#include <string.h>

#define DEBUG_MOVE   "\033[s\033[%d;%dr\033[%d;%dH\r\n"
#define TERM_MOVE    "\033[%d;%dr\033[%d;%dH\033[u"


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
        kputstr(SAVE_CURSOR);
    #else
        kprintf(SET_SCROLL, TOP_HALF, TERMINAL_HEIGHT);
        kprintf(MOVE_CURSOR, TOP_HALF, 0);
        kputstr(SAVE_CURSOR);
    #endif
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
