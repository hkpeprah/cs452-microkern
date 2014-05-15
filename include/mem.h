#ifndef __MEM__
#define __MEM__
#define MEM_BLOCK_SIZE       1024
#define BLOCK_COUNT          24


typedef struct {
    unsigned int sp;
    char * addrspace[MEM_BLOCK_SIZE];
} MemBlock;


typedef struct {
    unsigned int id = 0;
    unsigned int fp = 0;
    MemBlock blocks[BLOCK_COUNT];
    MemBlock * freed[BLOCK_COUNT];
} Memory;


/* global addrspace stack */
static Memory __stack;
static Memory * Stack = &__stack;

void* memcpy(void*, void*);
void* getMem();
void* getMemory();
void free(unsigned int);

#endif /* __MEM__ */
