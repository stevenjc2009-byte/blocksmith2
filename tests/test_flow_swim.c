#include <math.h>

#include "block.h"
#include "hotbar.h"
#include "place.h"
#include "player.h"
#include "raycast.h"
#include "test.h"
#include "water.h"
#include "world.h"

/* Row 4-04: flowing and falling water are water to everything that knew
 * still water (block_is_water()): the aiming ray goes through them, the eye
 * is under them below their surface, they float a swimmer at their own
 * height, they cannot be broken, held or placed, and a block (not a torch)
 * goes in their place. And their current carries a swimmer. */

#define FLOOR 50
#define L (FLOOR + 1) /* the layer the water lies on */
#define DT 0.05f      /* one Minecraft tick */

static World s_w;

/* Stone from y 0 to FLOOR over x and z -16 to 15, air above. */
static void ground(void)
{
    int x, y, z;

    test_world_cleared(&s_w);
    for (x = -16; x < 16; x++)
        for (z = -16; z < 16; z++)
            for (y = 0; y <= FLOOR; y++)
                world_set(&s_w, x, y, z, BLOCK_STONE);
}

static void test_ray(void)
{
    RayHit h = { -99, -99, -99, -1, -1.0f };

    ground();
    world_set(&s_w, 0, L, 0, BLOCK_FLOW_3);
    world_set(&s_w, 0, L + 1, 0, BLOCK_FALL);
    world_set(&s_w, 0, L + 2, 0, BLOCK_FALL);
    CHECK_EQ_INT(raycast(&s_w, 0.5f, L + 3.5f, 0.5f, 0, -1, 0, RAY_REACH, &h), 1);
    CHECK_EQ_INT(h.x * 10000 + h.y * 100 + h.z, FLOOR * 100);
    CHECK_EQ_INT(h.face, FACE_POS_Y);
    CHECK_NEAR(h.dist, 3.5, 1e-4);
    world_set(&s_w, 1, L, 0, BLOCK_FLOW_7);
    world_set(&s_w, 2, L, 0, BLOCK_FLOW_1);
    world_set(&s_w, 3, L, 0, BLOCK_STONE);
    CHECK_EQ_INT(raycast(&s_w, 0.5f, L + 0.5f, 0.5f, 1, 0, 0, RAY_REACH, &h), 1);
    CHECK_EQ_INT(h.x * 10000 + h.y * 100 + h.z, 3 * 10000 + L * 100);
    CHECK_EQ_INT(h.face, FACE_NEG_X);
    CHECK_NEAR(h.dist, 2.5, 1e-4);
}

/* The eye is under flowing water below its surface, water_height() up the
 * cell, or anywhere in it with water above. */
static void test_eye(void)
{
    ground();
    world_set(&s_w, 0, L, 0, BLOCK_FLOW_3); /* 5/9 high */
    world_set(&s_w, 1, L, 0, BLOCK_FALL);
    world_set(&s_w, 2, L, 0, BLOCK_WATER);
    world_set(&s_w, 3, L, 0, BLOCK_FLOW_3);
    world_set(&s_w, 3, L + 1, 0, BLOCK_FLOW_3);
    CHECK_EQ_INT(water_eye_under(&s_w, 0.5f, L + 0.5f, 0.5f), 1);
    CHECK_EQ_INT(water_eye_under(&s_w, 0.5f, L + 0.6f, 0.5f), 0);
    CHECK_EQ_INT(water_eye_under(&s_w, 1.5f, L + 0.99f, 0.5f), 1);
    CHECK_EQ_INT(water_eye_under(&s_w, 2.5f, L + 0.8f, 0.5f), 1);
    CHECK_EQ_INT(water_eye_under(&s_w, 2.5f, L + 0.9f, 0.5f), 0);
    CHECK_EQ_INT(water_eye_under(&s_w, 3.5f, L + 0.9f, 0.5f), 1);
    CHECK_EQ_INT(water_eye_under(&s_w, 3.5f, L + 1.6f, 0.5f), 0);
    CHECK_EQ_INT(water_eye_under(&s_w, 4.5f, L + 0.1f, 0.5f), 0);
}

/* The swimmer's surface in a thin cell is its own, lower surface: as far
 * under the top of the cell as the drawn top is under still water's. */
static void test_surface(void)
{
    Player p = { 0.5f, (float)L, 0.5f, 0.0f, true };
    float s = 0.0f;

    ground();
    world_set(&s_w, 0, L, 0, BLOCK_FLOW_7);
    CHECK_EQ_INT(player_in_water(&s_w, &p, &s), 1);
    CHECK_NEAR(s, L + 1.0 - (WATER_TOP - 1.0 / 9), 1e-5);
    world_set(&s_w, 0, L, 0, BLOCK_FLOW_1);
    CHECK_EQ_INT(player_in_water(&s_w, &p, &s), 1);
    CHECK_NEAR(s, L + 1.0 - (WATER_TOP - 7.0 / 9), 1e-5);
    world_set(&s_w, 0, L, 0, BLOCK_WATER);
    CHECK_EQ_INT(player_in_water(&s_w, &p, &s), 1);
    CHECK_NEAR(s, L + 1.0, 1e-6); /* as row 4-01 */
    world_set(&s_w, 0, L, 0, BLOCK_FALL);
    CHECK_EQ_INT(player_in_water(&s_w, &p, &s), 1);
    CHECK_NEAR(s, L + 1.0, 1e-6);
    world_set(&s_w, 0, L + 1, 0, BLOCK_FLOW_3); /* the run's top cell counts */
    CHECK_EQ_INT(player_in_water(&s_w, &p, &s), 1);
    CHECK_NEAR(s, L + 2.0 - (WATER_TOP - 5.0 / 9), 1e-5);
}

/* A channel one wide along +x (dir 0) or -z (dir 1) holding levels 2, 3, 4
 * downstream from 0, stone walls either side. */
static void channel(int ox, int oz, int dir)
{
    int i;

    for (i = -1; i <= 3; i++) {
        int x = dir == 0 ? ox + i : ox, z = dir == 0 ? oz : oz - i;
        int wx = dir == 0 ? 0 : 1, wz = dir == 0 ? 1 : 0;

        world_set(&s_w, x, L, z, i >= 0 && i < 3 ? BLOCK_FLOW_2 + i : BLOCK_STONE);
        world_set(&s_w, x + wx, L, z + wz, BLOCK_STONE);
        world_set(&s_w, x - wx, L, z - wz, BLOCK_STONE);
    }
}

static void current(int x, int y, int z, double fx, double fz)
{
    float gx = 99.0f, gz = 99.0f;

    water_current(&s_w, x, y, z, &gx, &gz);
    CHECK_NEAR(gx, fx, 1e-5);
    CHECK_NEAR(gz, fz, 1e-5);
}

static void test_current(void)
{
    ground();
    channel(0, 0, 0);
    current(1, L, 0, 1.0, 0.0);
    current(0, L, 0, 1.0, 0.0); /* the wall upstream counts nothing */
    channel(-8, -4, 1);
    current(-8, L, -5, 0.0, -1.0);
    /* An open side with water a cell under it pulls hard that way. */
    world_set(&s_w, 6, L, 6, BLOCK_FLOW_6);
    current(6, L, 6, 0.0, 0.0); /* open all round, over stone */
    world_set(&s_w, 7, FLOOR, 6, BLOCK_FLOW_7);
    current(6, L, 6, 1.0, 0.0);
    /* Two equal pulls at right angles: half way between. */
    world_set(&s_w, 6, FLOOR, 7, BLOCK_FLOW_7);
    current(6, L, 6, sqrt(0.5), sqrt(0.5));
    /* Still and falling water and dry cells have none. */
    world_set(&s_w, 10, L, 10, BLOCK_WATER);
    world_set(&s_w, 11, L, 10, BLOCK_FLOW_1);
    current(10, L, 10, 0.0, 0.0);
    world_set(&s_w, 10, L, 12, BLOCK_FALL);
    world_set(&s_w, 11, L, 12, BLOCK_FLOW_5);
    current(10, L, 12, 0.0, 0.0);
    current(12, L, 12, 0.0, 0.0);
}

/* One tick standing in the stream moves the player PLAYER_CURRENT * DT
 * (0.014 blocks, Minecraft's push a tick) along it, and no other way. */
static void push(float x, float z, double dx, double dz)
{
    Player p = { x, (float)L, z, 0.0f, true };

    player_step(&p, &s_w, 0.0f, 0.0f, DT);
    CHECK_NEAR(p.x - x, dx, 1e-4);
    CHECK_NEAR(p.z - z, dz, 1e-4);
}

static void test_push(void)
{
    ground();
    channel(0, 0, 0);
    channel(-8, -4, 1);
    world_set(&s_w, 10, L, 10, BLOCK_FALL);
    world_set(&s_w, 11, L, 10, BLOCK_FLOW_5);
    world_set(&s_w, 10, L, 12, BLOCK_WATER);
    world_set(&s_w, 11, L, 12, BLOCK_FLOW_1);
    CHECK_NEAR(PLAYER_CURRENT * DT, 0.014, 1e-6);
    push(1.5f, 0.5f, 0.014, 0.0);
    push(-7.5f, -4.5f, 0.0, -0.014);
    push(10.5f, 10.5f, 0.0, 0.0); /* falling water: no push sideways */
    push(10.5f, 12.5f, 0.0, 0.0); /* still water */
    push(5.5f, 5.5f, 0.0, 0.0);   /* dry ground */
}

/* Flowing water cannot be broken, held or placed; a block goes in its
 * place, a torch does not. */
static void test_hands(void)
{
    static const uint8_t slots[HOTBAR_SLOTS] = {
        BLOCK_FLOW_1, BLOCK_GRASS, BLOCK_TORCH, BLOCK_SAND, BLOCK_FALL, BLOCK_FLOW_7,
        BLOCK_AIR, BLOCK_AIR, BLOCK_AIR,
    };
    Player far = { 12.5f, (float)L, 12.5f, 0.0f, true };
    RayHit h = { 0, FLOOR, 0, FACE_POS_Y, 1.0f };
    Hotbar hb;
    int b, x, y, z;

    ground();
    for (b = BLOCK_FLOW_1; b <= BLOCK_FALL; b++) {
        world_set(&s_w, 0, L, 0, b);
        CHECK_EQ_INT(world_break(&s_w, 0, L, 0), 0);
        CHECK_EQ_INT(world_get(&s_w, 0, L, 0), b);
        CHECK_EQ_INT(place_block(&s_w, &far, &h, BLOCK_TORCH, &x, &y, &z), 0);
        CHECK_EQ_INT(place_block(&s_w, &far, &h, BLOCK_FLOW_1, &x, &y, &z), 0);
        CHECK_EQ_INT(place_block(&s_w, &far, &h, BLOCK_FALL, &x, &y, &z), 0);
        CHECK_EQ_INT(world_get(&s_w, 0, L, 0), b);
        CHECK_EQ_INT(place_block(&s_w, &far, &h, BLOCK_DIRT, &x, &y, &z), 1);
        CHECK_EQ_INT(world_get(&s_w, 0, L, 0), BLOCK_DIRT);
        world_set(&s_w, 0, L, 0, BLOCK_AIR);
    }
    /* Into air nothing but real blocks go either. */
    CHECK_EQ_INT(place_block(&s_w, &far, &h, BLOCK_FLOW_4, &x, &y, &z), 0);
    CHECK_EQ_INT(world_get(&s_w, 0, L, 0), BLOCK_AIR);
    hotbar_load(&hb, slots, 1);
    CHECK_EQ_INT(hb.block[0], BLOCK_AIR);
    CHECK_EQ_INT(hb.block[1], BLOCK_GRASS);
    CHECK_EQ_INT(hb.block[2], BLOCK_TORCH);
    CHECK_EQ_INT(hb.block[3], BLOCK_SAND);
    CHECK_EQ_INT(hb.block[4], BLOCK_AIR);
    CHECK_EQ_INT(hb.block[5], BLOCK_AIR);
    CHECK_EQ_INT(hb.chosen, 1);
}

void test_flow_swim(void)
{
    test_ray();
    test_eye();
    test_surface();
    test_current();
    test_push();
    test_hands();
}
