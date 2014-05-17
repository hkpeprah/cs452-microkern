#include <bwio.h>
#include "mem.h"
#include "task.h"
#include "utasks.h"

#define SWI_HANDLER_ADDR 0x28

extern int swi_enter(int sp, void* tf);
extern int swi_handler();
extern int swi_exit(int result, int sp, void** tf);

extern int get_cpsr();
extern int get_spsr();

int main() {
    bwsetspeed(COM1, 2400);
    bwsetfifo( COM1, OFF );
    bwsetfifo( COM2, OFF );

    initMem();
    initTasks();

    bwprintf(COM2, "cpsr: %x, spsr: %x", get_cpsr(), get_spsr());

    bwprintf(COM2, "c0\n");

    // swi handler address -> 0x28
    uint32_t *swiHandlerAddr = (uint32_t*) SWI_HANDLER_ADDR;
    *swiHandlerAddr = (uint32_t) swi_handler;

    bwprintf(COM2, "c1\n");

    task_t *task = createTaskD(1);

    bwprintf(COM2, "c2\n");

    uint32_t originalSP = task->sp;
    uint32_t *sp = (uint32_t*) task->sp;
    *--sp = (uint32_t) firstTask;
    *--sp = originalSP;

    int i;
    for(i = 0; i < 9; ++i) {
        *--sp = 0;
    }

    bwprintf(COM2, "c3\n");

    task->sp = (uint32_t) sp;

    bwprintf(COM2, "task initialized, sp: %x\n", sp);
    for(i = 0; i < 11; ++i) {
        bwprintf(COM2, "    stack at ptr+%d: %x\n", i, sp[i]);
    }
    bwprintf(COM2, "task location: %x\n", firstTask);

    int ret = swi_exit(0, task->sp, 0);

    bwprintf(COM2, "back to stupid main, returned value: %x", ret);

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
