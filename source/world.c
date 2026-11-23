#include "world.h"

#include <stddef.h>

#include "block.h"
#include "terrain.h"

void world_init(World *w, uint32_t seed)
{
    int i, j;

    w->seed = seed;
    for (j = 0; j < WORLD_SLOTS; j++)
        for (i = 0; i < WORLD_SLOTS; i++)
            w->slots[j][i].loaded = false;
}

int world_slot(int c)
{
    int r = c % WORLD_SLOTS;

    return r < 0 ? r + WORLD_SLOTS : r;
}

static WorldSlot *slot_of(const World *w, int cx, int cz)
{
    return (WorldSlot *)&w->slots[world_slot(cz)][world_slot(cx)];
}

void world_load(World *w, int cx, int cz)
{
    WorldSlot *s = slot_of(w, cx, cz);

    terrain_fill_chunk(&s->chunk, w->seed, cx, cz);
    s->cx = cx;
    s->cz = cz;
    s->loaded = true;
}

void world_unload(World *w, int cx, int cz)
{
    WorldSlot *s = slot_of(w, cx, cz);

    if (s->loaded && s->cx == cx && s->cz == cz)
        s->loaded = false;
}

const Chunk *world_chunk(const World *w, int cx, int cz)
{
    const WorldSlot *s = slot_of(w, cx, cz);

    return s->loaded && s->cx == cx && s->cz == cz ? &s->chunk : NULL;
}

int world_get(const World *w, int x, int y, int z)
{
    const Chunk *c = world_chunk(w, block_to_chunk(x), block_to_chunk(z));

    return c ? chunk_get(c, block_to_local(x), y, block_to_local(z)) : BLOCK_AIR;
}

void world_set(World *w, int x, int y, int z, int block)
{
    Chunk *c = (Chunk *)world_chunk(w, block_to_chunk(x), block_to_chunk(z));

    if (c)
        chunk_set(c, block_to_local(x), y, block_to_local(z), block);
}

bool world_break(World *w, int x, int y, int z)
{
    if (y <= 0 || world_get(w, x, y, z) == BLOCK_AIR)
        return false;
    world_set(w, x, y, z, BLOCK_AIR);
    return true;
}

static int add_chunk(const World *w, int out[3][2], int n, int cx, int cz)
{
    if (!world_chunk(w, cx, cz))
        return n;
    out[n][0] = cx;
    out[n][1] = cz;
    return n + 1;
}

int world_touched_chunks(const World *w, int x, int z, int out[3][2])
{
    int cx = block_to_chunk(x), cz = block_to_chunk(z);
    int lx = block_to_local(x), lz = block_to_local(z);
    int n;

    if (!world_chunk(w, cx, cz))
        return 0;
    n = add_chunk(w, out, 0, cx, cz);
    if (lx == 0)
        n = add_chunk(w, out, n, cx - 1, cz);
    else if (lx == CHUNK_SIZE - 1)
        n = add_chunk(w, out, n, cx + 1, cz);
    if (lz == 0)
        n = add_chunk(w, out, n, cx, cz - 1);
    else if (lz == CHUNK_SIZE - 1)
        n = add_chunk(w, out, n, cx, cz + 1);
    return n;
}

void world_neighbours(const World *w, int cx, int cz, const Chunk *nb[NB_COUNT])
{
    nb[NB_POS_X] = world_chunk(w, cx + 1, cz);
    nb[NB_NEG_X] = world_chunk(w, cx - 1, cz);
    nb[NB_POS_Z] = world_chunk(w, cx, cz + 1);
    nb[NB_NEG_Z] = world_chunk(w, cx, cz - 1);
}
