#ifndef WORLD_H
#define WORLD_H

#include <stdbool.h>
#include <stdint.h>

#include "chunk.h"
#include "chunk_mesh.h"

/* The endless world (roadmap row 2-01, kickoff answer W-1). Pure C, so the
 * PC tests can run it.
 *
 * It holds up to WORLD_SLOTS x WORLD_SLOTS chunks at once (13 x 13, 5.5 MB).
 * Each chunk lives in the slot its position picks: slot
 * [world_slot(cz)][world_slot(cx)], the chunk position's remainder after
 * dividing by WORLD_SLOTS, rounded down for negative chunks. Any WORLD_SLOTS
 * chunks in a row on x (or on z) land in different slots, so the square of
 * chunks round the player always fits; loading a chunk puts away whatever
 * chunk was in its slot. stream.h decides which chunks are loaded.
 *
 * A chunk that is not loaded reads as air and ignores writes.
 *
 * Saving (roadmap row 2-04): a world can be given a store (save.h hands it
 * one). Then a chunk being loaded comes from the store if it was saved
 * there, else from the seed; and a chunk that was changed since it was
 * loaded or last kept goes to the store when it is put away, or when
 * world_save_changed() is called. With no store, changes to a chunk are
 * lost when it is put away. */

#define WORLD_SLOTS 13

/* Where changed chunks are kept. load fills c with chunk (cx, cz) as it was
 * saved and returns true, or returns false if it never was (the chunk is
 * then made from the seed); save keeps changed chunk (cx, cz) and returns
 * false if it could not. Both get ctx back. */
typedef struct {
    bool (*load)(void *ctx, Chunk *c, int cx, int cz);
    bool (*save)(void *ctx, const Chunk *c, int cx, int cz);
    void *ctx;
} WorldStore;

typedef struct {
    Chunk chunk;
    int cx, cz;   /* which chunk is in the slot, when loaded */
    bool loaded;
    bool changed; /* a block changed since it was loaded or last kept */
} WorldSlot;

struct World;

/* Row 4-04: who is told when the world changes (water_flow.h wakes the
 * water round a change and finds the flowing water in a chunk as it
 * loads). set is called after world_set() really changed block (x, y, z);
 * load after world_load() loaded chunk (cx, cz). Either may be NULL. Both
 * get ctx back. */
typedef struct {
    void (*set)(void *ctx, const struct World *w, int x, int y, int z);
    void (*load)(void *ctx, const struct World *w, int cx, int cz);
    void *ctx;
} WorldWatch;

typedef struct World {
    WorldSlot slots[WORLD_SLOTS][WORLD_SLOTS]; /* [z][x], see world_slot() */
    uint32_t seed;
    const WorldStore *store; /* NULL: none */
    int lost; /* changed chunks put away that the store could not keep */
    unsigned torch_edits; /* world_set() calls that put or took away a torch
                             (row 3-03: meshes.c relights round them) */
    const WorldWatch *watch; /* NULL: nobody (row 4-04) */
} World;

/* An empty world: nothing loaded yet, no store. Its ground comes from the
 * seed. */
void world_init(World *w, uint32_t seed);

/* Give the world a store (NULL: none), which must stay valid while the
 * world uses it. Give it before loading any chunk. */
void world_set_store(World *w, const WorldStore *store);

/* Give the world a watcher (NULL: none, as world_init() leaves it), which
 * must stay valid while the world uses it (row 4-04). */
void world_set_watch(World *w, const WorldWatch *watch);

/* The slot index (0 to WORLD_SLOTS - 1) for chunk position c, on x or z. */
int world_slot(int c);

/* Load chunk (cx, cz) into its slot, putting away the chunk that was there:
 * from the store if it was saved there, else its ground from the seed
 * (terrain.h). */
void world_load(World *w, int cx, int cz);

/* Put chunk (cx, cz) away, if it is loaded: if it was changed, it goes to
 * the store first (w->lost counts one that could not be kept). */
void world_unload(World *w, int cx, int cz);

/* Hand every loaded chunk changed since it was loaded or last kept to the
 * store. Returns how many could not be kept (all of them with no store);
 * those stay marked changed. */
int world_save_changed(World *w);

/* Is chunk (cx, cz) loaded and changed since it was loaded or last kept? */
bool world_chunk_changed(const World *w, int cx, int cz);

/* Chunk (cx, cz), or NULL if it is not loaded. */
const Chunk *world_chunk(const World *w, int cx, int cz);

/* The block at world position (x, y, z); BLOCK_AIR outside a loaded chunk. */
int world_get(const World *w, int x, int y, int z);

/* Set the block at world (x, y, z); spots outside a loaded chunk are
 * ignored. A block that really changes marks its chunk changed, and counts
 * in w->torch_edits when it was or becomes a torch. */
void world_set(World *w, int x, int y, int z, int block);

/* Break the block at (x, y, z): it turns to air, and so does a torch
 * standing on it (row 3-03: a torch needs a solid block under it). False,
 * and nothing changes, for air, for water of any kind (rows 4-01, 4-04,
 * block_is_water()), for y 0 (the floor
 * of the world stays, so nobody can dig out of it) and for spots outside a
 * loaded chunk. */
bool world_break(World *w, int x, int y, int z);

/* Most chunks world_touched_chunks() lists. */
#define WORLD_TOUCHED_MAX 4

/* The chunks whose meshes change when the block at (x, any y, z) changes:
 * its own chunk, then the chunk across each side of it that the block
 * touches (a block on a chunk's edge shows or hides a face drawn by that
 * neighbour, and shades its corners), then, for a block on a chunk's
 * corner, the chunk across that corner (whose corner shadows it reaches,
 * row 3-01). Writes up to WORLD_TOUCHED_MAX chunk (cx, cz) pairs to out and
 * returns how many; none when the block's own chunk is not loaded, and no
 * neighbour that is not loaded. */
int world_touched_chunks(const World *w, int x, int z, int out[WORLD_TOUCHED_MAX][2]);

/* The eight chunks round chunk (cx, cz), in NB_* order (chunk_mesh.h),
 * NULL where one is not loaded, ready for chunk_mesh_build(). */
void world_neighbours(const World *w, int cx, int cz, const Chunk *nb[NB_COUNT]);

#endif
