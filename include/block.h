#ifndef BLOCK_H
#define BLOCK_H

/* Block kinds and which texture tile each face of a block uses. */

enum {
    BLOCK_AIR,
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
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

#endif
