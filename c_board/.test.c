#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <math.h>
#include "Board.h"

// #define nl printf("\n")
// #define SIZE 35
// #define TRIALS 1

#include "BoardReducer.h"

unsigned test_ConstraintVisibility()
{
#define R 1<<1
#define B 1<<0
#define W (R | B)

        unsigned fail = 0;
        struct Var lhs[7];
        struct Var *lhsp[7];
        struct Var rhs;
        struct Constraint vis;
        for (unsigned i = 0; i < 7; i++) {
                Var_create(&lhs[i], i, 2, 1<<0 | 1<<1);
                lhsp[i] = &lhs[i];
        }
        Var_create(&rhs, 7, 7, 1<<0 | 1<<1 | 1<<2 | 1<<3 | 1<<4 | 1<<5 | 1<<6 | 1<<7);

        ConstraintVisibility_create(&vis, lhsp, 7, &rhs);
        ConstraintVisibility_filter(&vis, NULL);
        for (unsigned i = 0; i < 8; i++) {
                bitset_print(vis.cached_domains[i]);
        }
        lhs[5].domain = 1<<0;
        lhs[2].domain = 1<<0;

        lhs[3].domain = 1<<1;
        rhs.domain &= ~(1<<0);

        ConstraintVisibility_filter(&vis, NULL);
        for (unsigned i = 0; i < 8; i++) {
                bitset_print(vis.cached_domains[i]);
        }
        return fail;
}

unsigned test_ConstraintSum(bitset target, unsigned n_sets, bitset * initial, bitset * final)
{
        unsigned fail = 0;
        struct Var vars[n_sets];
        struct Constraint cons;
        struct Var *tmp[n_sets];
        for (size_t i = 0; i < n_sets; i++) {
                Var_create(&vars[i], i, 11, initial[i]);
                bitset_print(vars[i].domain);
                tmp[i] = &vars[i];
        }
        ConstraintSum_create(&cons, target, tmp, n_sets);
        fail |= ConstraintSum_filter(&cons, NULL);
        bitset_print(cons.sum_data.domain);
        if (fail) {
                goto cleanup;
        }
        for (size_t i = 0; i < n_sets; i++) {
                fail |= (cons.cached_domains[i] != final[i]);
        }
cleanup:
        Constraint_destroy(&cons);
        return fail;
}
unsigned test_Problem()
{
        unsigned fail = 0;

        struct Problem * p = Problem_create();

        struct Var * vars1 = Problem_create_vars(p, 5, 2);
        struct Var * vars2 = Problem_create_vars(p, 1, 7);
        unsigned n_sums = 5;
        struct Constraint * sums = Problem_create_empty_constraints(p, n_sums);
        for (unsigned i = 0; i < 5; i++) {
                struct Var * tmp[16];
                unsigned j;
                for (j = 0; j <= i; j++) {
                        tmp[j] = &vars1[j];
                }
                tmp[j++] = &vars2[0];
                ConstraintSum_create(&sums[i], 1<<6 | 1<<7 | 1<<8 | 1<<9, tmp, i+2);
        }
        Problem_create_registry(p);

        Problem_solve(p);

        Problem_destroy(p);
        return fail;
}
unsigned test_QueueSet()
{
        int size = 10;
        unsigned fail = 0;
        struct Var vars[size];
        struct Constraint cons[size];
        struct QueueSet_arc * Q;
        // srand(time(0));
        for (unsigned i = 0; i < size; i++) {
                Var_create(&vars[i], i, size, 1<<0 | 1<<2);
                cons[i].id = i;
        }
        Q = QueueSet_create_arc(1024);
        for (unsigned i = 0; i < 10000; i++) {
                if (rand() % 2 == 0) {
                        unsigned x = rand() % size,
                                 y = rand() % size;
                                 // printf("PUSH %u %u\n", x, y);
                        QueueSet_insert_arc(Q, (struct Arc){&vars[x], &cons[y]});
                } else {
                        struct Arc arc;
                        if (NO_FAILURE == QueueSet_pop_arc(Q, &arc)) {
                                // printf("POP %u %u\n", arc.var->identifier, arc.constraint->identifier);
                        }
                        // else printf("EMPTY\n");
                }
        }
        // QNode_print(Q.root, 0);
        printf("%u\n", Q->n_entries);
        while (Q->n_entries) {
               QueueSet_pop_arc(Q, NULL);
        }
        return fail;
}

unsigned test_related_arcs()
{
        unsigned fail = 0;
        struct Problem * p = Problem_create();

        struct Var * vars1 = Problem_create_vars(p, 5, 2);
        struct Var * vars2 = Problem_create_vars(p, 1, 10);
        unsigned n_sums = 5;
        struct Constraint * sums = Problem_create_empty_constraints(p, n_sums);
        // struct Constraint * dummy = Problem_create_empty_constraints(&p, 1);

        for (unsigned i = 0; i < 5; i++) {
                struct Var * tmp[16];
                unsigned j;
                for (j = 0; j <= i; j++) {
                        tmp[j] = &vars1[j];
                }
                tmp[j++] = &vars2[0];
                ConstraintSum_create(&sums[i], 1<<0 | 1<<1 | 1<<2 | 1<<3 | 1<<6, tmp, i+2);

        }
        pln;
        Problem_create_registry(p);

        // Problem_enqueue_related_arcs(&p, (struct Arc){&vars1[4], &sums[4]});
        // Problem_enqueue_related_arcs(&p, (struct Arc){&vars1[0], &sums[2]});
pln;
        Problem_solve(p);
        for (unsigned i = 0; i < 5; i++) {
                printf("v%i = ", i); bitset_print(vars1[i].domain);
        }
        printf("vx = "); bitset_print(vars2[0].domain);

        // QNode_print(p.Q.root, 0);

        return fail;
}
void build_constraints(struct Problem * p,
                       struct Grid * board,
                       struct Var * tile_bools,
                       unsigned index,
                       struct Var * dummy_wall_var,
                       unsigned max_n_tiles)
{
        unsigned n_valid_directions = 0;
        struct Var * dir[4];

        for (unsigned d = 0; d < 4; d++) {
                struct Var * vars[30];
                int j = index;
                unsigned current_distance = 0;
                while(-1 != (j = Grid_traverse(board, j, Directions[d]))) {
                        vars[current_distance++] = &tile_bools[j];
                        if (current_distance == max_n_tiles + 1) {
                                break;
                        }
                }
                // If there are no tiles in this direction, continue
                if (current_distance == 0) {
                        continue;
                }
                // handle the case where we stopped at an edge
                if (current_distance <= max_n_tiles) {
                        vars[current_distance++] = dummy_wall_var;
                }
                dir[n_valid_directions] = Problem_create_vars(p, 1, current_distance);
                struct Constraint * how_many_visible = Problem_create_empty_constraints(p, 1);
                ConstraintVisibility_create(how_many_visible, vars, current_distance, dir[n_valid_directions]);
                n_valid_directions++;
        }

        struct Constraint * sum = Problem_create_empty_constraints(p, 1);
        ConstraintSum_create(sum, 1<<max_n_tiles, dir, n_valid_directions);
}

unsigned bools_are_single(struct Var * tile_bools, unsigned len)
{
        for (unsigned i = 0; i < len; i++) {
                if (tile_bools[i].domain == (RED | BLUE) ||
                    tile_bools[i].domain == 0) {
                            // printf("NOT SINGLE: %u\n", i);
                        return 0;
                }
        }
        return 1;
}

unsigned test_Board()
{
        pln;
        struct Grid * board = Grid_generate(5, 5);
        maxify(board, 5);
        pln;
        // breakdown(board);
        Grid_print(board);

pln;
        struct Problem * p =  Problem_create();

        struct Var * tile_bools = NULL;
        tile_bools = Problem_create_vars(p, board->length, 2);

        struct Var * RED_const = Problem_create_vars(p, 1, 2);
        Var_set(RED_const, RED);

        for (unsigned i = 0; i < board->length; i++) {
                switch (board->tiles[i].type){
                case NUMBER:
                case FILLED:
                        Var_set(&tile_bools[i], BLUE);
                        break;

                case WALL:
                        Var_set(&tile_bools[i], RED);
                        break;
                case EMPTY:
                        break;
                }
        }
        for (unsigned i = 0; i < board->length; i++) {
                if (board->tiles[i].type != NUMBER) {
                        continue;
                }
                unsigned max_n_tiles = board->tiles[i].value;
                unsigned n_valid_directions = 0;
                struct Var * dir[4];

                for (unsigned d = 0; d < 4; d++) {
                        struct Var * vars[30];
                        int j = i;
                        unsigned current_distance = 0;
                        while(-1 != (j = Grid_traverse(board, j, Directions[d]))) {
                                vars[current_distance++] = &tile_bools[j];
                                if (current_distance == max_n_tiles + 1) {
                                        break;
                                }
                        }
                        // If there are no tiles in this direction, continue
                        if (current_distance == 0) {
                                continue;
                        }
                        // handle the case where we stopped at an edge
                        if (current_distance <= max_n_tiles) {
                                vars[current_distance++] = RED_const;
                        }
                        dir[n_valid_directions] = Problem_create_vars(p, 1, current_distance);
                        struct Constraint * how_many_visible = Problem_create_empty_constraints(p, 1);
                        ConstraintVisibility_create(how_many_visible, vars, current_distance, dir[n_valid_directions]);
                        n_valid_directions++;
                }
                struct Constraint * sum = Problem_create_empty_constraints(p, 1);
                ConstraintSum_create(sum, 1<<max_n_tiles, dir, n_valid_directions);
        }
        // unsigned n_anchors = 0;
        // for (unsigned i = 0; i < board->length; i++) {
        //         if (board->tiles[i].type == NUMBER) {
        //                 build_constraints(&p, board, tile_bools, i, RED_const, board->tiles[i].value);
        //                 n_anchors++;
        //         }
        // }
        // printf("n_anchors = %u\n", n_anchors);

        Problem_create_registry(p);
        Grid_print(board);
        Problem_solve(p);

        // for (unsigned i = 0; i < p.n_constraints; i++) {
        //         printf("%u %u\n", i, LNode_count(p.c_registry[i].instances));
        // }

        Problem_destroy(p);
        return NO_FAILURE;
}
bitset t2bits(struct Tile * tile) {
        bitset b = 0;
        switch (tile->type) {
        case NUMBER:
        case FILLED:
                b = BLUE;
                break;
        case WALL:
                b = RED;
        case EMPTY:
                break;
        }
        return b;
}

////////////////////////////////////////////////////////////////////////////////
typedef enum { active = 0, inactive = 1, taboo = 2} State;
struct TileData {
        State state;
        struct Var * var;
        unsigned n_constraints;
        struct Constraint * constraints[5];
};

unsigned * get_random_order(unsigned size)
{
        srand(18);//clock());
        unsigned * ret = NULL;
        ret = malloc(size * sizeof(unsigned));
        assert(ret);
        for (unsigned i = 0; i < size; i++) {
                ret[i] = i;
        }
        for (int i = size - 1; i > 1; i--) {
                int j = rand() / ((double)RAND_MAX / i + 1);
                unsigned swap = ret[i];
                ret[i] = ret[j];
                ret[j] = swap;
        }
        return ret;
}

void print1(struct Grid * board, struct Var * tile_bools)
{
        for (int y = 0; y < board->height; y++) {
                for (int x = 0; x < board->width; x++) {
                        unsigned i = y*board->width+x;

                        switch (tile_bools[i].domain) {
                        case (BLUE | RED):

                                printf(" . ");
                                break;
                        case BLUE:
                                if (board->tiles[i].type == NUMBER) {
                                        printf("%2i ", board->tiles[i].value);
                                        continue;
                                } else {
                                        printf(" = ");
                                }
                                break;
                        case RED:
                                printf("## ");
                                break;
                        default:
                                // printf(" . ");
                                break;
                        }
               }
                printf("\n");
        }
}


void reduce()
{
        unsigned size = 80;
// Generate finished grid
        struct Grid * grid = Grid_generate(size, size);
        maxify(grid, 10);
        pln;
        // breakdown(grid);

        unsigned len = grid->length;
pln;
// Problem definition
        struct Problem  * p;
        p = Problem_create();

        struct Var * tile_bools = Problem_create_vars(p, len, 2);
        struct Var * RED_const = Problem_create_vars(p, 1, 2);
        Var_set(RED_const, RED);
        // Read
        for (unsigned i = 0; i < len; i++) {
                switch (grid->tiles[i].type) {
                case NUMBER:
                case FILLED:
                        Var_set(&tile_bools[i], BLUE);
                        break;
                case WALL:
                        Var_set(&tile_bools[i], RED);
                case EMPTY:
                        break;
                }
        }
pln;
// Init bookkeeping structures

        struct TileData tile_data[len];
        for (unsigned i = 0; i < len; i++) {
                tile_data[i].state = active;
                tile_data[i].var = &tile_bools[i];
                tile_data[i].n_constraints = 0;
        }

// Define the problem
        for (unsigned i = 0; i < len; i++) {
                if (grid->tiles[i].type != NUMBER) {
                        continue;
                }
                unsigned max_n_tiles = grid->tiles[i].value;
                unsigned n_valid_directions = 0;
                struct Var * dir[4];

                for (Direction d = 0; d < 4; d++) {
                        struct Var * vars[DOMAIN_SIZE+1];
                        struct Tile * t = &grid->tiles[i];
                        unsigned current_distance = 0;
                        while((t = t->dir[d])) {
                                vars[current_distance++] = &tile_bools[t->id];
                                if (current_distance == max_n_tiles + 1) {
                                        break;
                                }
                        }
                        // If there are no tiles in this direction, continue
                        if (current_distance == 0) {
                                continue;
                        }
                        // handle the case where we stopped at an edge
                        if (current_distance <= max_n_tiles) {
                                vars[current_distance++] = RED_const;
                                // printf("%i ", RED_const->id);
                        }
                        dir[n_valid_directions] = Problem_create_vars(p, 1, current_distance);
                        // printf("-> %i ", dir[n_valid_directions]->id);

                        struct Constraint * how_many_visible = Problem_create_empty_constraints(p, 1);
                        // printf(" #%i\n", how_many_visible->id);
                        ConstraintVisibility_create(how_many_visible, vars, current_distance, dir[n_valid_directions]);
                        n_valid_directions++;
                        // Remember
                        tile_data[i].constraints[tile_data[i].n_constraints++] = how_many_visible;
                }
                struct Constraint * sum = Problem_create_empty_constraints(p, 1);
                // printf("\tS = %i\n", sum->id);
                ConstraintSum_create(sum, 1<<max_n_tiles, dir, n_valid_directions);
                // Remember
                tile_data[i].constraints[tile_data[i].n_constraints++] = sum;
        }

        Problem_create_registry(p);
        Problem_solve(p);
        Grid_print(grid);

#       define tile_to_bitset(x) ((x).type == NUMBER ? BLUE : RED)

        unsigned * order = get_random_order(len);

        for (unsigned i = 0; i < len; i++) {
                unsigned index = order[i];
                // index = i;
                // printf("%.2f\n", 100.0 * (double)i / len);
                struct TileData * td = &tile_data[index];
                for (unsigned j = 0; j < td->n_constraints; j++) {
                        Problem_constraint_deactivate(p, td->constraints[j]);
                }
                Problem_var_reset_domain(p, td->var, BLUE | RED);
                // QueueSet_data(&p.Q);
                unsigned fail = Problem_solve_queue(p);
                assert( ! fail );
                unsigned singular = bools_are_single(tile_bools, len);
                if (! singular) {
                        for (unsigned j = 0; j < td->n_constraints; j++) {
                                Problem_constraint_activate(p, td->constraints[j]);
                        }
                        Problem_var_reset_domain(p, td->var, t2bits(&grid->tiles[index]));//tile_to_bitset(grid->tiles[i]));
                        td->state = taboo;
                } else {
                        td->state = inactive;
                }
        }

        Problem_solve(p);
        for (int y = 0; y < grid->height; y++) {
                for (int x = 0; x < grid->width; x++) {
                        unsigned i = y*grid->width+x;
                        char __m[3] = {'X','.','O'};
                        printf("%c ", __m[tile_data[i].state]);
                }
                printf("\n");
        }
        printf("\n");

        Problem_destroy(p);

        free(order);
}

// Select a tile (constraint + var). Preferably an unused constraint.
// Push them onto the backtrack stack
// Unrestrict the domain of the var
// Deactivate the constraint
        // 1. Set the inactive flag in the c_registry
        // 2. Optionally, push it to the back of its Vars' lists and decrement
        // 3. If the constraint has references in the DAG, remove all
        //    children of these nodes and rebuild.
// If a solution is found, repeat.
// Otherwise
        // 1. Activate the constraint and restrict the var
        // 2. Swap the domain restriction backtrack node
        //    with a taboo node
// Repeat this until either all constraint tiles are taboo or the optimal (fewest)
// number of taboo tiles are leftover



int main( int argc, char *argv[])
{

#define PASS(x) assert(!(x))
#define FAIL(x) assert(x)

/*        test_related_arcs();
        test_ConstraintVisibility();
        pln;
        { // Trick's knapsack example
                bitset initial[4] = {1<<0 | 1<<2, 1<<0 | 1<<3, 1<<0 | 1<<4, 1<<0 | 1<<5};
                bitset final[4] =   {1<<0 | 1<<2, 1<<0 | 1<<3, 1<<0 | 1<<4, 1<<5};
                PASS(test_ConstraintSum(1<<10 | 1<<11 | 1<<12, 4, initial, final));
        }
        pln;
        {
                bitset initial[3] = {1<<1 | 1<<2, 1<<3 | 1<<4, 1<<1 | 1<<5};
                bitset final[3] =   {0 | 0 | 0 | 0};
                FAIL(test_ConstraintSum(1<<8, 3, initial, final));
        }
        test_QueueSet();
        pln;
        test_Problem();*/
        // test_Board();
        reduce();
        return 0;
}
