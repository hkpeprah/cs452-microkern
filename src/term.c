#include <term.h>


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
