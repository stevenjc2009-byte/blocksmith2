#ifndef STRUCTURE_H
#define STRUCTURE_H

#include <stdbool.h>
#include <stdint.h>

#include "chunk.h"

/* Villages and ruins (roadmap row 4-05). Pure C, whole numbers only, so the
 * PC tests and the 3DS build exactly the same ones from the same seed.
 *
 * How they are made chunk by chunk, seamlessly: the world is cut into
 * square REGIONS (villages: VILLAGE_REGION_CHUNKS chunks a side, ruins:
 * RUIN_REGION_CHUNKS), lined up with the chunk grid. Each region has at
 * most one structure, planned from the seed and the region's number alone
 * (village.h, ruins.h): where its middle is, every building, path and
 * stone, with the ground heights it was planned on. A structure never
 * reaches past its own region (its middle is kept a margin away from the
 * region's edge), so a chunk only ever needs the plan of the one village
 * region and the one ruin region it lies in. A plan is the same whenever
 * and however often it is made, so the order chunks are made in, and what
 * else is loaded, change nothing. Plans are kept in a small cache (they
 * cost a few thousand terrain columns to work out) which only saves time.
 *
 * Applying a plan to a chunk writes just the blocks of the plan that fall
 * inside it, over the ground terrain_fill_chunk() made. The same code
 * writes into a canvas of any size, which is how the tests build a whole
 * area at once to compare with the chunk-by-chunk result.
 *
 * Not for several threads at once: the plan cache is shared. The game makes
 * chunks only on its own thread (meshes_stream()). */

#define VILLAGE_REGION_CHUNKS 24
#define RUIN_REGION_CHUNKS 12

/* A box of blocks to write into: cells[(y * d + z) * w + x] is the block at
 * world (x0 + x, y, z0 + z), y from 0 to WORLD_HEIGHT - 1. A chunk's blocks
 * are one with w = d = CHUNK_SIZE. */
typedef struct {
    uint8_t *cells;
    int x0, z0, w, d;
} StructCanvas;

/* Set / read the block at a world position; positions outside the canvas
 * (or above or below the world) are ignored / read as air. */
void structure_canvas_set(const StructCanvas *cv, int x, int y, int z, int block);
int structure_canvas_get(const StructCanvas *cv, int x, int y, int z);

/* Put every village and ruin that reaches the canvas into it. */
void structure_apply_canvas(const StructCanvas *cv, uint32_t seed);

/* The same for chunk (cx, cz), after terrain_fill_chunk() made its ground. */
void structure_apply(Chunk *c, uint32_t seed, int cx, int cz);

/* Forget the cached plans (the tests do, to prove a cold cache gives the
 * same blocks). */
void structure_cache_clear(void);

/* The spot terrain_find_spawn() gives for the seed (worked out once and
 * kept: no building or ruin is put where the player appears). False if the
 * seed has none. */
bool structure_spawn(uint32_t seed, int *x, int *z);

/* a / b rounded down, for negative a too (b above 0). */
int structure_floor_div(int a, int b);

#endif
