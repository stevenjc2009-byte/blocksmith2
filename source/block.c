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
    case BLOCK_TORCH:
        return TILE_TORCH;
    case BLOCK_SAND:
        return TILE_SAND;
    case BLOCK_WATER: /* its own mesh (water_mesh.h) */
        return TILE_WATER;
    case BLOCK_SNOWY_GRASS:
        if (face == FACE_POS_Y)
            return TILE_SNOW;
        if (face == FACE_NEG_Y)
            return TILE_DIRT;
        return TILE_SNOWY_GRASS_SIDE;
    case BLOCK_SNOW:
        return TILE_SNOW;
    case BLOCK_ICE:
        return TILE_ICE;
    case BLOCK_SANDSTONE:
        return face == FACE_POS_Y || face == FACE_NEG_Y ? TILE_SANDSTONE_TOP : TILE_SANDSTONE_SIDE;
    case BLOCK_RED_SAND:
        return TILE_RED_SAND;
    case BLOCK_TERRACOTTA:
        return TILE_TERRACOTTA;
    case BLOCK_TERRACOTTA_WHITE:
        return TILE_TERRACOTTA_WHITE;
    case BLOCK_TERRACOTTA_YELLOW:
        return TILE_TERRACOTTA_YELLOW;
    case BLOCK_TERRACOTTA_RED:
        return TILE_TERRACOTTA_RED;
    case BLOCK_TERRACOTTA_BROWN:
        return TILE_TERRACOTTA_BROWN;
    case BLOCK_GRAVEL:
        return TILE_GRAVEL;
    default:
        return -1; /* air, or not a block */
    }
}

bool block_solid(int block)
{
    return block == BLOCK_GRASS || block == BLOCK_DIRT || block == BLOCK_STONE ||
           block == BLOCK_SAND || (block >= BLOCK_SNOWY_GRASS && block < BLOCK_COUNT);
}
