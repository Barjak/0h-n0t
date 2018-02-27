#ifndef CONSTRAINT_H
#define CONSTRAINT_H

#include "Var.h"
#include "CSError.h"

/*
 * struct Constraint is a tagged union whose tag is the function pointer
 * unsigned (*filter)(struct Constraint*, bitset*)
 *
*/

////////
// Constraint Structures
////////
struct ConstraintSum {
        bitset domain;
};

struct ConstraintVisibility {
        unsigned n_lhs_variables;
};

struct Constraint {
        unsigned id;
        unsigned n_vars;
        struct Var ** var;
        bitset * cached_domains;
        unsigned (*filter)(struct Constraint*, bitset*);
        union {
                struct ConstraintVisibility visibility_data;
                struct ConstraintSum sum_data;
        };
};

// ConstraintSum

/* Following Trick 2003
 */
static inline CSError ConstraintSum_filter(struct Constraint * c, bitset * modified_domains)
{
        unsigned fail = 0;
        unsigned N = c->n_vars;

        bitset modified = 0;
        struct ConstraintSum * cs = &c->sum_data;
        bitset * f = malloc((N+1) * sizeof(bitset));
        if (!f) {
                fail = FAIL_ALLOC;
                goto bad_alloc1;
        }
        bitset * g = malloc((N+1) * sizeof(bitset));
        if (!g) {
                fail = FAIL_ALLOC;
                goto bad_alloc2;
        }

        for (unsigned i = 1; i <= N; i++) {
                f[i] = 0;
                g[i] = 0;
        }
        f[0] = 1<<0;
        for (unsigned i = 1; i <= N; i++) {
                for (unsigned b = 0; b < DOMAIN_SIZE; b++) {
                        if (f[i-1] & (1 << b)) {
                                f[i] |= c->var[i-1]->domain << b;
                        }
                }
        }

        g[N] = f[N] & cs->domain;
        fail |= !g[N];
        if (fail) {
                goto cleanup;
        }

        for (int i = N-1; i >= 0; i--) {
                for (unsigned b = 0; b < DOMAIN_SIZE; b++) {
                        // The expression
                        // !!((var_domain >> b) & g[i+1])
                        //     is 1 iff
                        //     there exists a v in var_domain
                        //              and a u in g[i+1]      s.t.: v + b = u
                        // The expression
                        // g[i] |= {1,0} << b
                        //     writes the result to the appropriate bit
                        g[i] |= !!((c->var[i]->domain << b) & g[i+1]) << b;
                }
                g[i] &= f[i];
        }
        for (unsigned i = 0; i < N; i++) {
                c->cached_domains[i] = 0;
                for (unsigned b = 0; b < DOMAIN_SIZE; b++) {
                        c->cached_domains[i] |= ((c->var[i]->domain & 1<<b)
                                                   && ((f[i] << b) & g[i+1])) << b;
                        modified |= (c->cached_domains[i] != c->var[i]->domain) << i;
                }
        }
cleanup:
bad_alloc2:
        free(g);
bad_alloc1:
        free(f);
        if (modified_domains) {
                *modified_domains = modified;
        }
        return fail;
}

static inline CSError ConstraintSum_create(struct Constraint * c,
                                          bitset domain,
                                          struct Var ** addends,
                                          unsigned n_addends)
{
        c->filter = ConstraintSum_filter;
        c->n_vars = n_addends;
        c->var = malloc(c->n_vars * sizeof(struct Var*));
        if (!c->var) { goto bad_alloc1; }
        c->cached_domains = malloc(c->n_vars * sizeof(bitset));
        if (!c->cached_domains) { goto bad_alloc2; }

        for (unsigned i = 0; i < c->n_vars; i++) {
                c->var[i] = addends[i];
                c->cached_domains[i] = addends[i]->domain;
        }

        c->sum_data.domain = domain;
        return NO_FAILURE;

bad_alloc2:
        free(c->var);
bad_alloc1:
        return FAIL_ALLOC;
}

static inline CSError ConstraintSum_destroy(struct Constraint * c)
{
        if (!c->var || !c->cached_domains) { goto fail; }
        free(c->var);
        free(c->cached_domains);
        return NO_FAILURE;
fail:
        printf("Constraint not initialized.\n");
        return FAIL_ALLOC;
}

// ConstraintVisibility

// There's probably a one liner that does this.
static inline CSError ConstraintVisibility_filter(struct Constraint * c, bitset * modified_domains)
{
        unsigned n_lhs = c->n_vars - 1; // Cardinality var
        unsigned rhs_i = c->n_vars - 1; // Index var
        bitset modified = 0;
        // Load domains
        bitset R = 0;
        bitset B = 0;
        bitset y = 0;
        c->cached_domains[rhs_i] = c->var[rhs_i]->domain;
        y = c->cached_domains[rhs_i];
        for (unsigned b = 0; b < n_lhs; b++) {
                c->cached_domains[b] = c->var[b]->domain;
                unsigned isred =  !!HAS_RED(c->cached_domains[b]);
                R |= isred << b;
                unsigned isblue = !!HAS_BLUE(c->cached_domains[b]);
                B |= isblue << b;
        }

        unsigned blue_fixed = (B & ~R);
        unsigned red_fixed = (R & ~B);
        unsigned maxx = 0,
                 maxy = 0,
                 miny = 0;
        for (unsigned b = 0; b < n_lhs; b++) {
                if (!maxx && red_fixed & 1<<b) { maxx = 1<<b; }
                if (y & 1<<b) { maxy = 1<<b; }
                if (!miny && (y & 1<<b)) { miny = 1<<b; }
        }
        y = y & ~blue_fixed;
        y = y & ((maxx<<1)-1);
        R = R & ~(miny-1);
        if (maxy == miny) {
                B = B & ~y;
        }

        for (unsigned b = 0; b < n_lhs; b++) {
                c->cached_domains[b] = (!!(B & 1<<b) << BLUEBIT) | (!!(R & 1<<b) << REDBIT);
                // printf("%lu %u\n", c->cached_domains[b], (unsigned)((!(R & 1<<b) << REDBIT)));
                modified |= (c->cached_domains[b] != c->var[b]->domain) << b;
        }
        c->cached_domains[rhs_i] = y;

        modified |= (c->cached_domains[rhs_i] != c->var[rhs_i]->domain) << rhs_i;

        if (modified_domains) {
                *modified_domains = modified;
        }

        return NO_FAILURE;
}

static inline CSError ConstraintVisibility_create(struct Constraint * c,
                                                 struct Var ** lhsvars,
                                                 unsigned n_lhsvars,
                                                 struct Var *rhsvar)
{
        c->filter = ConstraintVisibility_filter;
        c->n_vars = n_lhsvars + 1;
        c->var = malloc(c->n_vars * sizeof(struct Var*));
        if (!c->var) { goto bad_alloc1; }
        c->cached_domains = malloc(c->n_vars * sizeof(bitset));
        if (!c->cached_domains) { goto bad_alloc2; }

        unsigned i;
        for (i = 0; i < n_lhsvars; i++) {
                c->var[i] = lhsvars[i];
                c->cached_domains[i] = lhsvars[i]->domain;
        }
        c->var[i] = rhsvar;

        return NO_FAILURE;
bad_alloc2:
        free(c->var);
bad_alloc1:
        return FAIL_ALLOC;
}

static inline CSError ConstraintVisibility_destroy(struct Constraint * c)
{
        if (!c->var || !c->cached_domains) { goto fail; }
        free(c->var);
        free(c->cached_domains);
        return NO_FAILURE;
fail:
        return FAIL_ALLOC;
}

////////
// Generic Constraint Functions
////////
static inline unsigned Constraint_filter(struct Constraint * c, bitset * modified_domains)
{
        return c->filter(c, modified_domains);
}

static inline CSError Constraint_destroy(struct Constraint * c)
{
        if (c->filter == ConstraintSum_filter) {
                ConstraintSum_destroy(c);
        } else if (c->filter == ConstraintVisibility_filter) {
                ConstraintVisibility_destroy(c);
        }
        return NO_FAILURE;

}
static inline void Constraint_print(struct Constraint * c)
{
        printf("id=%u: | N=%u:\n", c->id, c->n_vars);
        for (unsigned i = 0; i < c->n_vars; i++) {
                printf("%u ", c->var[i]->id); bitset_print(c->var[i]->domain);
        }
        printf("\n");
}
#endif
