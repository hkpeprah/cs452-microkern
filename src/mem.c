/*
 * mem.c - Memory functions
 */
#include <mem.h>
#include <types.h>

static Memory __stack;
static Memory * Stack = &__stack;


void * memcpy(void * dst, const void * src, size_t num) {
    size_t i;
    char * source = (char*)src;
    char * dest = (char*)dst;

    i = 0;
    while ((*dest++ = *source++) && i++ < num);

    return dst;
}



void * getMem () {
    return getMemory();
}


void * getMemory () {
    MemBlock * blk;

    if (Stack->fp > 0) {
        blk = Stack->freed[Stack->fp--];
        blk->sp = 0;
    } else {
        blk = &(Stack->blocks[Stack->id++]);
        blk->sp = 0;
    }

    return (void*)blk->addrspace;
}


void free(unsigned int i) {
    Stack->freed[Stack->fp++] = &(Stack->blocks[i]);
}
