/*
 * stdio.c - Standard I/O functions
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>
#include <assert.h>
#define __INT             1
#define __HEX             2
#define __STR             3
#define __UINT            4
#define __CHAR            5
#define __LONG            6


static int atod(const char ch) {
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


static int ctod(const char ch) {
    /*
     * unlike atod, only accepts numbers
     * returns the integer value of the digit
     */
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    return -1;
}


static int atoi_local(const char *source, int *status) {
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


static int atoin(const char *source, int *status) {
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


static void uitoa_local(unsigned int num, unsigned int base, char *buf) {
    /*
     * converts an unsigned integer into the string representation
     * stores result into buf
     */
    int n = 0;
    int digit;
    int d = 1;

    while ((num / d) >= base) {
        d *= base; /* Extract powers of 10 for digits */
    }

    while (d != 0) {
        digit = num / d;  /* extract digit */
        num %= d;         /* extract remainder */
        d = d / base;

        if (n || digit > 0 || d == 0) {
            *buf++ = digit + (digit < 10 ? '0' : 'a' - 10);
            ++n;
        }
    }
    *buf = '\0';
}


static void itoa(int num, char *buf) {
    /*
     * converts an integer into the string representation
     * stores result into buf
     */
    if (num < 0) {
        num = -num;
        *buf++ = '-';
    }
    uitoa_local((unsigned int)num, 10, buf);
}


static int sscanformatted(const char *input, const char *fmt, va_list va) {
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
        if (!(ch = *fmt++)) {
            break;
        } else if (len <= 0) {
            return -1;
        } else if (isspace(ch)) {
            while (*fmt && isspace(*fmt)) {
                fmt++;
            }
            continue;
        }

        if (ch != '%') {
            if (*input != ch || len <= 0) {
                return -1;
            }
            input++;
            nread++;
            len--;
            continue;
        }

        nread++;
        ch = *fmt++;

        switch (ch) {
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
                conversion_type = __STR;
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

        while (isspace(*input)) {
            ++input;
            --len;
        }

        if (*input == 0) {
            return -1;
        } else if (conversion_type == __CHAR) {
            buf[0] = *input++;
            buf[1] = 0;
            --len;
        } else {
            for (i = 0; !isspace(*input) && ((buf[i] = *input++)); ++i) {
                --len;
            }
            buf[i] = 0;
        }

        switch (conversion_type) {
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
                *va_arg(va, unsigned int*) = (unsigned int)atoi_local(buf, &conv);
                if (conv == 0) return -1;
                break;
            case __STR:
                tmp = va_arg(va, char*);
                while ((*tmp++ = *buf++));
                break;
        }
        nassigned++;
    }

    while (*input && isspace(*input)) {
        input++;
        --len;
    }

    if (len != 0) {
        return -1;
    }
    return nassigned;
}


static int scan(const char *src, const char *fmt, ...) {
    va_list va;
    int retval;

    va_start(va, fmt);
    retval = sscanformatted(src, fmt, va);
    va_end(va);

    return retval;
}


int main() {
    /* scanf testing */
    char buf[80];
    int args[4];
    int status;

    status = scan("setrv 50", "setrv %d", &args[0], &args[1]);
    assert(status != -1 && args[0] == 50);
    status = scan("whoami", "whoami");
    assert(status != -1);
    status = scan("rasputin", "doug");
    assert(status == -1);

    return 0;
}
