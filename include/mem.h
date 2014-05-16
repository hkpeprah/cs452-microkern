#ifndef __MEM__
#define __MEM__
#include <types.h>

typedef struct MemBlock {
    uint32_t addr;
    struct MemBlock *next;
} MemBlock_t;

void initMem();
MemBlock_t* getMem();
void freeMem(MemBlock_t* block);

#endif /* __MEM__ */
