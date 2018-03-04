#include "Problem.h"
#include "QueueSet_void_ptr.h"

struct Problem * Problem_create()
{
        struct Problem * p = malloc(sizeof(struct Problem));
        if (!p) {
                goto bad_alloc1;
        }
        *p = (struct Problem){
                .n_vars            = 0,
                .n_constraints     = 0,
                .n_DAG_nodes       = 0,
                .var_llist         = NULL,
                .constraint_llist  = NULL,
                .var_registry      = NULL,
                .var_registry_data = NULL,
                .c_registry        = NULL,
                .DAG_data          = NULL};
        p->Q = QueueSet_create_void_ptr();
        return p;
bad_alloc1:
        return NULL;
}

CSError Problem_enqueue_related_constraints(struct Problem * p, struct Var * v)
{
        struct VarRegister * vreg = P_var_register(p, v);
        for (struct Constraint ** cp = vreg->constraint;
             cp < vreg->constraint + vreg->n_active_constraints;
             cp++) {
                if ( ! P_cons_is_active(p, *cp)) {
                        continue;
                }
                QueueSet_insert_void_ptr(p->Q, *cp);
        }
        return NO_FAILURE;
}

CSError Problem_add_DAG_node(struct Problem * p, struct Restriction * r)
{
        struct VarRegister * vreg = P_var_register(p, r->var);
        if (r->constraint) {
                struct ConstraintRegister * creg = P_cons_register(p, r->constraint);
                r->n_necessary_conditions = r->constraint->n_vars;

                // Restrict the domain in question at this time

                // Add it to the c_registry's linked list

                // Find all the necessary condition variables
                // Get their corresponding most recent restrictions
                // Put them in an array and assign to r->necessary_conditions

                for (unsigned i = 0; i < r->n_necessary_conditions; i++) {
                        struct Var * current = r->constraint->vars[i];
                        struct Restriction * parent = P_var_register(p, current)->most_recent_restriction;

                        r->necessary_conditions[i] = parent;
                        // Append the present restriction to its parents' implications linked lists
                        // (Potentially using a custom allocator)
                        assert(parent != NULL);
                        LNode_prepend(&parent->implications, r, 0);
                        // assert(LNode_count(parent->implications) != 0);
                }

                LNode_prepend(&creg->instances, r, 0);
        }
        Var_set(r->var, r->domain);
        // Update the var_registry's most recent restriction link
        // and remember var_restrict_prev
        r->var_restrict_prev = vreg->most_recent_restriction;
        vreg->most_recent_restriction = r;
        p->n_DAG_nodes++;
        return NO_FAILURE;
}
// TODO: flush the cached domains or determine that they're fine
static CSError Problem_remove_DAG_node(struct Problem * p, struct Restriction * r, unsigned enqueue_invalidated_arcs)
{
        int fail = NO_FAILURE;
        struct VarRegister * vreg = P_var_register(p, r->var);
        if (r->constraint) {
                struct ConstraintRegister * creg = P_cons_register(p, r->constraint);
                // Need to remove it from c_registry
                fail = LNode_remove_node(&creg->instances, r);
                NOFAIL(fail);
                // Go to r's parents and remove r from their implications (traversing backwards?)
                for (unsigned i = 0; i < r->n_necessary_conditions; i++) {
                        assert(r->necessary_conditions[i]->implications);
                        fail = LNode_remove_node(&r->necessary_conditions[i]->implications, r);
                        NOFAIL(fail);
                }
        } else {
                assert(r->n_necessary_conditions == 0);
        }
        while (r->implications) {
                Problem_remove_DAG_node(p, r->implications->data, enqueue_invalidated_arcs);
                // This call removes the top element from r's implications also
        }

        vreg->most_recent_restriction = r->var_restrict_prev;
        if (r->var_restrict_prev) {
                Var_set(r->var, r->var_restrict_prev->domain);
        }

        if (enqueue_invalidated_arcs) {
                Problem_enqueue_related_constraints(p, r->var);
        }

        free(r);
        p->n_DAG_nodes--;
        return NO_FAILURE;
}

CSError Problem_solve_queue(struct Problem * p)
{
        int fail = NO_FAILURE;
        struct QueueSet_void_ptr * Q = p->Q;
        while (Q->n_entries != 0) {
                // Pop from Queue
                void * ptr = NULL;
                fail = QueueSet_pop_void_ptr(Q, &ptr);
                struct Constraint * c = ptr;
                NOFAIL(fail);
                // printf("%lu yolo %lu\n", (unsigned long)p, (unsigned long)c->id);
                if ( ! (p)->c_registry[(c)->id].active) {
                        continue;
                }
                struct LNode * restrictions = NULL;
                fail = Constraint_filter(c, &restrictions);
                NOFAIL(fail);
                while (restrictions) {
                        // If a domain reduction is found, call Problem_add_DAG_node()
                        struct Restriction * r = LNode_pop(&restrictions);
                        if (r->domain == 0) {
                                goto infeasible;
                        }
                        // TODO: all restrictions in the list should have the same necessary conditions
                        //       instead of depending on their siblings.
                        fail = Problem_add_DAG_node(p, r);
                        NOFAIL(fail);
                        // Push every arc that touches one of this var's constraints
                        Problem_enqueue_related_constraints(p, r->var);
                }
        }
        return NO_FAILURE;
infeasible:
        return FAILURE;
}

CSError Problem_solve(struct Problem * p)
{
        struct QueueSet_void_ptr * Q = p->Q;
        // Initialize Queue
        for (unsigned c_i = 0; c_i < p->n_constraints; c_i++) {
                struct ConstraintRegister * cr = &p->c_registry[c_i];
                if (cr->active == 0) {
                        continue;
                }
                QueueSet_insert_void_ptr(Q, cr->constraint);
        }

        return Problem_solve_queue(p);
}


CSError Problem_constraint_deactivate(struct Problem * p, struct Constraint * c)
{
        // find registry member
        struct ConstraintRegister * cr = P_cons_register(p, c);
        assert(cr);
        // change the flag
        cr->active = 0;
        // destroy each reference in the list
        while (cr->instances) {
                // struct Restriction * r = LNode_pop(&cr->instances);
                Problem_remove_DAG_node(p, cr->instances->data, 1);
        }

        return NO_FAILURE;
}
CSError Problem_constraint_activate(struct Problem * p, struct Constraint * c)
{
        struct ConstraintRegister * cr = P_cons_register(p, c);
        cr->active = 1;

        for (unsigned j = 0; j < c->n_vars; j++) {
                assert(c->vars[j]);
                QueueSet_insert_void_ptr(p->Q, c);
        }

        return NO_FAILURE;
}
CSError Problem_var_reset_domain(struct Problem * p, struct Var * v, bitset domain)
{
        struct VarRegister * vr = P_var_register(p, v);
        // struct Restriction * mrr = vr->most_recent_restriction
        // Destroy all dependent restrictions
        while (vr->most_recent_restriction) {
                struct Restriction * r = vr->most_recent_restriction;
                vr->most_recent_restriction = r->var_restrict_prev;
                Problem_remove_DAG_node(p, r, 1);
        }
        assert(vr->most_recent_restriction == NULL);
        struct Restriction * r = Restriction_create(v, domain, NULL);
        if (r == NULL) {
                goto bad_alloc1;
        }
        Problem_add_DAG_node(p, r);

        Problem_enqueue_related_constraints(p, v);
        // Change the initial node's domain
        v->domain = domain;
        return NO_FAILURE;
bad_alloc1:
        return FAIL_ALLOC;
}
static CSError Problem_DAG_init(struct Problem * p)
{
        int i;
        for (i = 0; i < (int)p->n_vars; i++) {
                struct Restriction * r = Restriction_create(p->var_registry[i].var, p->var_registry[i].var->domain, NULL);
                if (r == NULL) { goto bad_alloc1; }
                p->var_registry[i].most_recent_restriction = r; // put in registry
        }
        p->n_DAG_nodes = p->n_vars;
        return NO_FAILURE;
bad_alloc1:
        for (i = i; i >= 0; i--) {
                free(p->var_registry[i].most_recent_restriction);
                p->var_registry[i].most_recent_restriction = NULL;
        }

        return FAIL_ALLOC;
}

CSError Problem_create_registry(struct Problem * p)
{
        // Create constraint registries
        struct ConstraintRegister * c_register = malloc(p->n_constraints * sizeof(struct ConstraintRegister));
        if (!c_register) { goto bad_alloc1; }
        struct VarRegister * var_registry = malloc(p->n_vars * sizeof(struct VarRegister));
        if (!var_registry) { goto bad_alloc2; }
        // Init all constraints
        struct LNode * block = p->constraint_llist;
        while (block) {
                struct Constraint * list = block->data;
                for (struct Constraint * c = list; c < list + block->integer; c++) {
                        unsigned id = c->id;
                        // printf("id: %u\n", id);
                        c_register[id].constraint = c;
                        c_register[id].active = 1;
                        c_register[id].instances = NULL;
                }
                block = block->next;
        }
        // Init all vars
        block = p->var_llist;
        while (block) {
                struct Var * vars = block->data;
                for (unsigned i = 0; i < block->integer; i++) {
                        unsigned v_id = vars[i].id;
                        var_registry[v_id].var = &vars[i];
                        var_registry[v_id].n_constraints = 0;
                        var_registry[v_id].n_active_constraints = 0;
                        var_registry[v_id].constraint = NULL;
                        var_registry[v_id].most_recent_restriction = NULL;
                }
                block = block->next;
        }
        // Pass 1: Find lengths
        unsigned total_array_size = 0;
        for (struct ConstraintRegister * cr = c_register; cr != c_register + p->n_constraints; cr++) {
                assert(cr->constraint != NULL);
                struct Var ** vars = cr->constraint->vars;
                for (unsigned v_i = 0; v_i < cr->constraint->n_vars; v_i++) {
                        // printf("v_i %u\n", v_i);
                        // printf("vars %ul\n", vars[v_i]);
                        unsigned v_id = vars[v_i]->id;
                        var_registry[v_id].n_constraints += 1;
                        total_array_size += 1;
                }
        }
        // Pass 2: Allocate all register tables
        struct Constraint ** constraint_buffer = malloc(total_array_size * sizeof(struct Constraint*));
        if (!constraint_buffer) { goto bad_alloc3; }
        unsigned offset = 0;
        for (unsigned v_id = 0; v_id < p->n_vars; v_id++) {
                if (var_registry[v_id].n_constraints) {
                        var_registry[v_id].constraint = constraint_buffer + offset;
                } // else remain NULL
                offset += var_registry[v_id].n_constraints;
        }
        // Pass 3: Build all arrays
        for (struct ConstraintRegister * cr = c_register; cr != c_register + p->n_constraints; cr++) {
                struct Var ** vars = cr->constraint->vars;
                for (unsigned v_i = 0; v_i < cr->constraint->n_vars; v_i++) {
                        struct VarRegister * v_reg = &var_registry[vars[v_i]->id];
                        v_reg->constraint[v_reg->n_active_constraints++] = cr->constraint;
                }
        }
        for (unsigned v_id = 0; v_id < p->n_vars; v_id++) {
                assert(var_registry[v_id].n_constraints ==
                        var_registry[v_id].n_active_constraints);
        }

        p->var_registry = var_registry;
        p->var_registry_data = constraint_buffer;
        p->c_registry = c_register;

        Problem_DAG_init(p);

        return NO_FAILURE;

bad_alloc3:
        free(constraint_buffer);
bad_alloc2:
        free(var_registry);
bad_alloc1:
        return FAIL_ALLOC;
}


struct Var * Problem_create_vars(struct Problem * p, unsigned n, unsigned domain_width)
{
        struct Var * block = malloc(sizeof(struct Var) * n);
        if (!block) {
                return NULL;
        }
        LNode_prepend(&p->var_llist, block, n);
        for (unsigned i = 0; i < n; i++) {
                Var_create(&block[i], p->n_vars++, domain_width, (1<<(domain_width))-1 );
        }
        return block;
}


struct Constraint * Problem_create_empty_constraints(struct Problem * p, unsigned n)
{
        struct Constraint * block = malloc(sizeof(struct Constraint) * n);
        if (!block) {
                return NULL;
        }
        LNode_prepend(&p->constraint_llist, block, n);
        // printf("%lu\n",(unsigned long)p->constraint_llist);
        for (unsigned i = 0; i < n; i++) {
                block[i].id = p->n_constraints++;
        }
        return block;
}

void Problem_destroy(struct Problem * p)
{
        if (p->c_registry) {
                for (unsigned c_i = 0; c_i < p->n_constraints; c_i++) {
                        Constraint_destroy(p->c_registry[c_i].constraint);
                        while (p->c_registry[c_i].instances) {
                                Problem_remove_DAG_node(p, p->c_registry[c_i].instances->data, 0);
                        }
                }
                free(p->c_registry);
        }
        if (p->var_registry) {
                assert(p->var_registry_data);
                free(p->var_registry);
                free(p->var_registry_data);
        }
        QueueSet_destroy_void_ptr(p->Q);

        LNode_destroy_and_free_data(&p->var_llist);
        LNode_destroy_and_free_data(&p->constraint_llist);
}
