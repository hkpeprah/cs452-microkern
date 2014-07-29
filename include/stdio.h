#ifndef __STDIO__
#define __STDIO__
#include <types.h>
#include <ts7200.h>
#include <vargs.h>
#define BUF_LEN         80
#define VARG_BUF_LEN    256
#define IO              COM2
#define BS              9
#define LF              10
#define CR              13
#define EOF             -1  /* EOF is commonly -1 */

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
char *gets(int, char*, uint32_t);
int sscanf(const char*, const char*, ...);
void printformatted(int, char*, va_list);
int format(const char*, va_list, char*);
void bufputstr(int, char*);
void bufprintf(int, char*, ...);

#endif /* __STDIO__ */
