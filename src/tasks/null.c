/*
 * null.c - the null task
 */
#include <term.h>
#include <syscall.h>

void NullTask() {
    /* sits on the kernel passing */
    notice("NullTask: Entering.");
    while (1);
    Exit();
}
