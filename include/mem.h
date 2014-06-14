#ifndef __MEM__
#define __MEM__
#include <types.h>

// we can probably play around with these values
#define MEM_BLOCK_SIZE       0x20000
#define BLOCK_COUNT          64
#define TOP_OF_STACK         0x00230000

typedef struct MemBlock {
    uint32_t addr;
    struct MemBlock *next;
} MemBlock_t;

void initMem();
MemBlock_t* getMem();
void freeMem(MemBlock_t* block);

#endif /* __MEM__ */
