#ifndef BLOCK_H
#define BLOCK_H

#include <stdbool.h>

/* Block kinds and which texture tile each face of a block uses. */

enum {
    BLOCK_AIR,
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_TORCH, /* row 3-03: a small stick that lights the cells round it (light.h) */
    BLOCK_SAND,  /* row 4-01: beaches, river beds and the sea floor */
    BLOCK_WATER, /* row 4-01: still water in seas and rivers; not solid, see-through */
    BLOCK_COUNT
};

/* The six faces of a block, by the way they face. */
enum {
    FACE_POS_X,
    FACE_NEG_X,
    FACE_POS_Y, /* top */
    FACE_NEG_Y, /* bottom */
    FACE_POS_Z,
    FACE_NEG_Z,
    FACE_COUNT
};

/* The TILE_* (blocktex.h) for one face of a block, or -1 for air or a
 * value out of range. */
int block_face_tile(int block, int face);

/* Is the block a solid cube: something the player cannot walk through, that
 * hides the faces of the blocks beside it and stops light? Grass, dirt,
 * stone and sand are; air, a torch, water (and anything not a block) are not. */
bool block_solid(int block);

#endif
