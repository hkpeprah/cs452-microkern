/*
 * stdio.c - Standard I/O functions
 */
#include <stdio.h>
#include <string.h>
#include <ts7200.h>
#include <types.h>
#include <vargs.h>
#include <bwio.h>
#define __INT             1
#define __HEX             2
#define __STRING          3
#define __UINT            4
#define __CHAR            5
#define __LONG            6


void initIO() {
    bwsetspeed(IO, 115200);
    bwsetspeed(COM1, 2400);
    bwsetfifo(IO, OFF);
    bwsetfifo(COM1, OFF);
}


int atod(const char ch) {
    /*
     * returns the integer value of the digit
     */
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    } else if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
	} else if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
	return -1;
}


int ctod(const char ch) {
    /*
     * unlike atod, only accepts numbers
     * returns the integer value of the digit
     */
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    return -1;
}


int atoi(const char *source, int *status) {
    /*
     * parses a c-string and interprets the contents as integer
     */
    unsigned int base = 10, num = 0, i = 0;
    int digit;
    *status = 1;
    while(source[i]) {
        digit = atod(source[i]);
        if (digit > base || digit < 0) {
            *status = 0;
            break;
        }
        num = num * base + digit;
        ++i;
    }

    return num;
}


int atoin(const char *source, int *status) {
    /*
     * like atoi but only accepts digits
     */
    unsigned int base = 10, num = 0, i = 0;
    int digit;
    *status = 1;
    while(source[i]) {
        digit = ctod(source[i]);
        if (digit > base || digit < 0) {
            *status = 0;
            break;
        }
        num = num * base + digit;
        ++i;
    }

    return num;
}


int scanformatted(const char *input, const char *format, va_list va) {
    char ch;
    int conv;
    char *tmp;
    unsigned int i;
    unsigned int base;
    unsigned int conversion_type;
    unsigned int nread;
    unsigned int nassigned;
    unsigned int len = strlen(input);
    char buffer[len];
    char *buf = buffer;

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
            conversion_type = __CHAR;
            break;
        case 'l':
            conversion_type = __LONG;
            break;
        default:
            return -1;
        }

        i = 0;
        while (isspace(*input)) {
            ++input;
            --len;
        }

        if (*input == 0) {
            return -1;
        } else if (conversion_type == __CHAR) {
            buf[0] = *input++;
            --len;
        } else {
            for (i = 0; !isspace(*input) && ((buf[i] = *input++)); ++i) --len;
            buf[i] = 0;
        }

        switch(conversion_type) {
            case __INT:
                *va_arg(va, int*) = atoin(buf, &conv);
                if (conv == 0) return -1;
                break;
            case __CHAR:
                *va_arg(va, char*) = buf[0];
                break;
            case __UINT:
                *va_arg(va, unsigned int*) = (unsigned int)atoin(buf, &conv);
                if (conv == 0) return -1;
                break;
            case __HEX:
                *va_arg(va, unsigned int*) = (unsigned int)atoi(buf, &conv);
                if (conv == 0) return -1;
                break;
            case __STRING:
                tmp = va_arg(va, char*);
                while ((*tmp++ = *buf++));
                break;
        }
        nassigned++;
    }

    while(isspace(*input)) {
        input++;
        --len;
    }

    if (len != 0) return -1;
    return nassigned;
}


int sscanf(const char *src, const char *fmt, ...) {
    va_list va;
    int retval;

    va_start(va, fmt);
    retval = scanformatted(src, fmt, va);
    va_end(va);

    return retval;
}
