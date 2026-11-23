#include "world.h"

#include <stddef.h>

#include "block.h"
#include "terrain.h"

void world_generate(World *w, uint32_t seed)
{
    int i, j;

    w->seed = seed;
    for (j = 0; j < WORLD_CHUNKS; j++)
        for (i = 0; i < WORLD_CHUNKS; i++)
            terrain_fill_chunk(&w->chunks[j][i], seed, WORLD_CHUNK_MIN + i, WORLD_CHUNK_MIN + j);
}

const Chunk *world_chunk(const World *w, int cx, int cz)
{
    int i = cx - WORLD_CHUNK_MIN;
    int j = cz - WORLD_CHUNK_MIN;

    if (i < 0 || i >= WORLD_CHUNKS || j < 0 || j >= WORLD_CHUNKS)
        return NULL;
    return &w->chunks[j][i];
}

int world_get(const World *w, int x, int y, int z)
{
    const Chunk *c = world_chunk(w, block_to_chunk(x), block_to_chunk(z));

    return c ? chunk_get(c, block_to_local(x), y, block_to_local(z)) : BLOCK_AIR;
}

void world_neighbours(const World *w, int cx, int cz, const Chunk *nb[NB_COUNT])
{
    nb[NB_POS_X] = world_chunk(w, cx + 1, cz);
    nb[NB_NEG_X] = world_chunk(w, cx - 1, cz);
    nb[NB_POS_Z] = world_chunk(w, cx, cz + 1);
    nb[NB_NEG_Z] = world_chunk(w, cx, cz - 1);
}
