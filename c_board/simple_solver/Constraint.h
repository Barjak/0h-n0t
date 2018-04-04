#ifndef CONSTRAINT_H
#define CONSTRAINT_H
#include <assert.h>

#include "LNode.h"
#include "Var.h"
#include "CSError.h"

////////
// Constraint-specific Structures
////////
struct ConstraintSum {
        bitset domain;
};

struct ConstraintVisibility {
        unsigned n_lhs_variables;
};

struct ConstraintTile {
        unsigned dir_n[4];
        unsigned target_value;
};

/**
 * struct Constraint is a tagged union whose tag is Constraint::filter
 */
struct Constraint {
        unsigned      id;      /**< Index used by the solver. */
        unsigned      n_vars;  /**< Number of variables used by this constraint. */
        struct Var ** vars;    /**< List of pointers to variables.
                                    Every variable used by the filter MUST be in this list. */
        bitset      * domains; /**< Before invoking a filter, each variable's domain is copied
                                    here to use as a scratchpad. At some point in the future, this
                                    will reduce the number of copy operations and cache misses. */
        CSError    (* filter)(struct Constraint*, struct LNode **);
        /**< A domain propagation algorithm that also acts as a type tag.
             Invoking a filter passes back a linked list of domain restrictions.
             The solver applies the domain restrictions when it feels like it.
             Filters invoke C_push_restriction_on_nth_var() when they find a domain reduction. */
        union {
                struct ConstraintVisibility visibility_data;
                struct ConstraintSum        sum_data;
                struct ConstraintTile       tile_data;
        }; /**< This anonymous union contains constraint-specific data structures
                which indicate how the constraint's list of pointers is structured. */
};

struct Restriction {
        struct Var         * var;                    /**< Variable to be restricted. */
        bitset               domain;                 /**< The variable's new domain. */
        struct Constraint  * constraint;             /**< The constraint which generated the restriction. If NULL, then the domain was restricted by the user. */

        struct Restriction * var_restrict_prev;      /**< The variable's previous restriction. */
        struct LNode       * implications;           /**< Linked list of child restrictions. */
        unsigned             n_necessary_conditions; /**< Number of parent restrictions. */
        struct Restriction * necessary_conditions[]; /**< A flat list of parent restrictions. */
};

static struct Restriction * Restriction_create(struct Var * v, bitset domain, struct Constraint * c)
{
        unsigned N = c ? c->n_vars : 0;
        struct Restriction * r = malloc(sizeof(struct Restriction) + N * sizeof(struct Restriction*));
        if (!r) { goto bad_alloc1; }
        *r = (struct Restriction){
                .var                    = v,
                .domain                 = domain,
                .constraint             = c,
                .var_restrict_prev      = NULL,
                .implications           = NULL,
                .n_necessary_conditions = N};
        return r;
bad_alloc1:
        return NULL;
}

static inline CSError C_push_restriction_on_nth_var(struct Constraint * c, int index, bitset domain, struct LNode ** list)
{
        struct Restriction * r = Restriction_create(c->vars[index], domain, c);
        if (!r) {
                goto bad_alloc1;
        }
        if (FAIL_ALLOC == LNode_prepend(list, r, 0)) {
                goto bad_alloc2;
        }
        return NO_FAILURE;
bad_alloc2:
        free(r);
bad_alloc1:
        return FAIL_ALLOC;

}

// ConstraintTile

static inline CSError ConstraintTile_filter(struct Constraint * c, struct LNode ** restrictions_return)
{
        int modified;
        do {
                modified = 0;
                unsigned n_blue = 0;
                unsigned n_blue_dir[4] = {0,0,0,0};
                // index of the First Empty Domain (FED_i) for each direction
                // Not using a pointer because we'd like to use the index on the list of vars
                int FED_i[4] = {-1, -1, -1, -1};
                unsigned n_empty[4] = {0,0,0,0};
                unsigned add_one_yield[4] = {0,0,0,0};
                unsigned how_many_directions = 0;
                unsigned max_possible = 0;
                unsigned max_possible_dir[4] = {0,0,0,0};
                int only_direction = -1;

                for (int d = 0; d < 4; d++) {
                        for (unsigned i = (d ? c->tile_data.dir_n[d-1] : 0);
                             i < c->tile_data.dir_n[d];
                             i++) {
                                bitset domain = c->domains[i];
                                if (domain == (RED | BLUE)) {
                                        if (0 == n_empty[d]) {
                                                FED_i[d] = i;
                                                add_one_yield[d]++;

                                                how_many_directions++;
                                                only_direction = d;
                                        }
                                        n_empty[d]++;
                                } else if (domain == BLUE) {
                                        if (0 == n_empty[d]) {
                                                n_blue++;
                                                n_blue_dir[d]++;
                                        } else if (1 == n_empty[d]) {
                                                add_one_yield[d]++;
                                        }
                                } else if (domain == RED) {
                                        break;
                                } else {
                                        assert(0);
                                }
                                max_possible++;
                                max_possible_dir[d]++;
                        }
                }

                assert(n_blue <= c->tile_data.target_value);
                /* 1 */
                if (n_blue == c->tile_data.target_value) {
                        for (int d = 0; d < 4; d++) {
                                int i = FED_i[d];
                                if (i != -1) {
                                        c->domains[i] = RED;
                                        // RED is always more restricted than the domain in question if we get here
                                        if (FAIL_ALLOC == C_push_restriction_on_nth_var(c, i, RED, restrictions_return)) {
                                                goto bad_alloc1;
                                        }
                                        modified = 1;
                                        break;
                                }
                        }
                /* 2 */
                } else if (how_many_directions == 1) {
                        int i = FED_i[only_direction];
                        c->domains[i] = BLUE;
                        if (FAIL_ALLOC == C_push_restriction_on_nth_var(c, i, BLUE, restrictions_return)) {
                                goto bad_alloc1;
                        }
                        modified = 1;
                        break;
                } else {
                        for (int d = 0; d < 4; d++) {
                                unsigned max_possible_other_directions = (max_possible - max_possible_dir[d]);
                                /* 3 */
                                if (add_one_yield[d] + n_blue > c->tile_data.target_value) {
                                        int i = FED_i[d];
                                        assert(i != -1);
                                        c->domains[i] = RED;
                                        if (FAIL_ALLOC == C_push_restriction_on_nth_var(c, i, RED, restrictions_return)) {
                                                goto bad_alloc1;
                                        }
                                        modified = 1;
                                        break;
                                }
                                /* 4 */
                                // FIXME: Why does this make worse boards?
                                // else if (add_one_yield[d] &&
                                         // add_one_yield[d] + n_blue_dir[d] + max_possible_other_directions <= c->tile_data.target_value) {
                                            // int i = FED_i[d];
                                            // c->domains[i] = BLUE;
                                            // if (FAIL_ALLOC == C_push_restriction_on_nth_var(c, i, BLUE, restrictions_return)) {
                                                    // goto bad_alloc1;
                                            // }
                                            // modified = 1;
                                            // break;
                                // }
                        }
                }
        } while (modified);

        return NO_FAILURE;
bad_alloc1:
        LNode_destroy_and_free_data(restrictions_return);
        return FAIL_ALLOC;
}
// tile_bools is 4 different arrays concatenated together
// each array is in order of increasing distance from origin
// how_many[4] is the lengths of the 4 arrays
static inline CSError ConstraintTile_init(struct Constraint * c,
                                          unsigned target_value,
                                          struct Var ** tile_bools,
                                          unsigned how_many[4])
{
        c->filter = ConstraintTile_filter;
        c->n_vars = how_many[0] + how_many[1] + how_many[2] + how_many[3];
        c->vars = malloc(c->n_vars * sizeof(struct Var *));
        if (!c->vars) {
                goto bad_alloc1;
        }
        c->domains = malloc(c->n_vars * sizeof(bitset));
        if (!c->domains) {
                goto bad_alloc2;
        }

        for (unsigned i = 0; i < c->n_vars; i++) {
                c->vars[i] = tile_bools[i];
        }
        c->tile_data.dir_n[0] = how_many[0];
        c->tile_data.dir_n[1] = c->tile_data.dir_n[0] + how_many[1];
        c->tile_data.dir_n[2] = c->tile_data.dir_n[1] + how_many[2];
        c->tile_data.dir_n[3] = c->tile_data.dir_n[2] + how_many[3];

        c->tile_data.target_value = target_value;

        return NO_FAILURE;
bad_alloc2:
        free(c->vars);
bad_alloc1:
        return FAIL_ALLOC;
}

// ConstraintSum

/* Following Trick 2003
 */
static inline CSError ConstraintSum_filter(struct Constraint * c, struct LNode ** restrictions_return)
{
        unsigned fail = 0;
        unsigned N = c->n_vars;

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

        for (unsigned i = 0; i <= N; i++) {
                f[i] = 0;
                g[i] = 0;
        }
        f[0] = 1<<0;
        for (unsigned i = 1; i <= N; i++) {
                for (unsigned b = 0; b < DOMAIN_SIZE; b++) {
                        if (f[i-1] & (1 << b)) {
                                f[i] |= c->domains[i-1] << b;
                        }
                }
        }

        g[N] = f[N] & c->sum_data.domain;
        fail |= !g[N];
        if (fail) {
                fail = FAILURE;
                goto cleanup;
        }

        for (int i = N-1; i >= 0; i--) {
                for (unsigned b = 0; b < DOMAIN_SIZE; b++) {
                        // The expression
                        // !!((domains[i] << b) & g[i+1])
                        //     is 1 iff
                        //     there exists a v in domains[i]
                        //              and a u in g[i+1]      s.t.: v + b = u
                        // The expression
                        // g[i] |= {1,0} << b
                        //     writes the result to the appropriate bit
                        g[i] |= !!((c->domains[i] << b) & g[i+1]) << b;
                }
                g[i] &= f[i];
        }
        for (unsigned i = 0; i < N; i++) {
                bitset reduced_domain = 0;
                for (unsigned b = 0; b < DOMAIN_SIZE; b++) {
                        reduced_domain |= ((c->domains[i] & 1<<b) && ((f[i] << b) & g[i+1])) << b;
                }
                if (c->domains[i] != reduced_domain) {
                        if (FAIL_ALLOC == C_push_restriction_on_nth_var(c, i, reduced_domain, restrictions_return)) {
                                goto bad_alloc3;
                        }
                }
        }

        goto cleanup;
bad_alloc3:
        LNode_destroy_and_free_data(restrictions_return);
cleanup:
bad_alloc2:
        free(g);
bad_alloc1:
        free(f);
        return fail;
}

static inline CSError ConstraintSum_init(struct Constraint * c,
                                          bitset domain,
                                          struct Var ** addends,
                                          unsigned n_addends)
{
        c->filter = ConstraintSum_filter;
        c->n_vars = n_addends;
        c->vars = malloc(c->n_vars * sizeof(struct Var*));
        if (!c->vars) { goto bad_alloc1; }
        c->domains = malloc(c->n_vars * sizeof(bitset));
        if (!c->domains) { goto bad_alloc2; }

        for (unsigned i = 0; i < c->n_vars; i++) {
                c->vars[i] = addends[i];
                c->domains[i] = addends[i]->domain;
        }

        c->sum_data.domain = domain;
        return NO_FAILURE;

bad_alloc2:
        free(c->vars);
bad_alloc1:
        return FAIL_ALLOC;
}

// ConstraintVisibility

// There's probably a one liner that does this.
static inline CSError ConstraintVisibility_filter(struct Constraint * c, struct LNode ** restrictions_return)
{
        unsigned n_lhs = c->n_vars - 1; // Cardinality var
        unsigned rhs_i = c->n_vars - 1; // Index var

        bitset R = 0;
        bitset B = 0;
        bitset y = 0;
        y = c->domains[rhs_i];
        for (unsigned b = 0; b < n_lhs; b++) {
                unsigned isred =  !!HAS_RED(c->domains[b]);
                R |= isred << b;
                unsigned isblue = !!HAS_BLUE(c->domains[b]);
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
                bitset reduced_domain = (!!(B & 1<<b) << BLUEBIT) | (!!(R & 1<<b) << REDBIT);
                if (c->domains[b] != reduced_domain) {
                        if (FAIL_ALLOC == C_push_restriction_on_nth_var(c, b, reduced_domain, restrictions_return)) {
                                goto bad_alloc1;
                        }
                }
        }
        if (c->domains[rhs_i] != y) {
                if (FAIL_ALLOC == C_push_restriction_on_nth_var(c, rhs_i, y, restrictions_return)) {
                        goto bad_alloc2;
                }
        }

        return NO_FAILURE;
bad_alloc1:
bad_alloc2:
        LNode_destroy_and_free_data(restrictions_return);
        return FAIL_ALLOC;
}

static inline CSError ConstraintVisibility_init(struct Constraint * c,
                                                struct Var ** lhsvars,
                                                unsigned n_lhsvars,
                                                struct Var *rhsvar)
{
        c->filter = ConstraintVisibility_filter;
        c->n_vars = n_lhsvars + 1;
        c->vars = malloc(c->n_vars * sizeof(struct Var*));
        if (!c->vars) { goto bad_alloc1; }
        c->domains = malloc(c->n_vars * sizeof(bitset));
        if (!c->domains) { goto bad_alloc2; }

        unsigned i;
        for (i = 0; i < n_lhsvars; i++) {
                c->vars[i] = lhsvars[i];
                c->domains[i] = lhsvars[i]->domain;
        }
        c->vars[i] = rhsvar;

        return NO_FAILURE;
bad_alloc2:
        free(c->vars);
bad_alloc1:
        return FAIL_ALLOC;
}

static inline CSError Constraint_generic_destroy(struct Constraint * c)
{
        if (!c->vars || !c->domains) {
                goto fail;
        }
        free(c->vars);
        free(c->domains);
        return NO_FAILURE;
fail:
        return FAILURE;
}

////////
// Generic Constraint Functions
////////
static inline CSError Constraint_filter(struct Constraint * c, struct LNode ** ret)
{
        if (!ret || !c) {
                goto bad_input;
        }
        for (unsigned i = 0; i < c->n_vars; i++) {
                c->domains[i] = c->vars[i]->domain;
        }
        *ret = NULL;
        return c->filter(c, ret);
bad_input:
        return FAIL_PARAM;
}

static inline CSError Constraint_destroy(struct Constraint * c)
{
        CSError fail = NO_FAILURE;
        if (c->filter == ConstraintSum_filter) {
                fail = Constraint_generic_destroy(c);
        } else if (c->filter == ConstraintVisibility_filter) {
                fail = Constraint_generic_destroy(c);
        } else if (c->filter == ConstraintTile_filter) {
                fail = Constraint_generic_destroy(c);
        }
        return fail;
}
static inline void Constraint_print(struct Constraint * c)
{
        printf("id=%u: | N=%u:\n", c->id, c->n_vars);
        for (unsigned i = 0; i < c->n_vars; i++) {
                printf("%u ", c->vars[i]->id);
                bitset_print(c->vars[i]->domain);
        }
        printf("\n");
}
#endif
