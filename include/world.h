#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>

#include "chunk.h"
#include "chunk_mesh.h"

/* A small fixed world (roadmap row 1-06): WORLD_CHUNKS x WORLD_CHUNKS
 * chunks around the origin, chunk x and z from WORLD_CHUNK_MIN to
 * WORLD_CHUNK_MIN + WORLD_CHUNKS - 1, so blocks -32 to 31 on x and z.
 * The endless world that loads chunks as you walk comes in milestone 2.
 * Pure C, so the PC tests can run it. */

#define WORLD_CHUNKS 4
#define WORLD_CHUNK_MIN (-WORLD_CHUNKS / 2)

typedef struct {
    Chunk chunks[WORLD_CHUNKS][WORLD_CHUNKS]; /* [z][x], from WORLD_CHUNK_MIN */
    uint32_t seed;
} World;

/* Make every chunk's ground from the seed (terrain.h). */
void world_generate(World *w, uint32_t seed);

/* Chunk (cx, cz), or NULL if it is not in the world. */
const Chunk *world_chunk(const World *w, int cx, int cz);

/* The block at world position (x, y, z); BLOCK_AIR outside the world. */
int world_get(const World *w, int x, int y, int z);

/* Set the block at world (x, y, z); spots outside the world are ignored. */
void world_set(World *w, int x, int y, int z, int block);

/* The four chunks next to chunk (cx, cz), in NB_* order (chunk_mesh.h),
 * NULL where there is none, ready for chunk_mesh_build(). */
void world_neighbours(const World *w, int cx, int cz, const Chunk *nb[NB_COUNT]);

#endif
