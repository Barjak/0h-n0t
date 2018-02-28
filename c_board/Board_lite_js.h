#include <stdint.h>

typedef int8_t TileVal;
struct Board;

struct Board * Board_create(unsigned width, unsigned height);
unsigned Board_initialize(struct Board * board, unsigned max_value);
void Board_import_array(TileVal * exported_values);
double Board_reduce(struct Board * board, unsigned batch_size);
TileVal Board_get_full_tile(struct Board * board, unsigned index);
TileVal Board_get_reduced_tile(struct Board *board, unsigned index);
void Board_destroy(struct Board * board);
