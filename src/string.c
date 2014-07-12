/*
 * string.c - String functions
 */
#include <vargs.h>
#include <types.h>
#include <string.h>


size_t strlen(const char *str) {
    size_t len;
    for (len = 0; str[len] && str[len] != '\0'; ++len);
    return len;
}


char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}


char *strncpy(char *dest, const char *src, size_t num) {
    char *d;
    size_t i;

    i = 0;
    d = dest;
    while (i < num) {
        *d++ = *src++;
        ++i;
    }

    *d = '\0';
    return dest;
}


char *strcat(char *dest, const char *src) {
    char *d;

    d = dest + strlen(dest);
    while ((*d++ = *src++));

    return dest;
}


char *strncat(char *dest, const char *src, size_t num) {
    char *d;
    size_t i;

    i = 0;
    d = dest + strlen(dest);

    while (i < num) {
        *d++ = *src++;
        ++i;
    }

    *d = '\0';
    return dest;
}


int strcmp(const char *str1, const char *str2) {
    unsigned int i = 0;
    while (str1[i] && str2[i]) {
        if (str1[i] != str2[i]) {
            return (int)(str1[i] - str2[i]);
        }
        ++i;
    }

    if (strlen(str1) - i > 0) {
        return (int)str1[i];
    } else if (strlen(str2) - i > 0) {
        return (int)str2[i];
    }
    return 0;
}


int isspace(int ch) {
    switch (ch) {
    case TAB:
    case NEWLINE:
    case SPACE:
    case CARRIAGE:
    case VERTICAL_TAB:
    case FEED:
        return 1;
    }

    return 0;
}
