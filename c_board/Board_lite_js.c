#include "Board_lite_js.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "simple_solver/LNode.h"
#include "simple_solver/Problem.h"
#include "simple_solver/QueueSet_void_ptr.h"

typedef struct { int x; int y; } Vector;
typedef enum { EMPTY = -3, WALL = -2, FILLED = -1, NUMBER = 0} Type;
typedef enum { NO_DIRECTION = -1, UP = 0, DOWN = 1, LEFT = 2, RIGHT = 3} Direction;
extern Vector Directions[4];
#ifndef min
#        define min(x,y) ((x) < (y)?(x):(y))
#endif
#ifndef max
#        define max(x,y) ((x) > (y)?(x):(y))
#endif
#define IS_WALL(type) ((type) == WALL)
#define RANDOM(n) (int)(rand() / ((double)RAND_MAX / (n)  + 1))
Vector Directions[4] = {{.x = 0,  .y = -1},
                        {.x = 0,  .y = 1},
                        {.x = -1, .y = 0},
                        {.x = 1,  .y = 0}};

typedef enum { ACTIVE = 0, INACTIVE = 1, TABOO = 2} State;

struct Tile {
        int           id;
        int           value;
        Type          type;
        struct Tile * dir[4];
};

struct Grid {
        int         width,
                    height,
                    length;
        void      * Problem;
        struct Tile tiles[];
};

struct TileData {
        State               state;
        bitset              old_domain;
        struct Var        * var;
        unsigned            n_constraints;
        struct Constraint * constraints[5];
};

struct ProblemData {
        struct Problem * problem;

        unsigned       * order;
        unsigned         i;

        unsigned         length;
        struct TileData  tile_data[];
};

struct Board {
        unsigned             length,
                             width,
                             height;
        struct Grid        * max_grid;
        struct Grid        * min_grid;
        int                * min_tile_mask;

        struct ProblemData * pdata;
};

///////////////////
// Helper functions
///////////////////
int tile2int(struct Tile * tile)
{
        return tile->type + tile->value;
}
void int2tile(Type i, struct Tile * tile)
{
        tile->type = (i >= 0) ? 0 : i;
        tile->value = (i >= 0) ? i : 0;
}

unsigned * get_random_order(unsigned size)
{
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

unsigned bools_are_single(struct ProblemData * pdata)
{
        for (unsigned i = 0; i < pdata->length; i++) {
                if (pdata->tile_data[i].var->domain == (RED | BLUE) ||
                    pdata->tile_data[i].var->domain == 0) {
                            // printf("NOT SINGLE: %u\n", i);
                        return 0;
                }
        }
        return 1;
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
                break;
        case EMPTY:
                b = BLUE | RED;
                break;
        }
        return b;
}


/**
 * return: -1 if can't traverse
 *         else the index of the next tile
 */
int Grid_traverse(struct Grid * board, int index, Vector v)
{
        int x = v.x + (index % board->width),
            y = v.y + (index / board->width);
        int oob = !(x < 0 || x >= board->width || y < 0 || y >= board->height);
        return (y * board->width + x) * oob - !oob;
}
#define TRAVERSE(t, direction) ((t) = (t)->dir[(direction)])

struct Grid * Grid_create(int width, int height)
{
        unsigned length = width * height;
        struct Grid * board = malloc(sizeof(struct Grid) + sizeof(struct Tile) * length);
        if (!board) {
                goto bad_alloc1;
        }
        assert(board);
        board->width = width;
        board->height = height;
        board->length = length;
        for (unsigned i = 0; i < length; i++) {
                board->tiles[i].id = i;
                board->tiles[i].type = NUMBER;
                board->tiles[i].value = length;
        }
        for (unsigned i = 0; i < length; i++) {
                for (unsigned d = 0; d < 4; d++) {
                        const Vector dir = Directions[d];
                        int j = Grid_traverse(board, i, dir);
                        board->tiles[i].dir[d] = (j == -1) ? NULL : &board->tiles[j];
                }
        }
        return board;
bad_alloc1:
        return NULL;
}

void Grid_free(struct Grid * board)
{
        assert(board);
        free(board);
}
void Grid_copy_to(struct Grid * grid1, struct Grid * grid2)
{
        assert(grid1->width == grid2->width);
        assert(grid1->height == grid2->height);
        assert(grid1->length == grid2->length);

        for (int i = 0; i < grid1->length; i++) {
                grid2->tiles[i].id = grid1->tiles[i].id;
                grid2->tiles[i].type = grid1->tiles[i].type;
                grid2->tiles[i].value = grid1->tiles[i].value;
        }
}

void Grid_print(struct Grid * board)
{
        for (int y = 0; y < board->height; y++) {
                for (int x = 0; x < board->width; x++) {
                        struct Tile * t = &board->tiles[y * board->width + x];
                        switch (t->type) {
                        case NUMBER:
                                printf("%3i ", t->value);
                                break;
                        case WALL:
                                printf(" ## ");
                                break;
                        case EMPTY:
                                printf("  . ");
                                break;
                        case FILLED:
                                printf("  - ");
                                break;
                        }
                }
                printf("\n");
        }
}

unsigned count_tiles_in_range(struct Grid * board, struct Tile * tile)
{
        unsigned count = 0;
        for (Direction d = 0; d < 4; d++) {
                struct Tile * t = tile;
                while ((t = t->dir[d])) {
                        if (IS_WALL(t->type))
                                break;
                        count++;
                }
        }
        return count;
}

struct Tile * get_random_neighbor(struct Grid * board, struct Tile * tile)
{
        struct Tile * cut = NULL;
        unsigned n = 1;
        unsigned count = 0;
        for (Direction d = 0; d < 4; d++) {
                struct Tile * t = tile;
                unsigned range = 0;
                while (TRAVERSE(t, d)) {
                        count++;
                        if (range++ < 1)
                                continue;
                        if (IS_WALL(t->type))
                                break;
                        if (0 == (rand() % n++)) {
                                cut = t;
                        }
                }
        }
        if (1 == count) {
                return NULL;
        }
        return cut;
}
void update_values(struct Grid * board, struct Tile * tile)
{
        for (Direction d = 0; d < 4; d++) {
                struct Tile * t = tile;
                while (TRAVERSE(t, d)) {
                        if (IS_WALL(t->type))
                                break;
                        int2tile(count_tiles_in_range(board, t), t);
                        if (t->value == 0) {
                                int2tile(WALL, t);
                        }
                }
        }
}

unsigned maxify(struct Grid * board, int maxAllowed)
{
        srand(time(0));

        struct QueueSet_void_ptr * Q = QueueSet_create_void_ptr();
        if (!Q) {
                goto bad_alloc1;
        }

        unsigned * order = get_random_order(board->length);
        if (!order) {
                goto bad_alloc2;
        }

        for (int i = 0; i < board->length; i++) {
                unsigned index = order[i];
                board->tiles[index].value = count_tiles_in_range(board, &board->tiles[index]);
                if (board->tiles[index].value > maxAllowed) {
                        QueueSet_insert_void_ptr(Q, &board->tiles[index]);
                }
        }
        while (Q->n_entries != 0) {
                void * ptr = NULL;
                QueueSet_pop_void_ptr(Q, &ptr);
                struct Tile * tile = ptr;
                if (tile->value > maxAllowed) {
                        struct Tile * cut = get_random_neighbor(board, tile);
                        if (cut) {
                                int2tile(WALL, cut);
                                update_values(board, cut);
                                if (tile->value > maxAllowed) {
                                        QueueSet_insert_void_ptr(Q, tile);
                                }
                                for (Direction d = 0; d < 4; d++) {
                                        struct Tile * t = cut;
                                        while (TRAVERSE(t, d)) {
                                                if (IS_WALL(t->type)) {
                                                        break;
                                                }
                                                if (t->value > maxAllowed) {
                                                        QueueSet_insert_void_ptr(Q, t);
                                                }
                                        }
                                }
                        }
                }
        }
        free(order);
        return NO_FAILURE;
bad_alloc2:
        QueueSet_destroy_void_ptr(Q);
bad_alloc1:
        return FAIL_ALLOC;
}
//////////////
// ProblemData
//////////////

struct ProblemData * PData_create(struct Grid * grid)
{
        unsigned len = grid->length;
        struct ProblemData * pdata = malloc(sizeof(struct ProblemData) + len * sizeof(struct TileData));
// Problem definition
        if (! pdata) {
                goto bad_alloc1;
        }
        struct Problem * p = Problem_create();
        if (!p) {
                goto bad_alloc2;
        }

        struct Var * tile_bools = Problem_create_vars(p, len, 2);
        // Read
        for (unsigned i = 0; i < len; i++) {
                switch (grid->tiles[i].type) {
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

// Init bookkeeping structures
        unsigned max_tile_in_board = 0;
        struct TileData * tile_data = &pdata->tile_data[0];
        for (unsigned i = 0; i < len; i++) {
                tile_data[i].old_domain = t2bits(&grid->tiles[i]);
                tile_data[i].state = ACTIVE;
                tile_data[i].var = &tile_bools[i];
                tile_data[i].n_constraints = 0;
                max_tile_in_board = max(grid->tiles[i].value, max_tile_in_board);
        }
        pdata->problem = p;
        pdata->length = len;
        pdata->order = get_random_order(len);
        pdata->i = 0;

        // printf("%u\n",max_tile_in_board);
        struct Var ** vars = malloc(4 * (max_tile_in_board+1) * sizeof(struct Var *));
        if (!vars) {
                goto bad_alloc3;
        }
        for (int i = 0; i <4 * (max_tile_in_board+1); i++) vars[i] = (struct Var *)17;
// Define the problem
        for (unsigned i = 0; i < len; i++) {
                if (grid->tiles[i].type != NUMBER) {
                        continue;
                }
                unsigned target_value = grid->tiles[i].value;

                unsigned current_distances[4] = {0,0,0,0};
                unsigned n_neighbors = 0;
                for (Direction d = 0; d < 4; d++) {
                        struct Tile * t = &grid->tiles[i];
                        while ((t = t->dir[d])) {
                                vars[n_neighbors++] = &tile_bools[t->id];
                                current_distances[d]++;
                                if (current_distances[d] == target_value + 1) {
                                        break;
                                }
                        }
                        // If there are no tiles in this direction, continue
                        if (current_distances[d] == 0) {
                                continue;
                        }
                }
                struct Constraint * tile = Problem_create_empty_constraints(p, 1);
                ConstraintTile_create(tile, target_value, vars, current_distances);
                // Remember
                tile_data[i].constraints[tile_data[i].n_constraints++] = tile;
        }
        free(vars);
        // struct LNode * block = p->constraint_llist;
        // while (block) {
        //         struct Constraint * list = block->data;
        //         for (struct Constraint * c = list; c < list + block->integer; c++) {
        //                 printf("id: %u\n", c->id);
        //         }
        //         block = block->next;
        // }
        Problem_create_registry(p);
        Problem_solve(p);
        return pdata;
bad_alloc3:
        Problem_destroy(p);
bad_alloc2:
        free(pdata);
bad_alloc1:
        return NULL;
}

void PData_destroy(struct ProblemData * pdata)
{
        if (pdata->problem) {
                free(pdata->order);
                Problem_destroy(pdata->problem);
        }
        free(pdata);
}
double Board_reduce(struct Board * board, unsigned batch_size)
{
        struct ProblemData * pdata = board->pdata;
        struct Problem * p = pdata->problem;

        batch_size = (batch_size <= 0) ? pdata->length : batch_size;
        unsigned i;
        for (i = pdata->i; i < min(pdata->i + batch_size, pdata->length); i++) {
                unsigned index = pdata->order[i];
                struct TileData * td = &pdata->tile_data[index];

                for (unsigned j = 0; j < td->n_constraints; j++) {
                        Problem_constraint_deactivate(p, td->constraints[j]);
                }
                Problem_var_reset_domain(p, td->var, BLUE | RED);
                int fail = Problem_solve_queue(p);
                NOFAIL(fail);
                if (! bools_are_single(pdata)) {
                        for (unsigned j = 0; j < td->n_constraints; j++) {
                                Problem_constraint_activate(p, td->constraints[j]);
                        }
                        Problem_var_reset_domain(p, td->var, td->old_domain);
                        td->state = TABOO;
                } else {
                        td->state = INACTIVE;
                }

                board->min_tile_mask[index] = (TABOO == td->state) ? 1 : 0;
                if (TABOO != td->state) {
                        int2tile(EMPTY, &board->min_grid->tiles[index]);
                }
        }
        pdata->i = i;

        Problem_solve(p);
        return (double)pdata->i / pdata->length;
}
CSError Board_allocate_grids(struct Board * board, unsigned width, unsigned height)
{
        unsigned length = width * height;
        board->width = width;
        board->height = height;
        board->length = length;

        board->max_grid = Grid_create(width, height);
        if (!board->max_grid) { goto bad_alloc1; }

        board->min_grid = Grid_create(width, height);
        if (!board->min_grid) { goto bad_alloc2; }

        board->min_tile_mask = malloc(length * sizeof(int));
        if (!board->min_tile_mask) { goto bad_alloc3; }

        return NO_FAILURE;

bad_alloc3:
        Grid_free(board->min_grid);
bad_alloc2:
        Grid_free(board->max_grid);
bad_alloc1:
        return FAIL_ALLOC;
}

struct Board * Board_create(unsigned width, unsigned height)
{
        struct Board * board = malloc(sizeof(struct Board));
        if (!board) {
                goto bad_alloc1;
        }
        board->max_grid = NULL;
        board->min_grid = NULL;
        board->min_tile_mask = NULL;
        board->pdata = NULL;
        board->length = width * height;
        board->width = width;
        board->height = height;
        CSError fail = Board_allocate_grids(board, width, height);

        if (fail) {
                goto bad_alloc2;
        }
        return board;
bad_alloc2:
        free(board);
bad_alloc1:
        return NULL;
}

void Board_destroy(struct Board * board)
{
        if (board) {
                free(board->max_grid);
                free(board->min_grid);
                free(board->min_tile_mask);
                if (board->pdata) {
                        PData_destroy(board->pdata);
                }
                free(board);
        }
}

unsigned Board_maxify(struct Board * board, unsigned max_tile)
{
        maxify(board->max_grid, max_tile);
        Grid_copy_to(board->max_grid, board->min_grid);

        board->pdata = PData_create(board->min_grid);

        if (!board->pdata) {
                goto bad_alloc1;
        }
        return NO_FAILURE;
bad_alloc1:
        return FAIL_ALLOC;
}

int main()
{
        clock_t t0 = clock();
        // printf("YOLO\n");
        struct Board * board = Board_create(50, 50);
        Board_maxify(board, 9);
        Grid_print(board->max_grid);

        double frac = 0.;
        while (frac != 1.) {
                frac = Board_reduce(board, 100);
                printf("%.2f%%\n", 100. * frac);
        }
        Grid_print(board->min_grid);
        printf("%.2f sec.\n", (clock() - t0)/(double)CLOCKS_PER_SEC);

        Board_destroy(board);
}
