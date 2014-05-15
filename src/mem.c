/*
 * mem.c - Memory functions
 */
#include <mem.h>


void * memcpy(void * src, void * dst, size_t num) {
    size_t i;
    char * source = (char*)src;
    char * dest = (char*)dst;

    i = 0;
    while ((*source++ = *dest++) && i++ < num);
}



void * getMem () {
    return getMemory();
}


void * getMemory () {
    MemBlock * blk;

    if (Memory.fp > 0) {
        blk = Memory->freed[Memory->fp--];
        blk->sp = 0;
    } else {
        blk = &(Memory->blocks[Memory->id++]);
        blk->sp = 0;
    }

    return (void*)blk->addrspace;
}


void free(unsigned int i) {
    Meory->freed[Memory->fp++] = &(Memory->blocks[i]);
}
