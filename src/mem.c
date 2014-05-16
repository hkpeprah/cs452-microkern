/*
 * mem.c - Memory functions
 */
#include <mem.h>
#include <types.h>

// we can probably play around with these values
#define MEM_BLOCK_SIZE       0x10000
#define BLOCK_COUNT          64
#define TOP_OF_STACK         0x00220000

static MemBlock_t memBlocks[BLOCK_COUNT];
static MemBlock_t *freeHead;

void initMem() {
    uint32_t currentAddr = TOP_OF_STACK;
    MemBlock_t *lastBlock = NULL;
    int i;

    // set each address pointer of the blocks, and link them to each other
    for(i = 0; i < BLOCK_COUNT; ++i) {
        currentAddr += MEM_BLOCK_SIZE;

        memBlocks[i].addr = currentAddr;
        memBlocks[i].next = lastBlock;

        lastBlock = &memBlocks[i];
    }

    freeHead = lastBlock;
}

MemBlock_t* getMem () {
    if(freeHead == NULL) {
        return NULL;
    }

    MemBlock_t *block = freeHead;
    freeHead = freeHead->next;

    block->next = NULL;
    return block;
}

void freeMem(MemBlock_t* block) {
    block->next = freeHead;
    freeHead = block;
}
