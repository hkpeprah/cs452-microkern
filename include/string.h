#ifndef __STRING__
#define __STRING__
#include <types.h>

#define TAB               0x09
#define NEWLINE           0x0A
#define SPACE             0x20
#define CARRIAGE          0x0D
#define VERTICAL_TAB      0x0B
#define FEED              0x0C


int isspace(int);
size_t strlen(const char*str);
char * strcpy(char *dest, const char *src);
char * strncpy(char *dest, const char *src, size_t num);
int strcmp(const char *str1, const char *str2);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t num);

#endif /* __STRING__ */
