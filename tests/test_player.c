#include <math.h>

#include "block.h"
#include "player.h"
#include "terrain.h"
#include "test.h"
#include "world.h"

#define TOL 1e-4
#define DT (1.0f / 30.0f)
#define LO (WORLD_CHUNK_MIN * CHUNK_SIZE)                 /* -32 */
#define HI ((WORLD_CHUNK_MIN + WORLD_CHUNKS) * CHUNK_SIZE) /* 32 */

static World s_w;
/* Over a run: frames that ended with the body inside a block, and the
 * highest and lowest the feet went. */
static int s_inside;
static float s_top, s_low;

/* Stone from y 0 up to and including y top, everywhere. */
static void flat(int top)
{
    int i, j, x, y, z;

    for (j = 0; j < WORLD_CHUNKS; j++)
        for (i = 0; i < WORLD_CHUNKS; i++)
            chunk_clear(&s_w.chunks[j][i]);
    for (y = 0; y <= top; y++)
        for (z = LO; z < HI; z++)
            for (x = LO; x < HI; x++)
                world_set(&s_w, x, y, z, BLOCK_STONE);
}

/* Blocks at heights y0..y1 over x0..x1, z0..z1 (all inclusive). */
static void fill(int x0, int x1, int y0, int y1, int z0, int z1)
{
    int x, y, z;

    for (y = y0; y <= y1; y++)
        for (z = z0; z <= z1; z++)
            for (x = x0; x <= x1; x++)
                world_set(&s_w, x, y, z, BLOCK_STONE);
}

static Player at(float x, float y, float z)
{
    Player p = { x, y, z, 0.0f, false };

    return p;
}

static void run(Player *p, float vx, float vz, float secs, float dt)
{
    int i, n = (int)(secs / dt + 0.5f);

    s_inside = 0;
    s_top = s_low = p->y;
    for (i = 0; i < n; i++) {
        player_step(p, &s_w, vx, vz, dt);
        s_inside += !player_fits(&s_w, p->x, p->y, p->z);
        if (p->y > s_top)
            s_top = p->y;
        if (p->y < s_low)
            s_low = p->y;
    }
}

static void test_solid(void)
{
    flat(60);
    CHECK_EQ_INT(player_solid(&s_w, 0, -1, 0), 1);
    CHECK_EQ_INT(player_solid(&s_w, 0, 60, 0), 1);
    CHECK_EQ_INT(player_solid(&s_w, 0, 61, 0), 0);
    CHECK_EQ_INT(player_solid(&s_w, 0, WORLD_HEIGHT, 0), 0);
    /* Past each side: a wall all the way up, air above the world. */
    CHECK_EQ_INT(player_solid(&s_w, HI, 100, 0), 1);
    CHECK_EQ_INT(player_solid(&s_w, LO - 1, 100, 0), 1);
    CHECK_EQ_INT(player_solid(&s_w, 0, 100, HI), 1);
    CHECK_EQ_INT(player_solid(&s_w, 0, 100, LO - 1), 1);
    CHECK_EQ_INT(player_solid(&s_w, HI, WORLD_HEIGHT, 0), 0);
    CHECK_EQ_INT(player_solid(&s_w, HI - 1, 100, LO), 0);
}

static void test_spawn_and_fall(void)
{
    Player p;

    flat(60);
    player_spawn(&p, &s_w, 0, 0);
    CHECK_NEAR(p.x, 0.5, TOL);
    CHECK_NEAR(p.y, 61.0, TOL);
    CHECK_NEAR(p.z, 0.5, TOL);
    CHECK_EQ_INT(player_fits(&s_w, p.x, p.y, p.z), 1);
    player_spawn(&p, &s_w, -7, 12);
    CHECK_NEAR(p.x, -6.5, TOL);
    CHECK_NEAR(p.z, 12.5, TOL);

    /* Dropped from y 70: falls (down, by about half g t^2), then lands
     * exactly on the ground and stays. */
    p = at(0.5f, 70.0f, 0.5f);
    run(&p, 0, 0, 0.25f, DT);
    CHECK_EQ_INT(p.y < 69.3f && p.y > 68.8f, 1);
    CHECK_EQ_INT(p.on_ground, 0);
    run(&p, 0, 0, 3.0f, DT);
    CHECK_NEAR(p.y, 61.0, TOL);
    CHECK_NEAR(p.vy, 0.0, TOL);
    CHECK_EQ_INT(p.on_ground, 1);
    CHECK_EQ_INT(s_inside, 0);

    /* Long frames and a long fall: lands on top, never inside. */
    p = at(0.5f, 120.0f, 0.5f);
    run(&p, 0, 0, 4.0f, 0.1f);
    CHECK_NEAR(p.y, 61.0, TOL);
    CHECK_EQ_INT(s_inside, 0);
    CHECK_EQ_INT(s_low >= 61.0f - (float)TOL, 1);
    p = at(0.5f, 100.0f, 0.5f);
    run(&p, 0, 0, 5.0f, 1.0f);
    CHECK_NEAR(p.y, 61.0, TOL);
    CHECK_EQ_INT(s_inside, 0);
}

static void test_walk_flat(void)
{
    Player p;

    flat(60);
    player_spawn(&p, &s_w, 0, 0);
    run(&p, 0, 0, 0.5f, DT);
    run(&p, 0, -4.3f, 1.0f, DT); /* north */
    CHECK_NEAR(p.z, 0.5 - 4.3, 0.01);
    CHECK_NEAR(p.x, 0.5, TOL);
    CHECK_NEAR(p.y, 61.0, TOL);
    CHECK_EQ_INT(p.on_ground, 1);
    run(&p, 4.3f, 0, 1.0f, DT); /* east */
    CHECK_NEAR(p.x, 0.5 + 4.3, 0.01);
    run(&p, 0, 4.3f, 2.0f, DT); /* south */
    CHECK_NEAR(p.z, 0.5 + 4.3, 0.02);
    run(&p, -4.3f, 0, 2.0f, DT); /* west */
    CHECK_NEAR(p.x, 0.5 - 4.3, 0.02);
    CHECK_NEAR(p.y, 61.0, TOL);
    CHECK_EQ_INT(s_inside, 0);
}

static void test_walls(void)
{
    Player p;

    /* A wall two high across the world at z -3: stops flush, no hop. */
    flat(60);
    fill(LO, HI - 1, 61, 62, -3, -3);
    p = at(0.5f, 61.0f, 0.5f);
    run(&p, 0, -4.3f, 2.0f, DT);
    CHECK_NEAR(p.z, -2.0 + PLAYER_HALF, TOL);
    CHECK_NEAR(s_top, 61.0, TOL);
    CHECK_EQ_INT(s_inside, 0);

    /* Walking along it at a slant: stopped on z, still sliding on x. */
    p = at(0.5f, 61.0f, -1.0f);
    run(&p, 3.0f, -3.0f, 1.0f, DT);
    CHECK_NEAR(p.z, -2.0 + PLAYER_HALF, TOL);
    CHECK_NEAR(p.x, 3.5, 0.01);

    /* The same in x: a wall at x 3, walking east. */
    flat(60);
    fill(3, 3, 61, 62, LO, HI - 1);
    p = at(0.5f, 61.0f, 0.5f);
    run(&p, 4.3f, 0, 2.0f, DT);
    CHECK_NEAR(p.x, 3.0 - PLAYER_HALF, TOL);
    CHECK_NEAR(s_top, 61.0, TOL);
    /* Half-second frames carry 2.15 blocks each: still never through. */
    p = at(0.5f, 61.0f, 0.5f);
    run(&p, 4.3f, 0, 2.0f, 0.5f);
    CHECK_NEAR(p.x, 3.0 - PLAYER_HALF, TOL);
    CHECK_EQ_INT(s_inside, 0);
    /* And from the other side, walking west. */
    p = at(6.5f, 61.0f, 0.5f);
    run(&p, -4.3f, 0, 2.0f, DT);
    CHECK_NEAR(p.x, 4.0 + PLAYER_HALF, TOL);
}

static void test_steps(void)
{
    Player p;

    /* A one-block step up to a plateau north of z -3: walking north hops
     * onto it and carries on, no higher than the hop. */
    flat(60);
    fill(LO, HI - 1, 61, 61, LO, -3);
    p = at(0.5f, 61.0f, 0.5f);
    run(&p, 0, -4.3f, 2.0f, DT);
    CHECK_NEAR(p.y, 62.0, TOL);
    CHECK_EQ_INT(p.z < -4.0f, 1);
    CHECK_EQ_INT(p.on_ground, 1);
    CHECK_EQ_INT(s_top < 62.35f, 1);
    CHECK_EQ_INT(s_inside, 0);
    /* Back down off it, walking south: lands on the lower ground. */
    run(&p, 0, 4.3f, 2.0f, DT);
    CHECK_NEAR(p.y, 61.0, TOL);
    CHECK_EQ_INT(p.z > 0.0f, 1);

    /* The same step going east. */
    flat(60);
    fill(3, HI - 1, 61, 61, LO, HI - 1);
    p = at(0.5f, 61.0f, 0.5f);
    run(&p, 4.3f, 0, 2.0f, DT);
    CHECK_NEAR(p.y, 62.0, TOL);
    CHECK_EQ_INT(p.x > 4.0f, 1);

    /* Standing still against the step: no hop. */
    flat(60);
    fill(LO, HI - 1, 61, 61, LO, -3);
    p = at(0.5f, 61.0f, -2.0f + PLAYER_HALF);
    run(&p, 0, 0, 2.0f, DT);
    CHECK_NEAR(s_top, 61.0, TOL);

    /* Only one block of room over the step (a ceiling at y 63 above it):
     * the body does not fit up there, so no hop. */
    fill(LO, HI - 1, 63, 63, LO, -3);
    p = at(0.5f, 61.0f, 0.5f);
    run(&p, 0, -4.3f, 2.0f, DT);
    CHECK_NEAR(p.z, -2.0 + PLAYER_HALF, TOL);
    CHECK_NEAR(s_top, 61.0, TOL);

    /* Room over the step but a ceiling right over the player: no room to
     * rise here, so no hop. */
    flat(60);
    fill(LO, HI - 1, 61, 61, LO, -3);
    fill(LO, HI - 1, 63, 63, -2, -1);
    p = at(0.5f, 61.0f, -1.0f);
    run(&p, 0, -4.3f, 2.0f, DT);
    CHECK_NEAR(p.z, -2.0 + PLAYER_HALF, TOL);
    CHECK_NEAR(s_top, 61.0, TOL);

    /* Long frames (0.86 blocks a step): the spot ahead is wholly past the
     * player's own block, so only the room-over-head test stops a hop into
     * the ceiling over the player. */
    flat(60);
    fill(LO, HI - 1, 61, 61, LO, -3);
    fill(LO, HI - 1, 63, 63, -2, -2);
    p = at(0.5f, 61.0f, -1.0f);
    run(&p, 0, -4.3f, 2.0f, 0.2f);
    CHECK_NEAR(p.z, -2.0 + PLAYER_HALF, TOL);
    CHECK_NEAR(s_top, 61.0, TOL);
    CHECK_EQ_INT(s_inside, 0);
}

static void test_ceiling(void)
{
    Player p;

    /* Thrown up under a ceiling at y 63: the head stops at it, the body
     * comes back down, never inside. */
    flat(60);
    fill(LO, HI - 1, 63, 63, LO, HI - 1);
    p = at(0.5f, 61.0f, 0.5f);
    p.vy = PLAYER_HOP_SPEED;
    run(&p, 0, 0, 1.0f, DT);
    CHECK_NEAR(s_top, 63.0 - PLAYER_HEIGHT, 0.001);
    CHECK_NEAR(p.y, 61.0, TOL);
    CHECK_EQ_INT(p.on_ground, 1);
    CHECK_EQ_INT(s_inside, 0);
}

static void test_edges(void)
{
    Player p;

    /* The invisible wall at each side of the world, and no hop at it. */
    flat(60);
    p = at(28.5f, 61.0f, 0.5f);
    run(&p, 4.3f, 0, 3.0f, DT);
    CHECK_NEAR(p.x, HI - PLAYER_HALF, TOL);
    CHECK_NEAR(s_top, 61.0, TOL);
    p = at(-28.5f, 61.0f, 0.5f);
    run(&p, -4.3f, 0, 3.0f, DT);
    CHECK_NEAR(p.x, LO + PLAYER_HALF, TOL);
    p = at(0.5f, 61.0f, -28.5f);
    run(&p, 0, -4.3f, 3.0f, DT);
    CHECK_NEAR(p.z, LO + PLAYER_HALF, TOL);
    p = at(0.5f, 61.0f, 28.5f);
    run(&p, 0, 4.3f, 3.0f, DT);
    CHECK_NEAR(p.z, HI - PLAYER_HALF, TOL);
    CHECK_NEAR(s_top, 61.0, TOL);
    CHECK_EQ_INT(s_inside, 0);
}

/* The real world: walking off in 8 directions over the hills. Never inside
 * a block, never below the lowest ground; the hills are gentle, so the
 * walks go far and some climb. */
static void test_hills(void)
{
    static const float dir[8][2] = {
        { 0, -1 }, { 0.7071f, -0.7071f }, { 1, 0 }, { 0.7071f, 0.7071f },
        { 0, 1 }, { -0.7071f, 0.7071f }, { -1, 0 }, { -0.7071f, -0.7071f },
    };
    int d, inside = 0, far = 0, climbed = 0, low = 0;

    world_generate(&s_w, 2026u);
    for (d = 0; d < 8; d++) {
        Player p;
        float x0, z0, y0;

        player_spawn(&p, &s_w, 0, 0);
        run(&p, 0, 0, 0.5f, DT);
        x0 = p.x;
        z0 = p.z;
        y0 = p.y;
        run(&p, dir[d][0] * 4.3f, dir[d][1] * 4.3f, 6.0f, DT);
        inside += s_inside;
        low += s_low < TERRAIN_BASE + 1.0f;
        far += sqrtf((p.x - x0) * (p.x - x0) + (p.z - z0) * (p.z - z0)) > 15.0f;
        climbed += s_top - y0 >= 1.0f;
    }
    CHECK_EQ_INT(inside, 0);
    CHECK_EQ_INT(low, 0);
    CHECK_EQ_INT(far >= 6, 1);
    CHECK_EQ_INT(climbed >= 2, 1);
}

void test_player(void)
{
    test_solid();
    test_spawn_and_fall();
    test_walk_flat();
    test_walls();
    test_steps();
    test_ceiling();
    test_edges();
    test_hills();
}
