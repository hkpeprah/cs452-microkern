#ifndef __STRING__
#define __STRING__

#define TAB               0x09
#define NEWLINE           0x0A
#define SPACE             0x20
#define CARRIAGE          0x0D
#define VERTICAL_TAB      0x0B
#define FEED              0x0C


int atoi(const char*, int*);
int atoin(const char*, int*);
int isspace(int);
int sscanf(const char*, const char*, ...);
size_t strlen(const char*);
char * strcpy(char*, const char*);
char * strncpy(char*, const char*, size_t);
int strcmp(const char*, const char*);

#endif /* __STRING__ */
