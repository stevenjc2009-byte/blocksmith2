#include <stdio.h>
#include <string.h>

#include "block.h"
#include "chunk.h"
#include "test.h"
#include "water_flow.h"
#include "water_rules.h"
#include "world.h"

/* Row 4-04, the water queue (water_flow.h) in whole worlds: generated
 * water staying still, and water saved while it ran carrying on when
 * loaded. */

#define FLOOR 10
#define AT (FLOOR + 1)

static World s_w, s_w2;
static WaterFlow s_f, s_f2;

static void floor_world(World *w)
{
    int x, y, z;

    for (y = 0; y <= FLOOR; y++)
        for (z = -20; z < 20; z++)
            for (x = -20; x < 20; x++)
                world_set(w, x, y, z, BLOCK_STONE);
}

/* One whole step of f in w. */
static void step(WaterFlow *f, World *w)
{
    int guard = 0;

    water_flow_update(f, w, WATER_FLOW_STEP);
    while (water_flow_due(f) && guard++ < 1000)
        water_flow_update(f, w, 0.0f);
}

/* Steps until nothing waits; how many, or -1 if it never stopped. */
static int settle(WaterFlow *f, World *w, int most)
{
    int n;

    for (n = 0; n < most && f->count > 0; n++)
        step(f, w);
    return f->count > 0 ? -1 : n;
}

/* Generated seas and rivers: nothing is queued as they load, and no water
 * cell, if woken, would change. */
static void test_generated_still(void)
{
    static const uint32_t k_seeds[] = { 2026u, 7u, 12345u, 4242u, 31337u, 99u };
    long checked = 0, unstable = 0;
    size_t si;
    int cx, cz, x, y, z;

    for (si = 0; si < sizeof k_seeds / sizeof k_seeds[0]; si++) {
        world_init(&s_w, k_seeds[si]);
        water_flow_init(&s_f);
        water_flow_attach(&s_f, &s_w);
        for (cz = -6; cz <= 6; cz++)
            for (cx = -6; cx <= 6; cx++)
                world_load(&s_w, cx, cz);
        CHECK_EQ_INT(s_f.count, 0);
        for (cz = -6; cz <= 6; cz++)
            for (cx = -6; cx <= 6; cx++) {
                const Chunk *c = world_chunk(&s_w, cx, cz);

                for (y = 0; y < WORLD_HEIGHT; y++)
                    for (z = 0; z < CHUNK_SIZE; z++)
                        for (x = 0; x < CHUNK_SIZE; x++) {
                            int wx = cx * CHUNK_SIZE + x, wz = cz * CHUNK_SIZE + z;

                            if (!block_is_water(c->blocks[y][z][x]) ||
                                !water_rules_ready(&s_w, wx, wz))
                                continue;
                            checked++;
                            if (water_rules_would_change(&s_w, wx, y, wz) && unstable++ == 0)
                                printf("FAIL: seed %u water at (%d, %d, %d) would move\n",
                                       (unsigned)k_seeds[si], wx, y, wz);
                        }
            }
    }
    CHECK_EQ_INT((int)unstable, 0);
    CHECK_EQ_INT(checked > 100000, 1);
    printf("     (still water: %ld generated water cells checked)\n", checked);
}

/* An in-memory store for test_resume(). */
typedef struct {
    int n, cx[16], cz[16];
    Chunk c[16];
} Mem;

static Mem s_mem;

static bool mem_load(void *ctx, Chunk *c, int cx, int cz)
{
    Mem *m = ctx;
    int i;

    for (i = 0; i < m->n; i++)
        if (m->cx[i] == cx && m->cz[i] == cz) {
            *c = m->c[i];
            return true;
        }
    return false;
}

static bool mem_save(void *ctx, const Chunk *c, int cx, int cz)
{
    Mem *m = ctx;
    int i;

    for (i = 0; i < m->n && (m->cx[i] != cx || m->cz[i] != cz); i++)
        ;
    if (i == 16)
        return false;
    m->n += i == m->n;
    m->cx[i] = cx;
    m->cz[i] = cz;
    m->c[i] = *c;
    return true;
}

static int flowing_in(const World *w)
{
    int x, y, z, n = 0;

    for (y = 0; y < WORLD_HEIGHT; y++)
        for (z = -32; z < 32; z++)
            for (x = -32; x < 32; x++) {
                int b = world_get(w, x, y, z);

                n += block_is_water(b) && b != BLOCK_WATER;
            }
    return n;
}

/* Water saved while it ran carries on when its chunks load again, and
 * ends as it would have. */
static void test_resume(void)
{
    WorldStore store = { mem_load, mem_save, &s_mem };
    int cx, cz, x, y, z, differ = 0, moving;

    memset(&s_mem, 0, sizeof s_mem);
    world_init(&s_w, 0u);
    world_set_store(&s_w, &store);
    for (cz = SMALL_MIN; cz < SMALL_MIN + SMALL_CHUNKS; cz++)
        for (cx = SMALL_MIN; cx < SMALL_MIN + SMALL_CHUNKS; cx++) {
            world_load(&s_w, cx, cz);
            chunk_clear(test_slot_chunk(&s_w, cx, cz));
            s_w.slots[world_slot(cz)][world_slot(cx)].changed = true;
        }
    floor_world(&s_w);
    for (z = -9; z <= 9; z++)
        for (x = -9; x <= 9; x++)
            if (x == -9 || x == 9 || z == -9 || z == 9)
                world_set(&s_w, x, AT, z, BLOCK_STONE);
    world_set(&s_w, 4, FLOOR, 2, BLOCK_AIR); /* a pit, two deep */
    world_set(&s_w, 4, FLOOR - 1, 2, BLOCK_AIR);
    world_set(&s_w, 0, AT, 0, BLOCK_WATER);
    water_flow_init(&s_f);
    water_flow_attach(&s_f, &s_w);
    water_flow_wake(&s_f, &s_w, 0, AT, 0);
    step(&s_f, &s_w);
    step(&s_f, &s_w);
    step(&s_f, &s_w);
    moving = flowing_in(&s_w);
    CHECK_EQ_INT(moving > 0 && s_f.count > 0, 1);
    CHECK_EQ_INT(world_save_changed(&s_w), 0);

    world_init(&s_w2, 0u);
    world_set_store(&s_w2, &store);
    water_flow_init(&s_f2);
    water_flow_attach(&s_f2, &s_w2);
    for (cz = SMALL_MIN; cz < SMALL_MIN + SMALL_CHUNKS; cz++)
        for (cx = SMALL_MIN; cx < SMALL_MIN + SMALL_CHUNKS; cx++)
            world_load(&s_w2, cx, cz);
    CHECK_EQ_INT(s_f2.count, moving); /* every flowing cell found */
    CHECK_EQ_INT(settle(&s_f, &s_w, 200) >= 0, 1);
    CHECK_EQ_INT(settle(&s_f2, &s_w2, 200) >= 0, 1);
    for (y = FLOOR - 2; y <= AT + 1; y++)
        for (z = -9; z <= 9; z++)
            for (x = -9; x <= 9; x++)
                differ += world_get(&s_w, x, y, z) != world_get(&s_w2, x, y, z);
    CHECK_EQ_INT(differ, 0);
    CHECK_EQ_INT(world_get(&s_w2, 4, FLOOR - 1, 2), BLOCK_FALL); /* it reached the pit */
}

void test_flow_world(void)
{
    test_generated_still();
    test_resume();
}
