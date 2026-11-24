#ifndef WORLDGEN_H
#define WORLDGEN_H

#include <stdint.h>

#include "chunk.h"

/* Make a new chunk (cx, cz) from the seed: the ground (terrain.h), the trees
 * and plants on it (trees.h, row 4-03), then the
 * villages and ruins that reach it (structure.h, roadmap row 4-05). What
 * world_load() calls for a chunk that was never saved. */
void world_gen_chunk(Chunk *c, uint32_t seed, int cx, int cz);

#endif
