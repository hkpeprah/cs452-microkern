/*
 * null.c - the null task
 */
#include <term.h>
#include <syscall.h>

static volatile int SHELL_EXITED = 0;


void setExit(int term) {
    SHELL_EXITED = term;
}


void NullTask() {
    /* sits on the kernel passing */
    notice("NullTask: Entering.");
    while (SHELL_EXITED == 0);
    notice("NullTask: Exiting.");
    Exit();
}
