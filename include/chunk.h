#ifndef CHUNK_H
#define CHUNK_H

#include <stdint.h>

#include "world_coords.h"

/* One chunk: a column of CHUNK_SIZE x WORLD_HEIGHT x CHUNK_SIZE blocks
 * (16 x 128 x 16, 32 KB), and the biome of each of its columns (256 bytes
 * more; row 4-02). Positions inside it are local: x and z 0..15, y 0..127.
 * Pure C, so the PC tests can run it.
 *
 * The biomes are not saved with the blocks: terrain_fill_chunk() and
 * terrain_fill_biomes() (terrain.h) fill them, from the world's seed. Only
 * the mesh's grass tint reads them. */

typedef struct {
    uint8_t blocks[WORLD_HEIGHT][CHUNK_SIZE][CHUNK_SIZE]; /* [y][z][x], BLOCK_* */
    uint8_t biome[CHUNK_SIZE][CHUNK_SIZE];                /* [z][x], BIOME_* (biome.h) */
} Chunk;

/* Every block air, every biome 0 (BIOME_PLAINS). */
void chunk_clear(Chunk *c);

/* The block at a local position; BLOCK_AIR outside the chunk. */
int chunk_get(const Chunk *c, int x, int y, int z);

/* Set the block at a local position. Positions outside the chunk and values
 * that are not a BLOCK_* are ignored. */
void chunk_set(Chunk *c, int x, int y, int z, int block);

#endif
