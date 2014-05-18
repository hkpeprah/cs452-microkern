#include <term.h>


void initDebug() {
    #if DEBUG
        unsigned int i;
        move_cursor(0, BOTTOM_HALF);
        /* print seperator */
        for (i = 0; i < TERMINAL_WIDTH; ++i) puts("=");
        /* finally enable debugging */
        move_cursor(0, TOP_HALF);
        puts("Debugging Enabled...");
        set_scroll(BOTTOM_HALF + 1, TERMINAL_HEIGHT);
        move_cursor(0, BOTTOM_HALF + 1);
    #endif
}


void debug (const char *str) {
    #if DEBUG
        save_cursor();
        set_scroll(TOP_HALF, BOTTOM_HALF - 1);
        move_cursor(0, TOP_HALF);
        newline();
        puts(str);
        set_scroll(BOTTOM_HALF + 1, TERMINAL_HEIGHT);
        restore_cursor();
    #endif
}

void dumpRegisters() {
#if DEBUG
    int sp = (int)push_reg();

    printf("task cpsr: %x\n", get_cpsr(0));
    printf("task sp: %x\n", sp);

    int i;
    for(i = 0; i < 15; ++i) {
        printf("reg +%d: %x\n", i, ((int*)sp)[i]);
    }
#endif
}
