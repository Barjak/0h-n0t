#ifndef PROBLEM_H
#define PROBLEM_H
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "Var.h"
#include "Constraint.h"
#include "QueueSet_arc.h"
#include "CSError.h"
#include "LNode.h"

#define pln printf("%s %i\n", __FILE__, __LINE__)

#define D_TRUE 1
#define D_FALSE 0

struct Restriction {
        struct Var         * var;
        bitset               domain;
        struct Constraint  * constraint;

        struct Restriction * var_restrict_prev; // List of definition Restrictions
        struct LNode       * implications; // (struct Restriction*)implications->data
        unsigned             n_necessary_conditions;
        struct Restriction * necessary_conditions[];
};

// List of all related constraints
// Inactive constraints are put at the end of the list
struct VarRegister {
        struct Var         * var;
        unsigned             n_constraints;
        unsigned             n_active_constraints;
        struct Constraint ** constraint;

        struct Restriction * most_recent_restriction;
};
// This could contain the list of all adjacent variables,
// but instead it's just a pointer to the corresponding constraint.
struct ConstraintRegister {
        struct Constraint * constraint;
        unsigned            active;

        struct LNode *      instances; // (struct Restriction*)instances->data
};

struct Problem {
        unsigned                    n_vars;
        unsigned                    n_constraints;
        unsigned                    n_DAG_nodes;

        // Memory blocks to be freed
        struct LNode              * var_llist;        // (Var*)var_llist->data
        struct LNode              * constraint_llist; // (Constraint*)constraint_llist->data

        struct VarRegister        * var_registry;
        void                      * var_registry_data;
        struct ConstraintRegister * c_registry;

        void                      * DAG_data;

        struct QueueSet_arc       * Q;
};

struct Problem * Problem_create();
CSError Problem_get_restriction(struct Problem * p,
                                struct Constraint * c,
                                struct Var * v,
                                struct Restriction ** rp);
struct Var * Problem_create_vars(struct Problem * p, unsigned n, unsigned domain_width);
struct Constraint * Problem_create_empty_constraints(struct Problem * p, unsigned n);
void Problem_destroy(struct Problem * p);
CSError Problem_enqueue_related_arcs(struct Problem * p, struct Arc arc);
CSError Problem_create_registry(struct Problem * p);
CSError Problem_solve_queue(struct Problem * p);
CSError Problem_solve(struct Problem * p);
CSError Problem_constraint_deactivate(struct Problem * p, struct Constraint * c);
CSError Problem_constraint_activate(struct Problem * p, struct Constraint * c);
CSError Problem_var_reset_domain(struct Problem * p, struct Var * v, bitset domain);
CSError Problem_add_DAG_node(struct Problem * p, struct Restriction * r);

#define P_var_register(p,v) (&(p)->var_registry[(v)->id])
#define P_cons_register(p,c) ((c) ? &(p)->c_registry[(c)->id] : NULL)
#define P_recent_restriction(p,v) (P_var_register((p),(v))->most_recent_restriction)
#define P_cons_is_active(p,c) (P_cons_register((p), (c))->active == 1)

#endif
