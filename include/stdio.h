#ifndef __STDIO__
#define __STDIO__
#include <types.h>
#include <ts7200.h>
#include <vargs.h>
#define BUF_LEN      80
#define IO           COM2


typedef struct {
    uint32_t oFlags;
    uint32_t iFlags;
    uint32_t iMask;
    uint32_t oMask;
    uint32_t addr;
    char buffer[BUF_LEN];
} IOController;


int atod(const char);
int ctod(const char);
int atoi(const char*, int*);
int atoin(const char*, int*);
void itoa(int, char*);
void uitoa(unsigned int, unsigned int, char*);
int sscanf(const char*, const char*, ...);
int format(const char*, va_list, char*);
void bufputstr(int, char*);
void bufprintf(int, char*, ...);

#endif /* __STDIO__ */
