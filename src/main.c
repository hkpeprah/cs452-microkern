#include <utasks.h>
#include <task.h>
#include <mem.h>
#include <term.h>
#include <stdio.h>
#include <syscall.h>

#define INIT_SPSR          0x13c0
#define SWI_HANDLER_ADDR   0x28
#define FOREVER            while(1)
#define FIRST_PRIORITY     5

extern int swi_enter(int sp, void* tf);
extern int swi_handler();
extern int swi_exit(int result, int sp, void** tf);
extern int get_cpsr();
extern int get_spsr();


static void boot() {
    initIO();
    initMem();
    initTasks();
    clear_screen();
    initDebug();
    puts("Windows ME booting up.");
    newline();
}


int main() {
    int status;
    uint32_t tid;
    task_t *task;
    uint32_t *swiHandlerAddr;

    boot();

    /* create the first user task */
    status = sys_create(FIRST_PRIORITY, firstTask, &tid);
    if (status != 0) {
        /* something went wrong creating the first user task */
        return -1;
    }

    task = schedule();

    // swi handler address -> 0x28
    swiHandlerAddr = (uint32_t*)SWI_HANDLER_ADDR;
    *swiHandlerAddr = (uint32_t)swi_handler;
    status = swi_exit(0, task->sp, 0);

    puts("Exiting...\n");
    return 0;
}


void testMem() {
    initMem();
    MemBlock_t *blocks[10];

    bwprintf(COM2, "allocating\n");

    int i;
    for(i = 0; i < 10; ++i) {
        blocks[i] = getMem();
        bwprintf(COM2, "%x\n", blocks[i]->addr);
    }

    bwprintf(COM2, "Freeing...\n");

    for(i = 0; i < 10; ++i) {
        freeMem(blocks[i]);
    }

    bwprintf(COM2, "allocating again...\n");

    for(i = 0; i < 10; ++i) {
        blocks[i] = getMem();
        bwprintf(COM2, "%x\n", blocks[i]->addr);
    }
}
