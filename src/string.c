/*
 * string.c - String functions
 */
#include <vargs.h>
#include <types.h>
#include <string.h>


size_t strlen(const char * str) {
    size_t len;
    for (len = 0; str[len] && str[len] != '\0'; ++len);
    return len;
}


char * strcpy(char * dest, const char * src) {
    unsigned int i;
    while ((dest[i] = src[i]) && i++);
    return dest;
}


char * strncpy(char * dest, const char * src, size_t num) {
    size_t i;
    i = 0;
    while (i < num && (dest[i] = src[i]) && ++i);

    if (i < num) {
        while (i <= num) {
            dest[i] = '\0';
            ++i;
        }
    }

    dest[i] = '\0';
    return dest;
}


int strcmp(const char * str1, const char * str2) {
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


int scanformatted(const char *input, const char *format, va_list va) {
    char ch;
    unsigned int i;
    unsigned int base;
    unsigned int conversion_type;
    unsigned int nread;
    unsigned int nassigned;
    unsigned int len = strlen(input);
    char buffer[len];
    char * buf = buffer;

    nread = 0;
    nassigned = 0;

    while (true) {
        if (!(ch = *format++)) {
            break;
        } else if (isspace(ch)) {
            while (isspace(*input) && len-- > 0) ++nread;
            continue;
        } else if (len <= 0) {
            return -1;
        }

        if (ch != '%') {
            if (*input != ch || len <= 0) {
                return -1;
            }
            len--;
            input++;
            nread++;
            continue;
        }

        nread++;
        ch = *format++;
        switch(ch) {
        case 'd':
            conversion_type = __INT;
            base = 10;
            break;
        case 'X':
        case 'x':
        case 'p':
            conversion_type = __HEX;
            base = 16;
            break;
        case 's':
            conversion_type = __STRING;
            break;
        case 'i':
            conversion_type = __INT;
            base = 10;
            break;
        case 'u':
            conversion_type = __UINT;
            base = 10;
            break;
        case 'c':
            converstion_type = __CHAR;
            break;
        case 'l':
            conversion_type = __LONG;
            break;
        default:
            return -1;
        }
    }

    return nassigned;
}


int sscanf(const char *src, const char *fmt, ...) {
    va_list va;
    int retval;

    va_start(va, format);
    retval = scanformatted(src, fmt, va);
    va_end(va);

    return retval;
}
