#include "Board.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "simple_solver/LNode.h"
#include "simple_solver/Problem.h"
#include "simple_solver/QueueSet_void_ptr.h"

#define SAVEFILE_NAME ".last_session"
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

typedef enum {ACTIVE = 0, INACTIVE = 1, TABOO = 2} State;

typedef enum {EASY = 0, HARD = 1} Difficulty;

struct TileData {
        State               state;
        bitset              old_domain;
        struct Var        * var;
        unsigned            n_constraints;
        struct Constraint * constraints[5];
};

struct UndoAction {
        struct Tile * tile;
        Type          old_type;
        Type          new_type;
};

struct ProblemData {
        struct Problem * problem;

        unsigned       * order;
        unsigned         i;

        unsigned         n_empty;
        struct LNode   * mistakes;

        unsigned         length;
        struct TileData  tile_data[];

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


#define TRAVERSE(t, direction) ((t) = (t)->dir[(direction)])

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

        struct QueueSet_void_ptr * Q = QueueSet_create_void_ptr(board->length);
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

struct ProblemData * PData_create(struct Grid * grid, int difficulty)
{
// Initialize things
        unsigned len = grid->length;
        struct ProblemData * pdata = malloc(sizeof(struct ProblemData) + len * sizeof(struct TileData));
        if (! pdata) {
                goto bad_alloc1;
        }
        struct Problem * p = Problem_create();
        if (!p) {
                goto bad_alloc2;
        }

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
                        break;
                case EMPTY:
                        break;
                }
        }

// Init bookkeeping structures
        struct TileData * tile_data = &pdata->tile_data[0];
        pdata->n_empty = 0;
        for (unsigned i = 0; i < len; i++) {
                tile_data[i].old_domain = t2bits(&grid->tiles[i]);
                tile_data[i].state = ACTIVE;
                tile_data[i].var = &tile_bools[i];
                tile_data[i].n_constraints = 0;
                pdata->n_empty += (grid->tiles[i].type == EMPTY);
        }
        pdata->problem = p;
        pdata->length = len;
        pdata->order = get_random_order(len);
        pdata->i = 0;
        pdata->mistakes = NULL;

        unsigned max_tile_in_board = 0;
        for (unsigned i = 0; i < len; i++) {
                max_tile_in_board = max(grid->tiles[i].value, max_tile_in_board);
        }

        struct Var ** vars = malloc(4 * (max_tile_in_board+1) * sizeof(struct Var *));
        if (!vars) {
                goto bad_alloc3;
        }
// Define the problem
        if (HARD == difficulty) {
                for (unsigned i = 0; i < len; i++) {
                        if (grid->tiles[i].type != NUMBER) {
                                continue;
                        }
                        unsigned target_value = grid->tiles[i].value;
                        unsigned n_valid_directions = 0;
                        struct Var * dir[4];

                        for (Direction d = 0; d < 4; d++) {
                                struct Tile * t = &grid->tiles[i];
                                unsigned current_distance = 0;
                                while((t = t->dir[d])) {
                                        vars[current_distance++] = &tile_bools[t->id];
                                        if (current_distance == target_value + 1) {
                                                break;
                                        }
                                }
                                // If there are no tiles in this direction, continue
                                if (current_distance == 0) {
                                        continue;
                                }
                                // handle the case where we stopped at an edge
                                if (current_distance <= target_value) {
                                        vars[current_distance++] = RED_const;
                                }
                                dir[n_valid_directions] = Problem_create_vars(p, 1, current_distance);
                                struct Constraint * how_many_visible = Problem_create_empty_constraints(p, 1);
                                ConstraintVisibility_init(how_many_visible, vars, current_distance, dir[n_valid_directions]);
                                n_valid_directions++;
                                // Remember
                                tile_data[i].constraints[tile_data[i].n_constraints++] = how_many_visible;
                        }
                        struct Constraint * sum = Problem_create_empty_constraints(p, 1);
                        ConstraintSum_init(sum, 1<<target_value, dir, n_valid_directions);
                        // Remember
                        tile_data[i].constraints[tile_data[i].n_constraints++] = sum;
                }
        } else {
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
                        }
                        struct Constraint * tile = Problem_create_empty_constraints(p, 1);
                        ConstraintTile_init(tile, target_value, vars, current_distances);
                        // Remember
                        tile_data[i].constraints[tile_data[i].n_constraints++] = tile;
                }
        }
        free(vars);

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
struct ProblemData * Board_pdata(struct Board * board)
{
        return board->private;
}

//////////
// Board
//////////
// Lua functions:
//   -> serialize()
//   -> deserialize()
double Board_reduce(struct Board * board, unsigned batch_size)
{
        struct ProblemData * pdata = board->private;
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
                        pdata->n_empty++;
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
        board->private = NULL;
        board->undo_stack = NULL;
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
                if (board->private) {
                        PData_destroy(board->private);
                }
                free(board);
        }
}

void Board_init_problem(struct Board * board, int difficulty)
{
        board->private = PData_create(board->min_grid, difficulty);
}
unsigned Board_maxify(struct Board * board, unsigned max_tile)
{
        if (!board) {
                goto no_board;
        }

        maxify(board->max_grid, max_tile);
        Grid_copy_to(board->max_grid, board->min_grid);

        return NO_FAILURE;
no_board:
        return FAIL_PARAM;
}

struct Hint Board_get_hint(struct Board * board)
{
        struct ProblemData * pdata = Board_pdata(board);
        struct Hint ret = (struct Hint){NULL, -1, EMPTY};
        if (pdata->mistakes != NULL) {
                ret.tile = pdata->mistakes->data;
                ret.id = ret.tile->id;
                ret.type = board->max_grid->tiles[ret.id].type;
        } else {
                // There are no mistakes so invoke the solver
                // Set the problem to the current board state
                struct Problem * p = pdata->problem;
                struct QueueSet_void_ptr * Q = p->Q;
                for (unsigned i = 0; i < board->length; i++) {
                        Problem_var_reset_domain(pdata->problem,
                                                 pdata->tile_data[i].var,
                                                 t2bits(&board->min_grid->tiles[i]));
                }
                for (unsigned c_i = 0; c_i < p->n_constraints; c_i++) {
                        struct ConstraintRegister * c = &p->c_registry[c_i];
                        if (c->active == 0) {
                                continue;
                        }
                        QueueSet_insert_void_ptr(Q, c->constraint);
                }
                int fail = NO_FAILURE;
                while (Q->n_entries != 0) {
                        // Pop from Queue
                        struct Constraint * c = NULL;
                        fail = QueueSet_pop_void_ptr(Q, (void**)&c);
                        NOFAIL(fail);
                        if ( ! P_cons_is_active(p, c)) {
                                continue;
                        }
                        struct LNode * restrictions = NULL;
                        fail = Constraint_filter(c, &restrictions);
                        NOFAIL(fail);

                        while (restrictions) {
                                struct Restriction * r = LNode_pop(&restrictions);

                                // POTENTIAL FIXME: noncompliant
                                ptrdiff_t v_i = (r->var - pdata->tile_data[0].var);
                                if (v_i >= 0 && v_i < board->length) {
                                        ret.tile = &board->min_grid->tiles[v_i];
                                        ret.id = (int)v_i;
                                        ret.type = board->max_grid->tiles[ret.id].type;
                                        free(r);
                                        LNode_destroy_and_free_data(&restrictions);
                                        break;
                                }
                                // assert(! HashSet_contains_void_ptr(pdata->vars_set, arc.var));
                                // assert(HashSet_contains_void_ptr(pdata->vars_set, pdata->tile_data[0].var));

                                fail = Problem_add_DAG_node(p, r);
                                NOFAIL(fail);
                                assert(r->var->domain);
                                Problem_enqueue_related_constraints(p, r->var);
                        }
                }
        }
        return ret;
}



int Board_get_x(struct Board * board, int index)
{
        return index % board->width;
}
int Board_get_y(struct Board * board, int index)
{
        return index / board->width;
}
struct Tile * Board_get_tile(struct Board * board, int index)
{
        return &board->min_grid->tiles[index];
}
void Board_set_tile(struct Board * board, struct Tile * tile, int state)
{
        int index = tile->id;
// Update the empty tile counter
        if (EMPTY == tile->type && EMPTY != state) {
                Board_pdata(board)->n_empty--;
        }
        if (EMPTY != tile->type && EMPTY == state) {
                Board_pdata(board)->n_empty++;
        }

// Actually set the tile
        int2tile(state, tile);

// Modify the list of mistakes as necessary
        int correct_state = board->max_grid->tiles[index].type;
        if ((state == FILLED && correct_state == WALL) ||
            (state == WALL && correct_state == NUMBER)) {
                LNode_prepend(&Board_pdata(board)->mistakes, tile, index);
        } else {
                // Make sure this node is not in the list of wrong tiles
                LNode_remove_node(&Board_pdata(board)->mistakes, tile);
        }
}

CSError Board_push_change(struct Board * board, struct Tile * tile, int state)
{
// Push this action onto the undo stack
        struct UndoAction * action = malloc(sizeof(struct UndoAction));
        if (!action) { goto bad_alloc1; }

        *action = (struct UndoAction){.tile = tile, .old_type = tile->type, .new_type = state};

        LNode_prepend((struct LNode **)&board->undo_stack, action, -1);

        Board_set_tile(board, tile, state);
        return NO_FAILURE;
bad_alloc1:
        return FAIL_ALLOC;
}
void Board_pop_change(struct Board * board)
{
        struct UndoAction * action = LNode_pop((struct LNode **)&board->undo_stack);
        if (!action) {
                return;
        }

        Board_set_tile(board, action->tile, action->old_type);
        free(action);
}

int Board_click(struct Board * board, int x, int y, int button)
{
        int index = y * board->width + x;
        struct Tile * tile = Board_get_tile(board, index);
        int left_click = (1 == button);
        if (1 == board->min_tile_mask[index]) {
                return 0;
        }
        int desired_state = EMPTY;
        switch (tile->type) {
        case EMPTY:
                desired_state = left_click ? FILLED : WALL;
                break;
        case FILLED:
                desired_state = left_click ? WALL : EMPTY;
                break;
        case WALL:
                desired_state = left_click ? EMPTY : FILLED;
                break;
        default:
                printf("%i\n", tile->type);
                assert(0);
                break;
        }
        Board_push_change(board, tile, desired_state);

        return 1;
}
int Board_get_mistake(struct Board * board)
{
        if (Board_pdata(board)->mistakes) {
                return Board_pdata(board)->mistakes->integer;
        } else {
                return -1;
        }
}
int Board_is_solved(struct Board * board)
{
        return (Board_get_mistake(board) == -1) && (Board_pdata(board)->n_empty == 0);
}

void Board_print(struct Board * board)
{
        printf("??? %u %u\n", board->width, board->height);
        Grid_print(board->min_grid);
}

struct bin_tile {
        int min_value;
        Type min_type;
        int max_value;
        Type max_type;
        int mask;
};

unsigned Board_write(struct Board * board, unsigned n_seconds)
{
        if (!board) {
                goto bad_input;
        }
        FILE *fp = NULL;
        size_t n_written = 0;
        const unsigned header[4] = {board->length, board->width, board->height, n_seconds};

        struct bin_tile * tiles = malloc(board->length * sizeof(struct bin_tile));
        if (!tiles) {
                goto bad_alloc1;
        }

        for (unsigned i = 0; i < board->length; i++) {
                tiles[i] = (struct bin_tile){
                        board->min_grid->tiles[i].value,
                        board->min_grid->tiles[i].type,
                        board->max_grid->tiles[i].value,
                        board->max_grid->tiles[i].type,
                        board->min_tile_mask[i]
                };
        }

        fp = fopen(SAVEFILE_NAME, "wb");

        if (!fp) {
                goto cannot_open;
        }

        n_written = fwrite(header, sizeof(unsigned), 4, fp);
        if (n_written != 4) {
                goto cannot_write;
        }

        n_written = fwrite(tiles, sizeof(struct bin_tile), board->length, fp); // trusted file
        if (n_written != board->length) {
                goto cannot_write;
        }

        fclose(fp);
        free(tiles);
        return NO_FAILURE;

cannot_write:
        fclose(fp);
cannot_open:
        free(tiles);
bad_alloc1:
bad_input:
        return FAILURE;
}

struct Board * Board_read(unsigned * n_seconds)
{
        FILE *fp = NULL;
        struct Board * board = NULL;
        size_t n_read = 0;
        unsigned header[4];
        struct bin_tile * tiles = NULL;

        fp = fopen(SAVEFILE_NAME, "rb");

        if (!fp) {
                goto cannot_open;
        }

        n_read = fread(header, sizeof(unsigned), 4, fp);
        if (n_read != 4) {
                goto cannot_read_header;
        }
        unsigned length = header[0],
                 width  = header[1],
                 height = header[2];
        if (n_seconds) {
                *n_seconds = header[3];
        }

        tiles = malloc(length * sizeof(struct bin_tile));
        if (!tiles) {
                goto bad_alloc1;
        }
        n_read = fread(tiles, sizeof(struct bin_tile), length, fp);
        if (n_read != length) {
                goto cannot_read_tiles;
        }
        board = Board_create(width, height);
        if (!board) {
                goto bad_alloc2;
        }
        CSError fail = Board_allocate_grids(board, width, height);
        if (fail) {
                goto bad_alloc3;
        }

        for (unsigned i = 0; i < board->length; i++) {
                board->min_grid->tiles[i].value = tiles[i].min_value;
                board->min_grid->tiles[i].type  = tiles[i].min_type;
                board->max_grid->tiles[i].value = tiles[i].max_value;
                board->max_grid->tiles[i].type  = tiles[i].max_type;
                board->min_tile_mask[i]         = tiles[i].mask;
        }

        Board_init_problem(board, HARD);

        for (unsigned i = 0; i < board->length; i++) {
                if ((tiles[i].min_type == FILLED && tiles[i].max_type == WALL) ||
                    (tiles[i].min_type == WALL && tiles[i].max_type == NUMBER)) {
                        LNode_prepend(&Board_pdata(board)->mistakes, Board_get_tile(board, i), i);
                }
        }

        free(tiles);
        fclose(fp);
        return board;

bad_alloc3:
        Board_destroy(board);
bad_alloc2:
cannot_read_tiles:
        free(tiles);
bad_alloc1:
cannot_read_header:
        fclose(fp);
cannot_open:
        return NULL;
}

// Emscripten helper functions

// TODO: validate/rebuild numbering
struct Board * Board_create_from_full_array(unsigned width,
                                            unsigned height,
                                            uint8_t * exportvalues) // Using Martin's encoding
{
        unsigned length = width * height;
        struct Board * board = Board_create(width, height);
        if (!board) {
                goto bad_alloc1;
        }
        for (unsigned i = 0; i < length; i++) {
                switch (exportvalues[i]) {
                case 0:
                        goto array_contains_empty;
                case 1:
                        int2tile(WALL, &board->max_grid->tiles[i]);
                        break;
                case 2:
                        goto array_contains_filled;
                default:
                        int2tile(exportvalues[i] - 2, &board->max_grid->tiles[i]);
                        break;
                }
        }
        // Grid_copy_to(board->max_grid, board->min_grid);
        return board;

array_contains_filled:
array_contains_empty:
        Board_destroy(board);
bad_alloc1:
        return NULL;
}
int tile2exportvalue(struct Tile * t)
{
        switch (t->type) {
        case EMPTY:
                return 0;
        case WALL:
                return 1;
        case FILLED:
                return 2;
        default:
                return t->value + 2;
        }
}
int Board_get_full_tile(struct Board * board, unsigned tile_i)
{
        return tile2exportvalue(&board->max_grid->tiles[tile_i]);
}
int Board_get_reduced_tile(struct Board * board, unsigned tile_i)
{
        return tile2exportvalue(&board->min_grid->tiles[tile_i]);
}
