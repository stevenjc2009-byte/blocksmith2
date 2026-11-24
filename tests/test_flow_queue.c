#include <stdio.h>
#include <string.h>

#include "block.h"
#include "chunk.h"
#include "test.h"
#include "water_flow.h"
#include "water_rules.h"
#include "world.h"

/* Row 4-04: the water queue (water_flow.h): what wakes water, the caps on
 * one update, the queue's table and a full queue (generated water and
 * saved water: test_flow_world.c). */

#define FLOOR 10
#define AT (FLOOR + 1)

static World s_w;
static WaterFlow s_f;

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

/* Changes to the world wake the water beside them; nothing else does. */
static void test_wakes(void)
{
    test_world_cleared(&s_w);
    floor_world(&s_w);
    world_set(&s_w, 0, AT, 0, BLOCK_WATER);
    world_set(&s_w, 2, AT, 0, BLOCK_WATER);
    water_flow_init(&s_f);
    water_flow_attach(&s_f, &s_w);
    CHECK_EQ_INT(s_f.count, 0);
    world_set(&s_w, 0, AT, 5, BLOCK_STONE); /* nowhere near water */
    CHECK_EQ_INT(s_f.count, 0);
    world_set(&s_w, -1, AT, 0, BLOCK_STONE); /* beside one source */
    CHECK_EQ_INT(s_f.count, 1);
    world_set(&s_w, -1, AT, 0, BLOCK_STONE); /* no change: no wake */
    CHECK_EQ_INT(s_f.count, 1);
    world_set(&s_w, 1, AT, 0, BLOCK_SAND); /* between both: one new */
    CHECK_EQ_INT(s_f.count, 2);
    CHECK_EQ_INT(world_break(&s_w, 2, FLOOR, 0), 1); /* under one: already waiting */
    CHECK_EQ_INT(s_f.count, 2);
    world_set(&s_w, 5, AT, 5, BLOCK_FLOW_3); /* water put down wakes itself */
    CHECK_EQ_INT(s_f.count, 3);
    world_set_watch(&s_w, NULL); /* nobody told: nothing wakes */
    world_set(&s_w, 10, AT, 10, BLOCK_WATER);
    world_set(&s_w, 11, AT, 10, BLOCK_STONE);
    CHECK_EQ_INT(s_f.count, 3);
    water_flow_attach(&s_f, &s_w);
    world_set(&s_w, 11, AT, 10, BLOCK_AIR);
    CHECK_EQ_INT(s_f.count, 4);
}

/* water_flow_found() queues the flowing and falling water of a chunk,
 * never its sources. */
static void test_found(void)
{
    Chunk *c;

    test_world_cleared(&s_w);
    c = test_slot_chunk(&s_w, 0, 0);
    chunk_set(c, 0, 0, 0, BLOCK_FLOW_1);
    chunk_set(c, 15, WORLD_HEIGHT - 1, 15, BLOCK_FALL);
    chunk_set(c, 7, 40, 3, BLOCK_FLOW_7);
    chunk_set(c, 8, 40, 3, BLOCK_WATER);
    chunk_set(c, 9, 40, 3, BLOCK_WATER);
    water_flow_init(&s_f);
    water_flow_found(&s_f, &s_w, 0, 0);
    CHECK_EQ_INT(s_f.count, 3);
    water_flow_found(&s_f, &s_w, 1, 0); /* all air */
    CHECK_EQ_INT(s_f.count, 3);
    water_flow_found(&s_f, &s_w, 9, 9); /* not loaded */
    CHECK_EQ_INT(s_f.count, 3);
}

/* Lone level-7 cells, each drying to air on its turn, two apart. */
static int lone_cells(int n, int y0)
{
    int i, made = 0;

    for (i = 0; i < n; i++) {
        int x = -16 + 2 * (i % 16), z = -16 + 2 * (i / 16 % 16), y = y0 + 2 * (i / 256);

        world_set(&s_w, x, y, z, BLOCK_FLOW_7);
        made += water_flow_add(&s_f, x, y, z);
    }
    return made;
}

/* One update works at most WATER_FLOW_CAP cells and looks at most
 * WATER_FLOW_LOOKS. */
static void test_caps(void)
{
    int total, calls, worst = 0, x, z, y, waiting = 0;

    test_world_cleared(&s_w);
    water_flow_init(&s_f);
    CHECK_EQ_INT(lone_cells(500, 20), 500);
    water_flow_attach(&s_f, &s_w);
    CHECK_EQ_INT(s_f.count, 500);
    total = water_flow_update(&s_f, &s_w, WATER_FLOW_STEP);
    CHECK_EQ_INT(total, WATER_FLOW_CAP);
    CHECK_EQ_INT(s_f.count, 500 - WATER_FLOW_CAP);
    for (calls = 1; water_flow_due(&s_f) && calls < 100; calls++) {
        int n = water_flow_update(&s_f, &s_w, 0.0f);

        worst = n > worst ? n : worst;
        total += n;
    }
    CHECK_EQ_INT(total, 500);
    CHECK_EQ_INT(calls, (500 + WATER_FLOW_CAP - 1) / WATER_FLOW_CAP);
    CHECK_EQ_INT(worst, WATER_FLOW_CAP);
    CHECK_EQ_INT(world_get(&s_w, -16, 20, -16), BLOCK_AIR);

    /* Cells near ground not loaded wait, looked at once a step. */
    test_world_cleared(&s_w);
    water_flow_init(&s_f);
    for (y = 20; y < 36; y += 2) /* apart, so each only dries */
        for (z = -20; z < 20; z++) {
            x = 29; /* chunk 2 (from x 32) is not loaded */
            world_set(&s_w, x, y, z, BLOCK_FLOW_7);
            waiting += water_flow_add(&s_f, x, y, z);
        }
    CHECK_EQ_INT(waiting, 320);
    water_flow_attach(&s_f, &s_w);
    CHECK_EQ_INT(water_flow_update(&s_f, &s_w, WATER_FLOW_STEP), 0);
    CHECK_EQ_INT(s_f.looked, WATER_FLOW_LOOKS);
    CHECK_EQ_INT(s_f.count, 320);
    CHECK_EQ_INT(water_flow_due(&s_f), 1); /* 64 not looked at yet */
    CHECK_EQ_INT(water_flow_update(&s_f, &s_w, 0.0f), 0);
    CHECK_EQ_INT(s_f.looked, 320 - WATER_FLOW_LOOKS);
    CHECK_EQ_INT(water_flow_due(&s_f), 0);
    CHECK_EQ_INT(world_get(&s_w, 29, 20, 0), BLOCK_FLOW_7);
    for (z = SMALL_MIN; z < SMALL_MIN + SMALL_CHUNKS; z++)
        world_load(&s_w, 2, z);
    total = 0;
    for (calls = 0; calls < 20 && s_f.count > 0; calls++)
        total += water_flow_update(&s_f, &s_w, WATER_FLOW_STEP);
    CHECK_EQ_INT(total, 320);
    CHECK_EQ_INT(world_get(&s_w, 29, 20, 0), BLOCK_AIR);

    /* A cell in a chunk not loaded is let go. */
    CHECK_EQ_INT(water_flow_add(&s_f, 100, 20, 0), 1);
    CHECK_EQ_INT(water_flow_update(&s_f, &s_w, WATER_FLOW_STEP), 0);
    CHECK_EQ_INT(s_f.count, 0);
}

static unsigned s_rng;

static int rnd(int n)
{
    s_rng = s_rng * 1103515245u + 12345u;
    return (int)((s_rng >> 16) % (unsigned)n);
}

/* Each cell waits once; the table forgets a cell when it has its turn. */
static void test_table(void)
{
    static bool s_in[16][16][16];
    static int s_at[1500][3];
    int i, distinct = 0, calls, empty = 0, ok = 0;

    test_world_cleared(&s_w);
    water_flow_init(&s_f);
    for (i = 0; i < 10; i++)
        CHECK_EQ_INT(water_flow_add(&s_f, 1, 2, 3), 1);
    CHECK_EQ_INT(s_f.count, 1);
    water_flow_add(&s_f, 1, 2, 4);
    CHECK_EQ_INT(s_f.count, 2);
    water_flow_update(&s_f, &s_w, WATER_FLOW_STEP); /* air: nothing to do */
    CHECK_EQ_INT(s_f.count, 0);
    CHECK_EQ_INT(water_flow_add(&s_f, 1, 2, 3), 1);
    CHECK_EQ_INT(s_f.count, 1);

    /* Many cells crowded into a small box, so the table's slots collide. */
    water_flow_init(&s_f);
    memset(s_in, 0, sizeof s_in);
    s_rng = 2026u;
    for (i = 0; i < 1500; i++) {
        int x = rnd(16), y = rnd(16), z = rnd(16);

        s_at[i][0] = x - 8;
        s_at[i][1] = y;
        s_at[i][2] = z - 8;
        distinct += !s_in[y][z][x];
        s_in[y][z][x] = true;
        ok += water_flow_add(&s_f, x - 8, y, z - 8);
    }
    CHECK_EQ_INT(ok, 1500);
    CHECK_EQ_INT(s_f.count, distinct);
    water_flow_update(&s_f, &s_w, WATER_FLOW_STEP);
    for (calls = 0; calls < 10; calls++)
        water_flow_update(&s_f, &s_w, 0.0f);
    CHECK_EQ_INT(s_f.count, distinct - 11 * WATER_FLOW_CAP);
    for (i = 0, ok = 0; i < 1500; i++)
        ok += water_flow_add(&s_f, s_at[i][0], s_at[i][1], s_at[i][2]);
    CHECK_EQ_INT(ok, 1500);
    CHECK_EQ_INT(s_f.count, distinct); /* the ones taken came back, no others twice */
    for (calls = 0; calls < 100 && s_f.count > 0; calls++) {
        water_flow_update(&s_f, &s_w, WATER_FLOW_STEP);
        while (water_flow_due(&s_f))
            water_flow_update(&s_f, &s_w, 0.0f);
    }
    CHECK_EQ_INT(s_f.count, 0);
    for (i = 0; i < WATER_FLOW_HASH; i++)
        empty += s_f.hash[i] == 0;
    CHECK_EQ_INT(empty, WATER_FLOW_HASH);
}

/* A full queue turns wakes away without losing any water. */
static void test_full(void)
{
    int x, z, i, ok = 0, sources = 0;

    test_world_cleared(&s_w);
    floor_world(&s_w);
    for (z = -1; z <= 3; z++) /* a pool of nine sources, walled in */
        for (x = -1; x <= 3; x++)
            world_set(&s_w, x, AT, z,
                      x < 0 || x > 2 || z < 0 || z > 2 ? BLOCK_STONE : BLOCK_WATER);
    water_flow_init(&s_f);
    water_flow_attach(&s_f, &s_w);
    for (i = 0; i < WATER_FLOW_QUEUE; i++)
        ok += water_flow_add(&s_f, -16 + i % 32, 30 + i / 1024, -16 + i / 32 % 32);
    CHECK_EQ_INT(ok, WATER_FLOW_QUEUE);
    CHECK_EQ_INT(water_flow_add(&s_f, 0, 40, 0), 0);
    CHECK_EQ_INT((int)s_f.dropped, 1);
    CHECK_EQ_INT(water_flow_add(&s_f, -16, 30, -16), 1); /* already waiting */
    CHECK_EQ_INT((int)s_f.dropped, 1);
    CHECK_EQ_INT(world_break(&s_w, 3, AT, 1), 1); /* the wake for the pool is turned away */
    CHECK_EQ_INT((int)s_f.dropped, 2);
    CHECK_EQ_INT(settle(&s_f, &s_w, 100) >= 0, 1);
    for (z = 0; z <= 2; z++)
        for (x = 0; x <= 2; x++)
            sources += world_get(&s_w, x, AT, z) == BLOCK_WATER;
    CHECK_EQ_INT(sources, 9);
    CHECK_EQ_INT(world_get(&s_w, 3, AT, 1), BLOCK_AIR); /* waits to be woken again */
    water_flow_wake(&s_f, &s_w, 3, AT, 1);
    step(&s_f, &s_w);
    CHECK_EQ_INT(world_get(&s_w, 3, AT, 1), BLOCK_FLOW_1);
}

void test_flow_queue(void)
{
    test_wakes();
    test_found();
    test_caps();
    test_table();
    test_full();
}
