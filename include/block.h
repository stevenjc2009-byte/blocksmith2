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
    /* Row 4-02 (biomes). Ids are stored in saved worlds: only ever add at the end. */
    BLOCK_SNOWY_GRASS,       /* snow on top, dirt sides with a white fringe */
    BLOCK_SNOW,              /* a whole block of snow: mountain tops, cold beaches */
    BLOCK_ICE,               /* solid and opaque for now: frozen seas and rivers */
    BLOCK_SANDSTONE,         /* under desert sand */
    BLOCK_RED_SAND,          /* badlands */
    BLOCK_TERRACOTTA,        /* badlands bands: plain orange-brown... */
    BLOCK_TERRACOTTA_WHITE,  /* ...pale... */
    BLOCK_TERRACOTTA_YELLOW, /* ...yellow... */
    BLOCK_TERRACOTTA_RED,    /* ...red... */
    BLOCK_TERRACOTTA_BROWN,  /* ...and dark brown */
    BLOCK_GRAVEL,            /* cold sea floors */
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
 * hides the faces of the blocks beside it and stops light? Every block is
 * except air, a torch and water (and anything not a block). */
bool block_solid(int block);

#endif
