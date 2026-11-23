#ifndef WORLD_H
#define WORLD_H

#include <stdbool.h>
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

/* Break the block at (x, y, z): it turns to air. False, and nothing
 * changes, for air, for y 0 (the floor of the world stays, so nobody can
 * dig out of it) and for spots outside the world. */
bool world_break(World *w, int x, int y, int z);

/* The chunks whose meshes change when the block at (x, any y, z) changes:
 * its own chunk, then the chunk across each side of it that the block
 * touches (a block on a chunk's edge shows or hides a face drawn by that
 * neighbour). Writes up to 3 chunk (cx, cz) pairs to out and returns how
 * many; none for a spot outside the world, and no neighbour past the
 * world's edge. */
int world_touched_chunks(int x, int z, int out[3][2]);

/* The four chunks next to chunk (cx, cz), in NB_* order (chunk_mesh.h),
 * NULL where there is none, ready for chunk_mesh_build(). */
void world_neighbours(const World *w, int cx, int cz, const Chunk *nb[NB_COUNT]);

#endif
