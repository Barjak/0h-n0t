#ifndef QUEUESET_VOID_PTR_H
#define QUEUESET_VOID_PTR_H

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define QTYPE void *
#define QTYPENAME void_ptr
// #define EQ_ARC(x,y) (((x).var == (y).var) && (x).constraint == (y).constraint)
#define EQ_FUN eq_void_ptr
static inline
unsigned long eq_void_ptr(void ** a, void ** b)
{
        return *a == *b;
}
#define HASH_FUN hash_void_ptr
static inline
unsigned long hash_void_ptr(void ** x)
{
        void * ptr = (*x);
        unsigned long hash = (unsigned long)ptr;
        hash = (75773 * hash) ^ ((hash * 35279) >> 8);
        return hash;
}
#include "QueueSet.h"

#undef QTYPE
#undef QTYPENAME
#undef EQ_FUN
#undef HASH_FUN
#endif
