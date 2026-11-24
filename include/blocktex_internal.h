#ifndef BLOCKTEX_INTERNAL_H
#define BLOCKTEX_INTERNAL_H

/* Shared by the files that draw the block tiles (source/blocktex.c and
 * source/blocktex_village.c) and by nothing else. */

#include <stdint.h>

#include "blocktex.h"

/* Every texel's wobble comes from a hash of where it is, so the picture is
 * the same on every run and every machine. */
static inline uint32_t hash3(uint32_t x, uint32_t y, uint32_t seed)
{
    uint32_t h = x * 0x8DA6B343u ^ y * 0xD8163841u ^ seed * 0xCB1AB31Fu;

    h ^= h >> 13;
    h *= 0x5BD1E995u;
    h ^= h >> 15;
    return h;
}

/* A whole number from -amount to +amount. */
static inline int wobble(uint32_t h, int amount)
{
    return (int)(h % (uint32_t)(2 * amount + 1)) - amount;
}

static inline int clamp255(int v)
{
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

static inline uint32_t rgb(int r, int g, int b)
{
    return ((uint32_t)clamp255(r) << 24) | ((uint32_t)clamp255(g) << 16) |
           ((uint32_t)clamp255(b) << 8) | 0xFF;
}

/* The row 4-05 tiles, in source/blocktex_village.c: (x, y) in 0..15. */
uint32_t blocktex_planks_texel(int x, int y);
uint32_t blocktex_log_side_texel(int x, int y);
uint32_t blocktex_log_top_texel(int x, int y);
uint32_t blocktex_cobble_texel(int x, int y);
uint32_t blocktex_mossy_cobble_texel(int x, int y);

/* The row 4-03 tiles, in source/blocktex_trees.c. */
uint32_t blocktex_tree_texel(int tile, int x, int y); /* TILE_BIRCH_LOG_SIDE .. TILE_DEAD_BUSH */

#endif
