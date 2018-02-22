#ifndef VAR_H
#define VAR_H
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
////////
// Bitset
////////
typedef uint_fast32_t bitset;
#define DOMAIN_SIZE 31

#define REDBIT (1)
#define BLUEBIT (0)
#define RED (1<<REDBIT)
#define BLUE (1<<BLUEBIT)
#define HAS_RED(x) ((x) & 1<<REDBIT)
#define HAS_BLUE(x) ((x) & 1<<BLUEBIT)

static inline void bitset_print(bitset bits)
{
        for (unsigned i = 0; i < DOMAIN_SIZE; i++) {
                        printf("%i", !!(bits & (1<<i)));
        }
        printf("|");
        for (unsigned i = 0; i < DOMAIN_SIZE; i++) {
                if (bits & (1<<i)) {
                        printf("%i ", i);
                }
        }
        printf("\n");
}

////////
// Generic Variable
////////
struct Var {
        unsigned id;
        unsigned N;
        bitset domain;
};


static inline void Var_create(struct Var * v, unsigned id, unsigned N, bitset domain)
{
        v->id = id;
        v->N = N;
        v->domain = domain;
}
static inline void Var_set(struct Var * v, bitset domain)
{
        v->domain = domain;
}

#endif // VAR_H
