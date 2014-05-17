#ifndef __STDIO__
#define __STDIO__
#include <types.h>

#define BUF_LEN      80

typedef struct {
    uint32_t oFlags;
    uint32_t iFlags;
    uint32_t iMask;
    uint32_t oMask;
    uint32_t addr;
    char buffer[BUF_LEN];
} IOController;


void initIO();
int atod(const char);
int ctod(const char);
int atoi(const char*, int*);
int atoin(const char*, int*);
int sscanf(const char*, const char*, ...);

#endif /* __STDIO__ */
