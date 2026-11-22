#include "world_coords.h"

/* C division rounds toward zero, so -1 / 16 is 0. The world needs it to round
 * down (-1 is in chunk -1, at local 15), so negative blocks are handled here. */
int block_to_chunk(int b)
{
    if (b >= 0)
        return b / CHUNK_SIZE;
    return -((-(b + 1)) / CHUNK_SIZE) - 1;
}

int block_to_local(int b)
{
    return b - block_to_chunk(b) * CHUNK_SIZE;
}
