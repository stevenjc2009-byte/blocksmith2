#ifndef BLOCKTEX_H
#define BLOCKTEX_H

#include <stdint.h>

/* The block textures, drawn in code (kickoff F-4): one 64 x 64 picture
 * holding a 4 x 4 grid of 16 x 16 tiles. Tile i sits in column i % 4 and
 * row i / 4, counting rows from the top. */
#define BLOCKTEX_SIZE 64
#define BLOCKTEX_TILE 16
#define BLOCKTEX_PER_ROW (BLOCKTEX_SIZE / BLOCKTEX_TILE)

/* Texture coordinates are pulled this far inside each tile so a block
 * never shows a sliver of the tile next to it. */
#define BLOCKTEX_INSET (1.0f / 1024.0f)

enum {
    TILE_GRASS_TOP,
    TILE_GRASS_SIDE,
    TILE_DIRT,
    TILE_STONE,
    TILE_COUNT
};

/* Draw the whole picture into atlas (BLOCKTEX_SIZE * BLOCKTEX_SIZE texels,
 * row by row from the top, 0xRRGGBBAA). Always draws the same picture.
 * Unused tiles are a loud magenta and black check, so a wrong tile shows. */
void blocktex_build(uint32_t *atlas);

/* Texture coordinates of a tile: uv[0] = left u, uv[1] = bottom v,
 * uv[2] = right u, uv[3] = top v (v = 0 is the bottom of the picture). */
void blocktex_tile_uv(int tile, float uv[4]);

#endif
