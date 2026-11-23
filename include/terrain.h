#ifndef TERRAIN_H
#define TERRAIN_H

#include <stdint.h>

#include "chunk.h"

/* Gentle hills (roadmap row 1-06). Pure C with whole numbers only, so the
 * PC tests and the 3DS make exactly the same ground from the same seed.
 *
 * The ground height is two layers of smooth noise added together: big
 * hills up to 8 blocks high, 32 blocks apart, and small bumps up to 4 high,
 * 16 apart. Next-door columns never differ by more than one block, so the
 * player can walk anywhere by stepping up one block, with no jump (row 1-07).
 *
 * Each column: grass on top, TERRAIN_DIRT layers of dirt under it, stone
 * down to y 0. */

#define TERRAIN_BASE 60                 /* lowest possible grass block */
#define TERRAIN_TOP (TERRAIN_BASE + 11) /* highest possible grass block */
#define TERRAIN_DIRT 3

/* y of the grass block at world column (x, z). Coordinates up to about a
 * million blocks from the origin either way. */
int terrain_height(uint32_t seed, int x, int z);

/* Fill chunk (cx, cz), which holds world blocks cx * CHUNK_SIZE onwards on
 * x and cz * CHUNK_SIZE onwards on z. Everything above the ground is air. */
void terrain_fill_chunk(Chunk *c, uint32_t seed, int cx, int cz);

#endif
