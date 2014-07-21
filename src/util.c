#include <util.h>
#include <stdlib.h>
#include <term.h>
#include <logger.h>


inline uint32_t min3(uint32_t a, uint32_t b, uint32_t c) {
    a = (b < a) ? b : a;
    a = (c < a) ? c : a;
    return a;
}
