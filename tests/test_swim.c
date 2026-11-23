#include <math.h>

#include "block.h"
#include "player.h"
#include "test.h"
#include "world.h"

/* Swimming (roadmap row 4-01): floating, half speed, climbing out. The
 * worlds are stone up to y 62 (ground top at 63, one above the water),
 * with a pool x -8..7, z -8..7 cut in: water up to y 61 (surface at 62),
 * air above. */

#define TOL 1e-4
#define DT (1.0f / 30.0f)
#define SURFACE 62.0f
#define FLOAT_Y (SURFACE - PLAYER_SWIM_LINE) /* feet when floating: 60.8 */
#define LO (SMALL_MIN * CHUNK_SIZE)
#define HI ((SMALL_MIN + SMALL_CHUNKS) * CHUNK_SIZE)

static World s_w;
static float s_top, s_low;
static int s_inside;

/* The pool, depth blocks of water over a stone floor. */
static void pool(int depth)
{
    int x, y, z;

    test_world_cleared(&s_w);
    for (y = 0; y <= 62; y++)
        for (z = LO; z < HI; z++)
            for (x = LO; x < HI; x++)
                world_set(&s_w, x, y, z, BLOCK_STONE);
    for (y = 62 - depth; y <= 62; y++)
        for (z = -8; z < 8; z++)
            for (x = -8; x < 8; x++)
                world_set(&s_w, x, y, z, y <= 61 ? BLOCK_WATER : BLOCK_AIR);
}

static Player at(float x, float y, float z)
{
    Player p = { x, y, z, 0.0f, false };

    return p;
}

static void run(Player *p, float vx, float vz, float secs)
{
    int i, n = (int)(secs / DT + 0.5f);

    s_inside = 0;
    s_top = s_low = p->y;
    for (i = 0; i < n; i++) {
        player_step(p, &s_w, vx, vz, DT);
        s_inside += !player_fits(&s_w, p->x, p->y, p->z);
        if (p->y > s_top)
            s_top = p->y;
        if (p->y < s_low)
            s_low = p->y;
    }
}

static void test_in_water(void)
{
    Player p = at(0.5f, 58.0f, 0.5f);
    float surface = 0.0f;

    pool(12);
    CHECK_EQ_INT(player_in_water(&s_w, &p, &surface), 1);
    CHECK_NEAR(surface, SURFACE, TOL);
    p.y = 62.0f; /* feet in the air above the surface */
    CHECK_EQ_INT(player_in_water(&s_w, &p, NULL), 0);
    p.y = 61.9f;
    CHECK_EQ_INT(player_in_water(&s_w, &p, NULL), 1);
    p = at(10.5f, 63.0f, 0.5f); /* on the bank */
    CHECK_EQ_INT(player_in_water(&s_w, &p, NULL), 0);
}

/* Settles with the eye 0.42 over the surface, and stays. */
static void test_float(void)
{
    Player p = at(0.5f, 58.0f, 0.5f); /* started 2.8 down, floats up */

    pool(12);
    run(&p, 0, 0, 3.0f);
    CHECK_NEAR(p.y, FLOAT_Y, 0.02);
    CHECK_NEAR(p.y + PLAYER_EYE - SURFACE, 0.42, 0.02);
    CHECK_EQ_INT(s_inside, 0);
    run(&p, 0, 0, 4.0f);
    CHECK_EQ_INT(s_top - s_low < 0.01f, 1); /* no bobbing */
    CHECK_NEAR(p.vy, 0.0, 0.02);

    /* Started above the float height: sinks down to it, no overshoot. */
    p = at(0.5f, 62.0f - 0.01f, 0.5f);
    run(&p, 0, 0, 3.0f);
    CHECK_NEAR(p.y, FLOAT_Y, 0.02);
    CHECK_EQ_INT(s_top < 62.0f, 1);
}

static void test_shallow(void)
{
    Player p = at(0.5f, 62.0f, 0.5f);

    pool(1); /* floor top at 61 */
    run(&p, 0, 0, 2.0f);
    CHECK_NEAR(p.y, 61.0, TOL);
    CHECK_EQ_INT(p.on_ground, 1);
    CHECK_EQ_INT(player_in_water(&s_w, &p, NULL), 1);
    pool(2); /* floor top at 60: floats 0.8 above it */
    p = at(0.5f, 61.0f, 0.5f);
    run(&p, 0, 0, 3.0f);
    CHECK_NEAR(p.y, FLOAT_Y, 0.02);
}

static void test_fall_in(void)
{
    Player p = at(0.5f, 72.0f, 0.5f); /* 10 blocks over the surface */

    pool(12); /* floor top at 50 */
    run(&p, 0, 0, 2.0f);
    CHECK_EQ_INT(s_low > 56.0f, 1); /* slowed by the water: nowhere near 50 */
    run(&p, 0, 0, 4.0f);
    CHECK_NEAR(p.y, FLOAT_Y, 0.02);
    CHECK_EQ_INT(p.on_ground, 0);
    CHECK_EQ_INT(s_inside, 0);
}

static void test_speed(void)
{
    Player p = at(-5.5f, FLOAT_Y, 0.5f);

    pool(12);
    run(&p, 0, 0, 1.0f);
    run(&p, 4.3f, 0, 1.0f);
    CHECK_NEAR(p.x, -5.5 + 4.3 * 0.5, 0.01);
    CHECK_NEAR(p.y, FLOAT_Y, 0.02);
    run(&p, 0, -4.3f, 1.0f);
    CHECK_NEAR(p.z, 0.5 - 2.15, 0.01);
}

/* A bank one block over the surface: out onto it. */
static void test_bank(void)
{
    Player p = at(0.5f, FLOAT_Y, 0.5f);

    pool(12);
    run(&p, 0, 0, 1.0f);
    run(&p, -4.3f, 0, 7.0f);
    CHECK_NEAR(p.y, 63.0, TOL);
    CHECK_EQ_INT(p.on_ground, 1);
    CHECK_EQ_INT(p.x < -8.5f, 1);
    CHECK_EQ_INT(s_inside, 0);
    CHECK_EQ_INT(s_top < 64.6f, 1);
    /* Other sides and a corner too. */
    p = at(0.5f, FLOAT_Y, 0.5f);
    run(&p, 0, 0, 1.0f);
    run(&p, 0, 4.3f, 7.0f);
    CHECK_NEAR(p.y, 63.0, TOL);
    CHECK_EQ_INT(p.z > 8.5f, 1);
}

/* A wall two blocks over the surface: stops the swimmer. */
static void test_wall(void)
{
    Player p = at(0.5f, FLOAT_Y, 0.5f);
    int x, z;

    pool(12);
    for (z = -20; z < 20; z++)
        for (x = -14; x <= -9; x++)
            world_set(&s_w, x, 63, z, BLOCK_STONE);
    run(&p, 0, 0, 1.0f);
    run(&p, -4.3f, 0, 8.0f);
    CHECK_NEAR(p.x, -8.0 + PLAYER_HALF, TOL);
    CHECK_NEAR(p.y, FLOAT_Y, 0.02);
    CHECK_EQ_INT(s_top < 61.5f, 1); /* never kicked up */
    CHECK_EQ_INT(s_inside, 0);
}

void test_swim(void)
{
    test_in_water();
    test_float();
    test_shallow();
    test_fall_in();
    test_speed();
    test_bank();
    test_wall();
}
