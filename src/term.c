#include <term.h>

void initDebug() {
    #if DEBUG
        unsigned int i;
        move_cursor(0, BOTTOM_HALF);
        /* print seperator */
        for (i = 0; i < TERMINAL_WIDTH; ++i) puts("=");
        move_cursor(0, TOP_HALF);
        printf("Debugging Enabled..");
        set_scroll(BOTTOM_HALF + 1, TERMINAL_HEIGHT);
        move_cursor(0, BOTTOM_HALF + 1);
        save_cursor();
    #else
        set_scroll(0, TERMINAL_HEIGHT);
    #endif
}
