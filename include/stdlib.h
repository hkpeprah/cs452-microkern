#ifndef __STDLIB__
#define __STDLIB__
#include <types.h>
#include <syscall.h>
#include <kernel.h>

/* http://www.cocoawithlove.com/2008/04/using-pointers-to-recast-in-c-is-bad.html */
#define UNION_CAST(x, destType)  (((union {__typeof__(x) a; destType b;}) x).b)
#define toLowerCase(ch)          (ch < 'a' ? ch + 'a' - 'A' : ch)
#define toUpperCase(ch)          (ch >= 'a' ? ch - ('a' - 'A') : ch)
#define ABS(x)                   (x < 0 ? x * -1 : x)
#define MIN(x, y)                (x < y ? x : y)
#define MAX(x, y)                (x > y ? x : y)
#define EXTRACT_BIT(n, k)        (((n) & (1 << (k))) >> (k))
#define ASSERT_MSG               "\033[31mPanic: assert failed at line %d of file %s (function <%s>): %s\033[0m\r\n\r\n"
#define ASSERT(condition, msg)   {                                        \
        if ((condition) == false) {                                       \
            Panic(ASSERT_MSG, __LINE__, __FILE__, __FUNCTION__, (msg));   \
        }                                                                 \
    }
#define KASSERT(condition, msg)   {                                       \
        if ((condition) == false) {                                       \
            kernel_disable();                                             \
            kprintf(ASSERT_MSG, __LINE__, __FILE__, __FUNCTION__, (msg)); \
        }                                                                 \
    }

#if DEBUG
    #define d(p) ( *((typeof(p)) pointer_check(p, __LINE__, __FILE__, __FUNCTION__)) )
#else
    #define d(p) (*(p))
#endif

inline void *pointer_check(void *x, int line, char *file, const char *function);

void *memset(void*, int, unsigned int);
void *memcpy(void*, const void*, size_t);
void initUart(short, int, bool);
void flushUart(int);

#endif /* __STDLIB__ */
