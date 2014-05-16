#ifndef __MEM__
#define __MEM__
#include <types.h>
#define MEM_BLOCK_SIZE       1024
#define BLOCK_COUNT          24


typedef struct {
    unsigned int sp;
    char addrspace[MEM_BLOCK_SIZE];
} MemBlock;


typedef struct {
    unsigned int id;
    unsigned int fp;
    MemBlock blocks[BLOCK_COUNT];
    MemBlock * freed[BLOCK_COUNT];
} Memory;


void* memcpy(void*, const void*, size_t);
void* getMem();
void* getMemory();
void free(unsigned int);

#endif /* __MEM__ */
