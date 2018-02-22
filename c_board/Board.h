typedef struct { int x; int y; } Vector;
typedef enum { EMPTY = -3, WALL = -2, FILLED = -1, NUMBER = 0} Type;
typedef enum { NO_DIRECTION = -1, UP = 0, DOWN = 1, LEFT = 2, RIGHT = 3} Direction;
extern Vector Directions[4];

struct Tile {
        int id;
        int value;
        Type type;
        struct Tile * dir[4];
};

struct Grid {
        int width, height, length;
        void * Problem;
        struct Tile tiles[];
};

struct Hint {
        struct Tile * tile;
        int id;
        Type type;
};

struct Board {
        unsigned length, width, height;
        struct Grid * min_grid;
        struct Grid * max_grid;

        int * min_tile_mask;

        void * undo_stack;
        void * private;
};

int tile2int(struct Tile * t);
void int2tile(Type i, struct Tile * t);

struct Board * Board_create(unsigned width, unsigned height);
unsigned Board_initialize(struct Board * board, unsigned max_tile);
void Board_print(struct Board * board);
void Board_destroy(struct Board * board);
int Board_click(struct Board * board, int x, int y, int button);
double Board_reduce(struct Board * board, unsigned batch_size);

int Board_get_x(struct Board * board, int index);
int Board_get_y(struct Board * board, int index);
struct Tile * Board_get_tile(struct Board * board, int index);
void Board_init_solver(struct Board * board);
int Board_get_mistake(struct Board * board);
struct Hint Board_get_hint(struct Board * board);
int Board_is_solved(struct Board * board);
void Board_pop_change(struct Board * board);

unsigned Board_write(struct Board * board, unsigned n_seconds);
struct Board * Board_read(unsigned * n_seconds);
