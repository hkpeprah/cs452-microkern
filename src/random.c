/*
 * random.c - Psuedorandom number generator
 * description: uses the mersenne twister algorithm
 */
#include <random.h>
#include <term.h>
#include <stdlib.h>

#define SIZE  624


static long TWISTER[SIZE];
static int index;
static unsigned int SEED;
static unsigned TRULY_RANDOM_SEED;


void seed(unsigned int s) {
    unsigned int i;
    unsigned int c;
    long *mt;

    SEED = s;
    TRULY_RANDOM_SEED = ABS(*((uint32_t*)0x80810060));
    index = 0;
    c = 1812433253;
    mt = TWISTER;

    mt[0] = s;
    for (i = 1; i < SIZE; ++i) {
        mt[i] = (c * (mt[i - 1] ^ (mt[i - 1] >> 30)) + i) & 0xFFFFFFFF;    /* lowest 32 bits of the xor product */
    }
}


long random_seed() {
    unsigned int seed;
    seed = TRULY_RANDOM_SEED;
    TRULY_RANDOM_SEED = ABS(*((uint32_t*)0x80810060));
    return seed;
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


long random_range(unsigned int lower_bound, unsigned int upper_bound) {
    long rand;

    rand = random() % (upper_bound - lower_bound + 1);
    rand += lower_bound;
    return rand;
}
