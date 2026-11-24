#include <string.h>

#include "block.h"
#include "blocktex.h"
#include "test.h"
#include "water.h"
#include "water_flow.h"
#include "world.h"

/* Row 4-04: the rules of flowing water (water_flow.h), each in a small
 * scene on a stone floor (y 0 to FLOOR, x and z -20 to 19) in the cleared
 * test world, and the water.h helpers they read. */

#define FLOOR 10
#define AT (FLOOR + 1) /* the layer on the floor */

static World s_w;
static WaterFlow s_f;
static int s_washed;         /* torches washed away, read from s_f.changed */
static int s_wx, s_wy, s_wz; /* where the last one was */
static int s_plants;         /* plants washed away (row 4-03) */

static void put(int x, int y, int z, int b)
{
    world_set(&s_w, x, y, z, b);
}

static int at(int x, int y, int z)
{
    return world_get(&s_w, x, y, z);
}

/* The floor, and an empty queue not yet attached (so building the scene
 * wakes nothing). */
static void scene(void)
{
    int x, y, z;

    test_world_cleared(&s_w);
    for (y = 0; y <= FLOOR; y++)
        for (z = -20; z < 20; z++)
            for (x = -20; x < 20; x++)
                put(x, y, z, BLOCK_STONE);
    water_flow_init(&s_f);
    s_washed = 0;
    s_plants = 0;
}

/* Hand the world to the queue and wake (x, y, z): the water starts. */
static void start(int x, int y, int z)
{
    water_flow_attach(&s_f, &s_w);
    water_flow_wake(&s_f, &s_w, x, y, z);
}

static void note_washed(void)
{
    int i;

    for (i = 0; i < s_f.changed_n; i++) {
        s_plants += s_f.changed[i].plant;
        if (s_f.changed[i].torch) {
            s_washed++;
            s_wx = s_f.changed[i].x;
            s_wy = s_f.changed[i].y;
            s_wz = s_f.changed[i].z;
        }
    }
}

/* One whole step: every cell whose turn has come has it. */
static void step(void)
{
    int guard = 0;

    water_flow_update(&s_f, &s_w, WATER_FLOW_STEP);
    note_washed();
    while (water_flow_due(&s_f) && guard++ < 1000) {
        water_flow_update(&s_f, &s_w, 0.0f);
        note_washed();
    }
}

static void steps(int n)
{
    while (n-- > 0)
        step();
}

/* A channel one cell wide on the floor, x 0 to 12 at z 0, stone round it
 * and across both ends. */
static void channel(void)
{
    int x, z;

    for (z = -1; z <= 1; z++)
        for (x = -1; x <= 13; x++)
            put(x, AT, z, z == 0 && x >= 0 && x <= 12 ? BLOCK_AIR : BLOCK_STONE);
}

static void test_water_helpers(void)
{
    static const int k_dry[] = { BLOCK_AIR, BLOCK_GRASS, BLOCK_DIRT, BLOCK_STONE, BLOCK_TORCH,
                                 BLOCK_SAND };
    uint8_t row[8];
    int n, b, i, wet = 0, bad = 0;

    CHECK_EQ_INT(BLOCK_FALL - BLOCK_FLOW_1, WATER_LEVEL_MAX); /* together, in order */
    CHECK_EQ_INT(water_level(BLOCK_WATER), 0);
    CHECK_EQ_INT(water_block(0), BLOCK_WATER);
    CHECK_EQ_INT(water_amount(BLOCK_WATER), 8);
    CHECK_NEAR(water_height(BLOCK_WATER), WATER_TOP, 1e-6);
    for (n = 1; n <= WATER_LEVEL_MAX; n++) {
        CHECK_EQ_INT(water_level(BLOCK_FLOW_1 + n - 1), n);
        CHECK_EQ_INT(water_block(n), BLOCK_FLOW_1 + n - 1);
        CHECK_EQ_INT(water_amount(BLOCK_FLOW_1 + n - 1), 8 - n);
        CHECK_NEAR(water_height(BLOCK_FLOW_1 + n - 1), (8 - n) / 9.0, 1e-6);
        CHECK_EQ_INT(block_solid(BLOCK_FLOW_1 + n - 1), 0);
        CHECK_EQ_INT(block_face_tile(BLOCK_FLOW_1 + n - 1, FACE_POS_Y), TILE_WATER);
    }
    CHECK_EQ_INT(water_level(BLOCK_FALL), WATER_FALLING);
    CHECK_EQ_INT(water_block(WATER_FALLING), BLOCK_FALL);
    CHECK_EQ_INT(water_amount(BLOCK_FALL), 8);
    CHECK_NEAR(water_height(BLOCK_FALL), 1.0, 1e-6);
    CHECK_EQ_INT(block_face_tile(BLOCK_FALL, FACE_NEG_X), TILE_WATER);
    CHECK_EQ_INT(water_block(WATER_FALLING + 1), BLOCK_AIR);
    CHECK_EQ_INT(water_block(-1), BLOCK_AIR);
    for (i = 0; i < (int)(sizeof k_dry / sizeof k_dry[0]); i++) {
        CHECK_EQ_INT(water_level(k_dry[i]), -1);
        CHECK_EQ_INT(water_amount(k_dry[i]), 0);
        CHECK_NEAR(water_height(k_dry[i]), 0.0, 1e-9);
        CHECK_EQ_INT(block_is_water(k_dry[i]), 0);
    }
    /* Every byte in every place of a row: water_row_has() is
     * block_is_water(), water_row_flowing() that but not a source. */
    for (b = 0; b < 256; b++) {
        wet += block_is_water(b);
        for (i = 0; i < 8; i++) {
            memset(row, BLOCK_STONE, sizeof row);
            row[i] = (uint8_t)b;
            bad += water_row_has(row, 8) != block_is_water(b);
            bad += water_row_flowing(row, 8) != (block_is_water(b) && b != BLOCK_WATER);
        }
    }
    CHECK_EQ_INT(wet, 1 + WATER_LEVEL_MAX + 1);
    CHECK_EQ_INT(bad, 0);
    memset(row, BLOCK_AIR, sizeof row);
    CHECK_EQ_INT(water_row_has(row, 8), 0);
    row[7] = BLOCK_WATER;
    CHECK_EQ_INT(water_row_has(row, 4), 0); /* only the first n bytes */
    CHECK_EQ_INT(water_row_has(row, 8), 1);
    CHECK_EQ_INT(water_row_flowing(row, 8), 0);
}

/* Water goes down before it goes sideways. */
static void test_down_first(void)
{
    scene();
    put(0, AT + 1, 0, BLOCK_WATER); /* a source with air under it */
    start(0, AT + 1, 0);
    step();
    CHECK_EQ_INT(at(0, AT, 0), BLOCK_FALL);
    CHECK_EQ_INT(at(1, AT + 1, 0), BLOCK_AIR);
    CHECK_EQ_INT(at(-1, AT + 1, 0), BLOCK_AIR);
    CHECK_EQ_INT(at(0, AT + 1, 1), BLOCK_AIR);
    CHECK_EQ_INT(at(0, AT + 1, -1), BLOCK_AIR);
    CHECK_EQ_INT(at(1, AT, 0), BLOCK_AIR); /* the falling water has not had its turn */
    step();
    /* Falling water on the floor runs out at level 1 (Minecraft's 7); the
     * source, unable to go down, spreads round itself too. */
    CHECK_EQ_INT(at(1, AT, 0), BLOCK_FLOW_1);
    CHECK_EQ_INT(at(-1, AT, 0), BLOCK_FLOW_1);
    CHECK_EQ_INT(at(0, AT, 1), BLOCK_FLOW_1);
    CHECK_EQ_INT(at(0, AT, -1), BLOCK_FLOW_1);
    CHECK_EQ_INT(at(1, AT + 1, 0), BLOCK_FLOW_1);
    CHECK_EQ_INT(at(0, AT + 1, -1), BLOCK_FLOW_1);
    step();
    /* Flowing water over water does not spread sideways, and water with
     * water above it falls. */
    CHECK_EQ_INT(at(2, AT + 1, 0), BLOCK_AIR);
    CHECK_EQ_INT(at(1, AT + 1, 0), BLOCK_FLOW_1);
    CHECK_EQ_INT(at(1, AT, 0), BLOCK_FALL);
    CHECK_EQ_INT(at(0, AT + 1, 0), BLOCK_WATER);
}

/* Each step on, one level thinner, and level 7 goes no further. */
static void test_levels(void)
{
    int k, x, guard;

    scene();
    channel();
    put(0, AT, 0, BLOCK_WATER);
    start(0, AT, 0);
    for (k = 1; k <= WATER_LEVEL_MAX; k++) {
        step();
        CHECK_EQ_INT(at(k, AT, 0), water_block(k));
        CHECK_EQ_INT(at(k + 1, AT, 0), BLOCK_AIR);
    }
    for (guard = 0; guard < 30 && s_f.count > 0; guard++)
        step();
    CHECK_EQ_INT(s_f.count, 0); /* all still again */
    CHECK_EQ_INT(at(0, AT, 0), BLOCK_WATER);
    for (x = 1; x <= 12; x++)
        CHECK_EQ_INT(at(x, AT, 0), x <= WATER_LEVEL_MAX ? water_block(x) : BLOCK_AIR);
}

/* A source on the floor at (0, AT, 0) with a hole dug at (hx, FLOOR, hz)
 * (two deep): after one step, which of its four sides got water (+x 1,
 * -x 2, +z 4, -z 8). */
static int sides_after_one_step(int hx, int hz, int hx2, int hz2)
{
    scene();
    put(hx, FLOOR, hz, BLOCK_AIR);
    put(hx, FLOOR - 1, hz, BLOCK_AIR);
    if (hx2 != hx || hz2 != hz) {
        put(hx2, FLOOR, hz2, BLOCK_AIR);
        put(hx2, FLOOR - 1, hz2, BLOCK_AIR);
    }
    put(0, AT, 0, BLOCK_WATER);
    start(0, AT, 0);
    step();
    return (at(1, AT, 0) == BLOCK_FLOW_1) | (at(-1, AT, 0) == BLOCK_FLOW_1) << 1 |
           (at(0, AT, 1) == BLOCK_FLOW_1) << 2 | (at(0, AT, -1) == BLOCK_FLOW_1) << 3;
}

/* Water runs only toward the nearest drop it can find within reach. */
static void test_drop(void)
{
    CHECK_EQ_INT(sides_after_one_step(3, 0, 3, 0), 1);   /* a drop 3 east: east only */
    CHECK_EQ_INT(sides_after_one_step(5, 0, 5, 0), 1);   /* 5 east: the furthest it looks */
    CHECK_EQ_INT(sides_after_one_step(6, 0, 6, 0), 15);  /* 6: too far, all four ways */
    CHECK_EQ_INT(sides_after_one_step(0, -4, 0, -4), 8); /* 4 toward -z */
    CHECK_EQ_INT(sides_after_one_step(2, 0, -2, 0), 3);  /* two as near: both */
    CHECK_EQ_INT(sides_after_one_step(2, 1, 2, 1), 5);   /* round the corner: east and +z */
    CHECK_EQ_INT(sides_after_one_step(1, 0, -3, 0), 1);  /* the nearer wins */
    /* Into the drop at 3: on along the floor, then down it. */
    sides_after_one_step(3, 0, 3, 0);
    step();
    CHECK_EQ_INT(at(2, AT, 0), BLOCK_FLOW_2);
    CHECK_EQ_INT(at(2, AT, 1), BLOCK_AIR);
    step();
    CHECK_EQ_INT(at(3, AT, 0), BLOCK_FLOW_3);
    step();
    CHECK_EQ_INT(at(3, FLOOR, 0), BLOCK_FALL);
    CHECK_EQ_INT(at(4, AT, 0), BLOCK_AIR); /* over a drop: no further sideways */
}

/* A cell between two sources becomes a source when the cell under it is
 * solid or a source (Minecraft's infinite water), but not over air. */
static void test_new_source(void)
{
    int guard;

    scene();
    channel();
    put(0, AT, 0, BLOCK_WATER);
    put(2, AT, 0, BLOCK_WATER);
    put(3, AT, 0, BLOCK_STONE);
    start(0, AT, 0);
    step();
    CHECK_EQ_INT(at(1, AT, 0), BLOCK_WATER);

    scene();
    channel();
    put(0, AT, 0, BLOCK_WATER);
    put(2, AT, 0, BLOCK_WATER);
    put(3, AT, 0, BLOCK_STONE);
    put(1, FLOOR, 0, BLOCK_AIR); /* over air: flowing water, not a source */
    start(0, AT, 0);
    step();
    CHECK_EQ_INT(at(1, AT, 0), BLOCK_FLOW_1);
    for (guard = 0; guard < 30 && s_f.count > 0; guard++)
        step();
    CHECK_EQ_INT(at(1, AT, 0), BLOCK_FLOW_1); /* over falling water, still not */
    CHECK_EQ_INT(at(1, FLOOR, 0), BLOCK_FALL);
    CHECK_EQ_INT(s_f.count, 0);

    scene();
    channel();
    put(0, AT, 0, BLOCK_WATER);
    put(2, AT, 0, BLOCK_WATER);
    put(3, AT, 0, BLOCK_STONE);
    put(1, FLOOR, 0, BLOCK_WATER); /* over a source: a source */
    start(0, AT, 0);
    step();
    CHECK_EQ_INT(at(1, AT, 0), BLOCK_WATER);
}

/* With its source gone, flowing water dries up. */
static void test_decay(void)
{
    int x, guard, wet = 0;

    scene();
    channel();
    put(0, AT, 0, BLOCK_WATER);
    start(0, AT, 0);
    steps(12);
    CHECK_EQ_INT(at(7, AT, 0), BLOCK_FLOW_7);
    put(0, AT, 0, BLOCK_STONE); /* wakes the water beside it */
    CHECK_EQ_INT(s_f.count, 1);
    step();
    CHECK_EQ_INT(at(1, AT, 0), BLOCK_FLOW_3); /* one less than its level-2 side */
    for (guard = 0; guard < 100 && s_f.count > 0; guard++)
        step();
    CHECK_EQ_INT(s_f.count, 0);
    CHECK_EQ_INT(guard < 60, 1);
    for (x = 1; x <= 12; x++)
        wet += at(x, AT, 0) != BLOCK_AIR;
    CHECK_EQ_INT(wet, 0);
}

/* Water washes a torch away, along the floor or falling on it. */
static void test_torch(void)
{
    unsigned edits;

    scene();
    channel();
    put(3, AT, 0, BLOCK_TORCH);
    put(0, AT, 0, BLOCK_WATER);
    edits = s_w.torch_edits;
    start(0, AT, 0);
    steps(2);
    CHECK_EQ_INT(at(3, AT, 0), BLOCK_TORCH);
    CHECK_EQ_INT(s_washed, 0);
    step();
    CHECK_EQ_INT(at(3, AT, 0), BLOCK_FLOW_3);
    CHECK_EQ_INT(s_washed, 1);
    CHECK_EQ_INT(s_wx * 10000 + s_wy * 100 + s_wz, 3 * 10000 + AT * 100);
    CHECK_EQ_INT((int)(s_w.torch_edits - edits), 1); /* relit as when broken */

    scene();
    put(0, AT, 0, BLOCK_TORCH);
    put(0, AT + 2, 0, BLOCK_WATER);
    start(0, AT + 2, 0);
    step();
    CHECK_EQ_INT(at(0, AT + 1, 0), BLOCK_FALL);
    CHECK_EQ_INT(at(0, AT, 0), BLOCK_TORCH);
    step();
    CHECK_EQ_INT(at(0, AT, 0), BLOCK_FALL);
    CHECK_EQ_INT(s_washed, 1);
}

/* Row 4-03: water washes a plant away like a torch, but no light changes. */
static void test_plant(void)
{
    unsigned edits;

    scene();
    channel();
    put(3, AT, 0, BLOCK_POPPY);
    put(5, AT, 0, BLOCK_SHORT_GRASS);
    put(0, AT, 0, BLOCK_WATER);
    edits = s_w.torch_edits;
    start(0, AT, 0);
    steps(2);
    CHECK_EQ_INT(at(3, AT, 0), BLOCK_POPPY);
    CHECK_EQ_INT(s_plants, 0);
    steps(3);
    CHECK_EQ_INT(at(3, AT, 0), BLOCK_FLOW_3);
    CHECK_EQ_INT(at(5, AT, 0), BLOCK_FLOW_5);
    CHECK_EQ_INT(s_plants, 2);
    CHECK_EQ_INT(s_washed, 0);
    CHECK_EQ_INT((int)(s_w.torch_edits - edits), 0);
}

void test_flow(void)
{
    test_water_helpers();
    test_down_first();
    test_levels();
    test_drop();
    test_new_source();
    test_decay();
    test_torch();
    test_plant();
}
