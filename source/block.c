#include "block.h"
#include "blocktex.h"

int block_face_tile(int block, int face)
{
    if (face < 0 || face >= FACE_COUNT)
        return -1;

    switch (block) {
    case BLOCK_GRASS:
        if (face == FACE_POS_Y)
            return TILE_GRASS_TOP;
        if (face == FACE_NEG_Y)
            return TILE_DIRT;
        return TILE_GRASS_SIDE;
    case BLOCK_DIRT:
        return TILE_DIRT;
    case BLOCK_STONE:
        return TILE_STONE;
    default:
        return -1; /* air, or not a block */
    }
}
