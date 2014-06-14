#include <term.h>
#include <string.h>

#define DEBUG_MOVE   "\033[s\033[%d;%dr\033[%d;%dH\r\n"
#define TERM_MOVE    "\033[%d;%dr\033[%d;%dH\033[u"


void initDebug() {
    kputstr(ERASE_SCREEN);
    #if DEBUG
        unsigned int i;
        kprintf(MOVE_CURSOR, BOTTOM_HALF, 0);
        /* print seperator */
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
        kprintf(MOVE_CURSOR, 0, TOP_HALF);
        kputstr(SAVE_CURSOR);
    #endif
}


#if DEBUG
    static void debugformat(char *src, char *dst, ...) {
        va_list va;
        va_start(va, dst);
        stringformat(src, dst, va);
        va_end(va);
    }
#endif


void debug(char *format, ...) {
    #if DEBUG
        char buffer[128];
        char *tmp, *term_move, *debug_move;
        va_list va;

        tmp = buffer;
        term_move = TERM_MOVE;
        debug_move = DEBUG_MOVE;

        while ((*tmp++ = *debug_move++));
        *tmp++ = '%';
        *tmp++ = 's';
        while ((*tmp++ = *term_move++));

        debugformat(buffer, buffer, TOP_HALF, BOTTOM_HALF - 1, 0, BOTTOM_HALF - 1,
                    format, BOTTOM_HALF + 1, TERMINAL_HEIGHT, 0, BOTTOM_HALF + 1);

        kprintf("\r\nLength: %u\r\n", strlen(buffer));

        va_start(va, format);
        printformatted(IO, buffer, va);
        va_end(va);
    #endif
}
