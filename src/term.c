#include <term.h>
#include <string.h>
#define USER        "jobs"
#define PASSWORD    "steve"

extern int get_cpsr(int dummy);
extern int get_sp(int dummy);
extern int *push_reg();


void initDebug() {
    #if DEBUG
        unsigned int i;
        move_cursor(0, BOTTOM_HALF);
        /* print seperator */
        for (i = 0; i < TERMINAL_WIDTH; ++i) puts("=");
        /* finally enable debugging */
        move_cursor(0, TOP_HALF);
        printf("Debugging Enabled...");
        set_scroll(BOTTOM_HALF + 1, TERMINAL_HEIGHT);
        move_cursor(0, BOTTOM_HALF + 1);
    #endif
}


void debug(char *str) {
    #if DEBUG
        save_cursor();
        set_scroll(TOP_HALF, BOTTOM_HALF - 1);
        move_cursor(0, BOTTOM_HALF - 1);
        printf("\r\n%s", str);
        set_scroll(BOTTOM_HALF + 1, TERMINAL_HEIGHT);
        restore_cursor();
    #endif
}


int login(char *user, char *pass) {
    debug("Processing user login.");
    if (!(strcmp(user, USER) || strcmp(pass, PASSWORD))) {
        return 1;
    }
    return 0;
}


void dumpRegisters() {
    #if DEBUG
        int i;
        int sp = (int)push_reg();

        printf("task cpsr: %x\n", get_cpsr(0));
        printf("task sp: %x\n", sp);

        for(i = 0; i < 15; ++i) {
            printf("reg +%d: %x\n", i, ((int*)sp)[i]);
        }
    #endif
}
