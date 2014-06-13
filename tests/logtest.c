#include <term.h>
#include <util.h>
#include <types.h>
#include <kernel.h>
#include <interrupt.h>
#include <uart.h>
#include <server.h>
#include <ts7200.h>
#include <syscall.h>
#include <k_syscall.h>
#include <logger.h>


int main() {
    boot();
    initLogger();

    log_f("hello world!");
    log_f("i am the best %d", 55);
    printLogger(0);

    return 0;
}
