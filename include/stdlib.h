#ifndef __STDLIB__
#define __STDLIB__
#include <types.h>

/* http://www.cocoawithlove.com/2008/04/using-pointers-to-recast-in-c-is-bad.html */
#define UNION_CAST(x, destType)  (((union {__typeof__(x) a; destType b;}) x).b)
#define toLowerCase(ch)          (ch < 'a' ? ch + 'a' - 'A' : ch)
#define toUpperCase(ch)          (ch >= 'a' ? ch - ('a' - 'A') : ch)

#define MIN(x, y)            (x < y ? x : y)
#define MAX(x, y)            (x > y ? x : y)
#define EXTRACT_BIT(n, k)    (((n) & (1 << (k))) >> (k))

void *memcpy(void*, const void*, size_t);
void initUart(short, int, bool);
void flushUart(int);

#endif /* __STDLIB__ */
