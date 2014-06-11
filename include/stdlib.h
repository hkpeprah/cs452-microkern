#ifndef __STDLIB__
#define __STDLIB__
#include <types.h>

/* http://www.cocoawithlove.com/2008/04/using-pointers-to-recast-in-c-is-bad.html */
#define UNION_CAST(x, destType)  (((union {__typeof__(x) a; destType b;}) x).b)


void *memcpy(void*, const void*, size_t);
void initUart(short, int, bool);

#endif /* __STDLIB__ */
