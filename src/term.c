#include <term.h>


extern int get_cpsr(int dummy);
extern int get_sp(int dummy);
extern int *push_reg();


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


void dumpRegisters() {
    #if DEBUG
        int i;
        int sp = (int)push_reg();
        debugf("Task CPSR: %x\n", get_cpsr(0));
        debugf("Task SP: %x\n", sp);
        for (i = 0; i < 15; ++i) {
            debugf("Register R%d: %x\n", i, ((int*)sp)[i]);
        }
    #endif
}
