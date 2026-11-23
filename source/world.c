#include "world.h"

#include <stddef.h>

#include "block.h"
#include "terrain.h"
#include "worldgen.h"

void world_init(World *w, uint32_t seed)
{
    int i, j;

    w->seed = seed;
    w->store = NULL;
    w->lost = 0;
    w->torch_edits = 0;
    for (j = 0; j < WORLD_SLOTS; j++)
        for (i = 0; i < WORLD_SLOTS; i++) {
            w->slots[j][i].loaded = false;
            w->slots[j][i].changed = false;
        }
}

void world_set_store(World *w, const WorldStore *store)
{
    w->store = store;
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

/* Hand a changed loaded chunk to the store; true if it was kept (or had
 * nothing to keep). */
static bool keep(World *w, WorldSlot *s)
{
    if (!s->loaded || !s->changed)
        return true;
    if (w->store == NULL || !w->store->save(w->store->ctx, &s->chunk, s->cx, s->cz))
        return false;
    s->changed = false;
    return true;
}

/* Put away whatever chunk is in slot s. */
static void put_away(World *w, WorldSlot *s)
{
    if (!keep(w, s))
        w->lost++;
    s->loaded = false;
    s->changed = false;
}

void world_load(World *w, int cx, int cz)
{
    WorldSlot *s = slot_of(w, cx, cz);

    put_away(w, s);
    if (w->store == NULL || !w->store->load(w->store->ctx, &s->chunk, cx, cz))
        world_gen_chunk(&s->chunk, w->seed, cx, cz);
    else
        terrain_fill_biomes(&s->chunk, w->seed, cx, cz); /* saves keep blocks only */
    s->cx = cx;
    s->cz = cz;
    s->loaded = true;
}

void world_unload(World *w, int cx, int cz)
{
    WorldSlot *s = slot_of(w, cx, cz);

    if (s->loaded && s->cx == cx && s->cz == cz)
        put_away(w, s);
}

int world_save_changed(World *w)
{
    int i, j, failed = 0;

    for (j = 0; j < WORLD_SLOTS; j++)
        for (i = 0; i < WORLD_SLOTS; i++)
            failed += !keep(w, &w->slots[j][i]);
    return failed;
}

bool world_chunk_changed(const World *w, int cx, int cz)
{
    const WorldSlot *s = slot_of(w, cx, cz);

    return s->loaded && s->cx == cx && s->cz == cz && s->changed;
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
    int cx = block_to_chunk(x), cz = block_to_chunk(z);
    int lx = block_to_local(x), lz = block_to_local(z);
    Chunk *c = (Chunk *)world_chunk(w, cx, cz);
    int before;

    if (c == NULL)
        return;
    before = chunk_get(c, lx, y, lz);
    chunk_set(c, lx, y, lz, block);
    if (chunk_get(c, lx, y, lz) != before) {
        slot_of(w, cx, cz)->changed = true;
        if (before == BLOCK_TORCH || block == BLOCK_TORCH)
            w->torch_edits++;
    }
}

bool world_break(World *w, int x, int y, int z)
{
    int b = world_get(w, x, y, z);

    if (y <= 0 || b == BLOCK_AIR || b == BLOCK_WATER)
        return false;
    world_set(w, x, y, z, BLOCK_AIR);
    if (world_get(w, x, y + 1, z) == BLOCK_TORCH)
        world_set(w, x, y + 1, z, BLOCK_AIR);
    return true;
}

static int add_chunk(const World *w, int out[WORLD_TOUCHED_MAX][2], int n, int cx, int cz)
{
    if (!world_chunk(w, cx, cz))
        return n;
    out[n][0] = cx;
    out[n][1] = cz;
    return n + 1;
}

int world_touched_chunks(const World *w, int x, int z, int out[WORLD_TOUCHED_MAX][2])
{
    int cx = block_to_chunk(x), cz = block_to_chunk(z);
    int lx = block_to_local(x), lz = block_to_local(z);
    int dx = lx == 0 ? -1 : lx == CHUNK_SIZE - 1 ? 1 : 0;
    int dz = lz == 0 ? -1 : lz == CHUNK_SIZE - 1 ? 1 : 0;
    int n;

    if (!world_chunk(w, cx, cz))
        return 0;
    n = add_chunk(w, out, 0, cx, cz);
    if (dx != 0)
        n = add_chunk(w, out, n, cx + dx, cz);
    if (dz != 0)
        n = add_chunk(w, out, n, cx, cz + dz);
    if (dx != 0 && dz != 0)
        n = add_chunk(w, out, n, cx + dx, cz + dz);
    return n;
}

void world_neighbours(const World *w, int cx, int cz, const Chunk *nb[NB_COUNT])
{
    nb[NB_POS_X] = world_chunk(w, cx + 1, cz);
    nb[NB_NEG_X] = world_chunk(w, cx - 1, cz);
    nb[NB_POS_Z] = world_chunk(w, cx, cz + 1);
    nb[NB_NEG_Z] = world_chunk(w, cx, cz - 1);
    nb[NB_POS_X_POS_Z] = world_chunk(w, cx + 1, cz + 1);
    nb[NB_NEG_X_POS_Z] = world_chunk(w, cx - 1, cz + 1);
    nb[NB_POS_X_NEG_Z] = world_chunk(w, cx + 1, cz - 1);
    nb[NB_NEG_X_NEG_Z] = world_chunk(w, cx - 1, cz - 1);
}
