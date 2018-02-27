#ifndef QUEUESET_ARC
#define QUEUESET_ARC

#include "Var.h"
#include "Constraint.h"

#define QTYPE struct Arc
#define QTYPENAME arc
struct Arc {
        struct Var * var;
        struct Constraint * constraint;
};

#define EQ_FUN eq_arc
static inline
unsigned long eq_arc(struct Arc * a, struct Arc * b)
{
        return a->var == b->var && a->constraint == b->constraint;
}
#define HASH_FUN hash_arc
static inline
unsigned long hash_arc(struct Arc * x)
{
        unsigned long hash = (unsigned long)(x->var);
        hash = (unsigned long)(x->constraint) + (hash << 6) + (hash << 16) - hash;
        return hash ^ (hash >> 4);
}
#include "QueueSet.h"

#undef QTYPE
#undef QTYPENAME
#undef EQ_FUN
#undef HASH_FUN
#endif
