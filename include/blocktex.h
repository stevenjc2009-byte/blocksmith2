#ifndef BLOCKTEX_H
#define BLOCKTEX_H

#include <stdint.h>

/* The block textures, the sun and moon (row 3-02) and the clouds' white
 * (row 3-04), drawn in code
 * (kickoff F-4): one 64 x 64 picture holding a 4 x 4 grid of 16 x 16
 * tiles. Tile i sits in column i % 4 and row i / 4, counting rows from the
 * top. */
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
    TILE_SUN, /* the sun and moon in the sky (sky.h) */
    TILE_MOON,
    TILE_TORCH, /* row 3-03: the torch's stick and flame in columns 7 and 8, rows 6 to
                   15 (TORCH_* in mesh.h); the rest is the hotbar's empty-slot grey */
    TILE_CLOUD, /* row 3-04: plain white, for the clouds (cloud.h) */
    TILE_COUNT
};

/* Where the torch's stick is in its tile, in texels from the tile's top
 * left (x1 and y1 one past the end): columns 7 and 8, rows 6 to 15, the
 * flame in rows 6 and 7 and the ember in row 8 (BLOCKTEX_TORCH_FLAME_Y1). */
#define BLOCKTEX_TORCH_X0 7
#define BLOCKTEX_TORCH_X1 9
#define BLOCKTEX_TORCH_Y0 6
#define BLOCKTEX_TORCH_Y1 16
#define BLOCKTEX_TORCH_FLAME_Y1 8

/* Draw the whole picture into atlas (BLOCKTEX_SIZE * BLOCKTEX_SIZE texels,
 * row by row from the top, 0xRRGGBBAA). Always draws the same picture.
 * Unused tiles are a loud magenta and black check, so a wrong tile shows. */
void blocktex_build(uint32_t *atlas);

/* Texture coordinates of a tile: uv[0] = left u, uv[1] = bottom v,
 * uv[2] = right u, uv[3] = top v (v = 0 is the bottom of the picture). */
void blocktex_tile_uv(int tile, float uv[4]);

/* The same for part of a tile: texels x0 .. x1 - 1 across and y0 .. y1 - 1
 * down from the tile's top left, pulled in by BLOCKTEX_INSET like a whole
 * tile, so a nearest-texel sample never lands outside the part. */
void blocktex_sub_uv(int tile, int x0, int y0, int x1, int y1, float uv[4]);

#endif
