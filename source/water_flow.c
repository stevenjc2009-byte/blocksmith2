#include <string.h>

#include "water_flow.h"

#include "block.h"
#include "chunk.h"
#include "water.h"
#include "water_rules.h"
#include "world_coords.h"

/* The ring and the table index with a mask; the table keeps queue index + 1
 * in a uint16_t, and is never more than half full, so a search always
 * meets an empty slot. */
_Static_assert((WATER_FLOW_QUEUE & (WATER_FLOW_QUEUE - 1)) == 0, "queue size a power of two");
_Static_assert((WATER_FLOW_HASH & (WATER_FLOW_HASH - 1)) == 0, "table size a power of two");
_Static_assert(WATER_FLOW_HASH >= 2 * WATER_FLOW_QUEUE && WATER_FLOW_QUEUE < 65535,
               "table room for every waiting cell");
_Static_assert(WATER_SPREAD_MAX + 1 <= WATER_FLOW_CHANGES / WATER_FLOW_CAP,
               "room to note every change of a full update");

#define QMASK (WATER_FLOW_QUEUE - 1)
#define HMASK (WATER_FLOW_HASH - 1)

static unsigned home(int x, int y, int z)
{
    uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ (uint32_t)z * 83492791u;

    h ^= h >> 15;
    return h & HMASK;
}

static bool same(const WaterFlowCell *c, int x, int y, int z)
{
    return c->x == x && c->y == y && c->z == z;
}

/* The table slot holding cell (x, y, z), or the empty slot where it would
 * go (*found false). */
static unsigned slot_of(const WaterFlow *f, int x, int y, int z, bool *found)
{
    unsigned s = home(x, y, z);

    while (f->hash[s] != 0) {
        if (same(&f->queue[f->hash[s] - 1], x, y, z)) {
            *found = true;
            return s;
        }
        s = (s + 1) & HMASK;
    }
    *found = false;
    return s;
}

/* Empty table slot s, moving later entries of the same run back into the
 * gap so every search still finds them (linear probing's deletion). */
static void unhash(WaterFlow *f, unsigned s)
{
    unsigned j = s;

    for (;;) {
        const WaterFlowCell *c;

        j = (j + 1) & HMASK;
        if (f->hash[j] == 0)
            break;
        c = &f->queue[f->hash[j] - 1];
        /* The entry at j may fill the gap at s if its home slot is not
         * between the gap and it. */
        if (((j - home(c->x, c->y, c->z)) & HMASK) >= ((j - s) & HMASK)) {
            f->hash[s] = f->hash[j];
            s = j;
        }
    }
    f->hash[s] = 0;
}

static bool push(WaterFlow *f, int x, int y, int z, uint32_t due)
{
    bool found;
    unsigned s = slot_of(f, x, y, z, &found);
    int i;

    if (found)
        return true; /* already waiting: keeps its place */
    if (f->count == WATER_FLOW_QUEUE) {
        f->dropped++;
        return false;
    }
    i = (f->head + f->count) & QMASK;
    f->queue[i].x = x;
    f->queue[i].y = (int16_t)y;
    f->queue[i].z = z;
    f->queue[i].due = due;
    f->hash[s] = (uint16_t)(i + 1);
    f->count++;
    return true;
}

/* Take the oldest waiting cell off the queue. */
static WaterFlowCell pop(WaterFlow *f)
{
    WaterFlowCell c = f->queue[f->head];
    bool found;
    unsigned s = slot_of(f, c.x, c.y, c.z, &found);

    if (found)
        unhash(f, s);
    f->head = (f->head + 1) & QMASK;
    f->count--;
    return c;
}

void water_flow_init(WaterFlow *f)
{
    memset(f, 0, sizeof *f);
}

static void on_set(void *ctx, const World *w, int x, int y, int z)
{
    water_flow_wake((WaterFlow *)ctx, w, x, y, z);
}

static void on_load(void *ctx, const World *w, int cx, int cz)
{
    water_flow_found((WaterFlow *)ctx, w, cx, cz);
}

void water_flow_attach(WaterFlow *f, World *w)
{
    f->watch.set = on_set;
    f->watch.load = on_load;
    f->watch.ctx = f;
    world_set_watch(w, &f->watch);
}

bool water_flow_add(WaterFlow *f, int x, int y, int z)
{
    return push(f, x, y, z, f->step + 1);
}

void water_flow_wake(WaterFlow *f, const World *w, int x, int y, int z)
{
    static const int k_round[7][3] = {
        { 0, 0, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 },
    };
    int i;

    for (i = 0; i < 7; i++) {
        int cx = x + k_round[i][0], cy = y + k_round[i][1], cz = z + k_round[i][2];

        if (block_is_water(world_get(w, cx, cy, cz)))
            water_flow_add(f, cx, cy, cz);
    }
}

void water_flow_found(WaterFlow *f, const World *w, int cx, int cz)
{
    const Chunk *c = world_chunk(w, cx, cz);
    int x, y, z;

    if (c == NULL)
        return;
    for (y = 0; y < WORLD_HEIGHT; y++) {
        if (!water_row_flowing(c->blocks[y][0], (int)sizeof c->blocks[y]))
            continue; /* most layers: sixteen 4-byte reads */
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++)
                if (water_level(c->blocks[y][z][x]) > 0)
                    water_flow_add(f, cx * CHUNK_SIZE + x, y, cz * CHUNK_SIZE + z);
    }
}

/* Make (x, y, z) block b, noting the change for the meshes. world_set()
 * wakes the cell and the water round it (water_flow_attach()). */
static void put(WaterFlow *f, World *w, int x, int y, int z, int b)
{
    int old = world_get(w, x, y, z);

    if (old == b)
        return;
    if (f->changed_n < WATER_FLOW_CHANGES) {
        WaterFlowChange *c = &f->changed[f->changed_n++];

        c->x = x;
        c->y = (int16_t)y;
        c->z = z;
        c->torch = old == BLOCK_TORCH;
        c->plant = block_is_plant(old);
    }
    world_set(w, x, y, z, b);
}

/* One cell's turn (water_flow.h). */
static void turn(WaterFlow *f, World *w, int x, int y, int z)
{
    WaterWrite out[WATER_SPREAD_MAX];
    int b = world_get(w, x, y, z), n, i;

    if (!block_is_water(b))
        return; /* gone since it was woken */
    if (b != BLOCK_WATER) {
        int now = water_rules_new(w, x, y, z);

        if (now != b) {
            put(f, w, x, y, z, now);
            if (now == BLOCK_AIR)
                return;
            b = now;
        }
    }
    n = water_rules_spread(w, x, y, z, b, out);
    for (i = 0; i < n; i++)
        put(f, w, out[i].x, out[i].y, out[i].z, out[i].b);
}

int water_flow_update(WaterFlow *f, World *w, float dt)
{
    f->worked = 0;
    f->looked = 0;
    f->changed_n = 0;
    f->time += dt;
    if (f->time >= WATER_FLOW_STEP) {
        f->step++;
        f->time -= WATER_FLOW_STEP;
        if (f->time > WATER_FLOW_STEP)
            f->time = WATER_FLOW_STEP; /* after a long pause, one step more, not all */
    }
    while (water_flow_due(f) && f->worked < WATER_FLOW_CAP && f->looked < WATER_FLOW_LOOKS) {
        WaterFlowCell c = pop(f);

        f->looked++;
        if (world_chunk(w, block_to_chunk(c.x), block_to_chunk(c.z)) == NULL)
            continue; /* its chunk went: found again if it comes back */
        if (!water_rules_ready(w, c.x, c.z)) {
            push(f, c.x, c.y, c.z, f->step + 1); /* wait for the chunks round it */
            continue;
        }
        f->worked++;
        turn(f, w, c.x, c.y, c.z);
    }
    return f->worked;
}

bool water_flow_due(const WaterFlow *f)
{
    return f->count > 0 && (int32_t)(f->queue[f->head].due - f->step) <= 0;
}
