#include <math.h>
#include <stdlib.h>

#include "block.h"
#include "blocktex.h"
#include "particles.h"
#include "place.h"
#include "player.h"
#include "raycast.h"
#include "test.h"
#include "water.h"

/* Row 4-01: water's rules (water.h): the aiming ray goes through it, a
 * placed block takes its place, it cannot be broken or placed; sand's
 * break bits; the eye under water and its fog's colour. */

static World s_w;

/* Empty world, stone from y 0 to 50 and water from 51 to 62 over x and z
 * -8 to 7. */
static void sea(void)
{
    int x, y, z;

    test_world_cleared(&s_w);
    for (x = -8; x < 8; x++)
        for (z = -8; z < 8; z++)
            for (y = 0; y <= 62; y++)
                world_set(&s_w, x, y, z, y <= 50 ? BLOCK_STONE : BLOCK_WATER);
}

static void check_hit(float ox, float oy, float oz, float dx, float dy, float dz, float reach,
                      int x, int y, int z, int face, float dist)
{
    RayHit h = { -999, -999, -999, -1, -1.0f };

    CHECK_EQ_INT(raycast(&s_w, ox, oy, oz, dx, dy, dz, reach, &h), 1);
    CHECK_EQ_INT(h.x * 10000 + h.y * 100 + h.z, x * 10000 + y * 100 + z);
    CHECK_EQ_INT(h.face, face);
    CHECK_NEAR(h.dist, dist, 1e-4);
}

static void test_ray(void)
{
    RayHit h;

    sea();
    /* Down from over the sea: through the water to the stone floor. */
    check_hit(0.5f, 65.62f, 0.5f, 0, -1, 0, 20.0f, 0, 50, 0, FACE_POS_Y, 14.62f);
    /* From inside it, the same. */
    check_hit(0.5f, 55.5f, 0.5f, 0, -1, 0, RAY_REACH, 0, 50, 0, FACE_POS_Y, 4.5f);
    /* Along it to a wall of stone. */
    world_set(&s_w, 3, 56, 0, BLOCK_STONE);
    check_hit(0.5f, 56.5f, 0.5f, 1, 0, 0, RAY_REACH, 3, 56, 0, FACE_NEG_X, 2.5f);
    /* Nothing but water in reach: nothing. */
    CHECK_EQ_INT(raycast(&s_w, 0.5f, 56.5f, 0.5f, 0, 0, -1, RAY_REACH, &h), 0);
}

static void test_place_and_break(void)
{
    Player far = { .x = 20.5f, .y = 64.0f, .z = 20.5f, .on_ground = true };
    Player in = { .x = 0.5f, .y = 51.0f, .z = 3.5f, .on_ground = true }; /* on the floor, in the water */
    RayHit h = { 0, 50, 0, FACE_POS_Y, 1.0f };
    int x, y, z;

    sea();
    /* A block goes into a water cell, in its place. */
    CHECK_EQ_INT(place_block(&s_w, &far, &h, BLOCK_DIRT, &x, &y, &z), 1);
    CHECK_EQ_INT(x * 10000 + y * 100 + z, 51 * 100);
    CHECK_EQ_INT(world_get(&s_w, 0, 51, 0), BLOCK_DIRT);
    h.x = 1; /* sand too, but not a torch: no torch goes under water */
    CHECK_EQ_INT(place_block(&s_w, &far, &h, BLOCK_SAND, &x, &y, &z), 1);
    CHECK_EQ_INT(world_get(&s_w, 1, 51, 0), BLOCK_SAND);
    h.x = 2;
    CHECK_EQ_INT(place_block(&s_w, &far, &h, BLOCK_TORCH, &x, &y, &z), 0);
    CHECK_EQ_INT(world_get(&s_w, 2, 51, 0), BLOCK_WATER);
    /* Not inside the player, water or not. */
    h.x = 0;
    h.z = 3;
    CHECK_EQ_INT(place_block(&s_w, &in, &h, BLOCK_STONE, &x, &y, &z), 0);
    CHECK_EQ_INT(world_get(&s_w, 0, 51, 3), BLOCK_WATER);
    /* Water itself is never placed, in water or in air. */
    h.x = 5;
    h.z = 5;
    CHECK_EQ_INT(place_block(&s_w, &far, &h, BLOCK_WATER, &x, &y, &z), 0);
    world_set(&s_w, 5, 63, 5, BLOCK_STONE);
    h.y = 63;
    CHECK_EQ_INT(place_block(&s_w, &far, &h, BLOCK_WATER, &x, &y, &z), 0);
    CHECK_EQ_INT(world_get(&s_w, 5, 64, 5), BLOCK_AIR);

    /* Water cannot be broken; a block under it breaks to air, and the
     * water above stays put. */
    CHECK_EQ_INT(world_break(&s_w, 4, 55, 4), 0);
    CHECK_EQ_INT(world_get(&s_w, 4, 55, 4), BLOCK_WATER);
    CHECK_EQ_INT(world_break(&s_w, 4, 50, 4), 1);
    CHECK_EQ_INT(world_get(&s_w, 4, 50, 4), BLOCK_AIR);
    CHECK_EQ_INT(world_get(&s_w, 4, 51, 4), BLOCK_WATER);
}

/* Sand's break bits are pieces of the sand tile. */
static void test_sand_bits(void)
{
    static Particles ps;
    float in[4];
    int k, bad = 0;

    CHECK_EQ_INT(block_face_tile(BLOCK_SAND, FACE_POS_X), TILE_SAND);
    blocktex_tile_uv(TILE_SAND, in);
    particles_init(&ps);
    particles_burst(&ps, 3, 60, -5, BLOCK_SAND, 1.0f);
    CHECK_EQ_INT(particles_live(&ps), PARTICLES_PER_BREAK);
    for (k = 0; k < PARTICLES_PER_BREAK; k++)
        bad += !(ps.p[k].uv[0] >= in[0] && ps.p[k].uv[2] <= in[2] && ps.p[k].uv[1] >= in[1] &&
                 ps.p[k].uv[3] <= in[3]);
    CHECK_EQ_INT(bad, 0);
}

static void test_eye(void)
{
    sea();
    CHECK_EQ_INT(water_eye_under(&s_w, 0.5f, 56.3f, 0.5f), 1);
    CHECK_EQ_INT(water_eye_under(&s_w, 0.5f, 61.95f, 0.5f), 1);  /* water above */
    CHECK_EQ_INT(water_eye_under(&s_w, 0.5f, 62.85f, 0.5f), 1);  /* under the surface */
    CHECK_EQ_INT(water_eye_under(&s_w, 0.5f, 62.9f, 0.5f), 0);   /* over it, in the top cell */
    CHECK_EQ_INT(water_eye_under(&s_w, 0.5f, 63.2f, 0.5f), 0);   /* air */
    CHECK_EQ_INT(water_eye_under(&s_w, -0.5f, 56.0f, -7.5f), 1); /* negative cells */
    CHECK_EQ_INT(water_eye_under(&s_w, 8.5f, 56.0f, 0.5f), 0);   /* past the sea */
    CHECK_EQ_INT(water_eye_under(&s_w, 0.5f, 50.5f, 0.5f), 0);   /* in the stone */
    CHECK_EQ_INT(water_eye_under(&s_w, 0.5f, 56.0f, 900.5f), 0); /* not loaded */
    CHECK_NEAR(WATER_TOP, 0.875, 0.0);
}

static int ch(uint32_t c, int shift)
{
    return (int)((c >> shift) & 0xFF);
}

static void test_fog_color(void)
{
    uint32_t day = water_fog_color(1.0f), dusk = water_fog_color(0.5f);

    CHECK_EQ_INT(ch(day, 0), 0xFF);
    CHECK_EQ_INT(ch(day, 8) > ch(day, 16) && ch(day, 16) > ch(day, 24), 1); /* blue */
    CHECK_EQ_INT(abs(ch(dusk, 8) * 2 - ch(day, 8)) <= 2, 1);
    CHECK_EQ_INT(abs(ch(dusk, 24) * 2 - ch(day, 24)) <= 2, 1);
    CHECK_EQ_INT(ch(dusk, 0), 0xFF);
    CHECK_EQ_INT((int)water_fog_color(0.0f), 0xFF);
    CHECK_EQ_INT(WATER_FOG_END < 16.0f && WATER_FOG_START < WATER_FOG_END, 1);
}

void test_water(void)
{
    test_ray();
    test_place_and_break();
    test_sand_bits();
    test_eye();
    test_fog_color();
}
