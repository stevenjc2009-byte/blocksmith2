#ifndef WORLD_H
#define WORLD_H

#include <stdbool.h>
#include <stdint.h>

#include "chunk.h"
#include "chunk_mesh.h"

/* The endless world (roadmap row 2-01, kickoff answer W-1). Pure C, so the
 * PC tests can run it.
 *
 * It holds up to WORLD_SLOTS x WORLD_SLOTS chunks at once (9 x 9, 2.6 MB).
 * Each chunk lives in the slot its position picks: slot
 * [world_slot(cz)][world_slot(cx)], the chunk position's remainder after
 * dividing by WORLD_SLOTS, rounded down for negative chunks. Any WORLD_SLOTS
 * chunks in a row on x (or on z) land in different slots, so the square of
 * chunks round the player always fits; loading a chunk puts away whatever
 * chunk was in its slot. stream.h decides which chunks are loaded.
 *
 * A chunk that is not loaded reads as air and ignores writes. */

#define WORLD_SLOTS 9

typedef struct {
    Chunk chunk;
    int cx, cz;  /* which chunk is in the slot, when loaded */
    bool loaded;
} WorldSlot;

typedef struct {
    WorldSlot slots[WORLD_SLOTS][WORLD_SLOTS]; /* [z][x], see world_slot() */
    uint32_t seed;
} World;

/* An empty world: nothing loaded yet. Its ground comes from the seed. */
void world_init(World *w, uint32_t seed);

/* The slot index (0 to WORLD_SLOTS - 1) for chunk position c, on x or z. */
int world_slot(int c);

/* Make chunk (cx, cz)'s ground from the seed (terrain.h) in its slot,
 * putting away the chunk that was there. Changes made to a chunk are lost
 * when it is put away (saving comes in roadmap row 2-04). */
void world_load(World *w, int cx, int cz);

/* Put chunk (cx, cz) away, if it is loaded. */
void world_unload(World *w, int cx, int cz);

/* Chunk (cx, cz), or NULL if it is not loaded. */
const Chunk *world_chunk(const World *w, int cx, int cz);

/* The block at world position (x, y, z); BLOCK_AIR outside a loaded chunk. */
int world_get(const World *w, int x, int y, int z);

/* Set the block at world (x, y, z); spots outside a loaded chunk are ignored. */
void world_set(World *w, int x, int y, int z, int block);

/* Break the block at (x, y, z): it turns to air. False, and nothing
 * changes, for air, for y 0 (the floor of the world stays, so nobody can
 * dig out of it) and for spots outside a loaded chunk. */
bool world_break(World *w, int x, int y, int z);

/* The chunks whose meshes change when the block at (x, any y, z) changes:
 * its own chunk, then the chunk across each side of it that the block
 * touches (a block on a chunk's edge shows or hides a face drawn by that
 * neighbour). Writes up to 3 chunk (cx, cz) pairs to out and returns how
 * many; none when the block's own chunk is not loaded, and no neighbour
 * that is not loaded. */
int world_touched_chunks(const World *w, int x, int z, int out[3][2]);

/* The four chunks next to chunk (cx, cz), in NB_* order (chunk_mesh.h),
 * NULL where one is not loaded, ready for chunk_mesh_build(). */
void world_neighbours(const World *w, int cx, int cz, const Chunk *nb[NB_COUNT]);

#endif
