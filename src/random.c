/*
 * random.c - Psuedorandom number generator
 * description: uses the mersenne twister algorithm
 */
#include <random.h>

#define SIZE  624


static long TWISTER[SIZE];
static int index;
static unsigned int SEED;


void seed(unsigned int s) {
    unsigned int i;
    unsigned int c;
    long *mt;

    SEED = s;
    index = 0;
    c = 1812433253;
    mt = TWISTER;

    mt[0] = s;
    for (i = 1; i < SIZE; ++i) {
        mt[i] = (c * (mt[i - 1] ^ (mt[i - 1] >> 30)) + i) & 0xFFFFFFFF;    /* lowest 32 bits of the xor product */
    }
}


long random() {
    long y;
    long *mt;
    unsigned int i;
    unsigned int cons1, cons2;

    mt = TWISTER;

    if (index == 0) {
        /* if index is 0, regenerate random numbers */
        for (i = 0; i < SIZE; ++i) {
            y = mt[i] & 0x80000000;                   /* bit 31 (32nd bit) of mt[i] */
            y += (mt[i + 1] % SIZE) & 0x7FFFFFFF;     /* bits 0-30 of mt[...] */
            mt[i] = mt[(i + 397) % SIZE] ^ (y >> 1);
            if (y % 2 != 0) {
                mt[i] ^= 0x9908B0DF;                  /* 2567483615 */
            }
        }
    }

    y = mt[index];
    cons1 = 0x9D2C5680;
    cons2 = 0xEFC60000;
    y ^= (y >> 11);              /* xor y shifted right 11 bits */
    y ^= ((y << 7) & cons1);     /* xor y shifted left 7 bits and 2636928640 */
    y ^= ((y << 15) & cons2);    /* xor y shifted left 15 bits and 4022730752 */
    y ^= (y >> 18);              /* xor y shifted right 18 bits */

    index = (index + 1) % SIZE;
    return y;
}


long h_random() {

    return 0;
}
