#include <math.h>
#include <stdint.h>
#include <string.h>

#include "block.h"
#include "raycast.h"
#include "test.h"

/* Row 1-08: which block the player looks at. Hand-built worlds for exact
 * cases, then thousands of random rays over the real hills checked against
 * a slow walk along the ray in tiny steps. */

static World s_w;

/* Empty world, then stone from y 0 up to and including top. */
static void flat(int top)
{
    int x, y, z;

    test_world_cleared(&s_w);
    for (x = -32; x < 32; x++)
        for (z = -32; z < 32; z++)
            for (y = 0; y <= top; y++)
                world_set(&s_w, x, y, z, BLOCK_STONE);
}

static void check_hit(float ox, float oy, float oz, float dx, float dy, float dz, float reach,
                      int x, int y, int z, int face, float dist)
{
    RayHit h = { -999, -999, -999, -1, -1.0f };
    bool got = raycast(&s_w, ox, oy, oz, dx, dy, dz, reach, &h);

    CHECK_EQ_INT(got, 1);
    CHECK_EQ_INT(h.x, x);
    CHECK_EQ_INT(h.y, y);
    CHECK_EQ_INT(h.z, z);
    CHECK_EQ_INT(h.face, face);
    CHECK_NEAR(h.dist, dist, 1e-4);
}

static void check_miss(float ox, float oy, float oz, float dx, float dy, float dz, float reach)
{
    RayHit h;

    CHECK_EQ_INT(raycast(&s_w, ox, oy, oz, dx, dy, dz, reach, &h), 0);
}

static void test_exact(void)
{
    flat(63);
    /* Straight down from eye height: the block under the feet, its top. */
    check_hit(0.5f, 65.62f, 0.5f, 0, -1, 0, RAY_REACH, 0, 63, 0, FACE_POS_Y, 1.62f);
    /* Length of the direction does not matter. */
    check_hit(0.5f, 65.62f, 0.5f, 0, -7, 0, RAY_REACH, 0, 63, 0, FACE_POS_Y, 1.62f);
    /* Negative blocks round down. */
    check_hit(-0.25f, 64.5f, -3.75f, 0, -1, 0, RAY_REACH, -1, 63, -4, FACE_POS_Y, 0.5f);
    /* Looking level over flat ground: nothing (the world's side is not a block). */
    check_miss(0.5f, 64.5f, 0.5f, 0, 0, -1, RAY_REACH);
    /* Zero direction: nothing. */
    check_miss(0.5f, 65.62f, 0.5f, 0, 0, 0, RAY_REACH);

    /* The block the ray starts in is skipped: an eye inside the ground sees
     * the block below, not the one it is in. */
    check_hit(0.5f, 63.5f, 0.5f, 0, -1, 0, RAY_REACH, 0, 62, 0, FACE_POS_Y, 0.5f);

    /* Six directions from a spot in open air, a block two away each way:
     * the face is the one turned toward the eye. */
    flat(0);
    world_set(&s_w, 7, 100, 5, BLOCK_DIRT);
    world_set(&s_w, 3, 100, 5, BLOCK_DIRT);
    world_set(&s_w, 5, 102, 5, BLOCK_DIRT);
    world_set(&s_w, 5, 98, 5, BLOCK_DIRT);
    world_set(&s_w, 5, 100, 7, BLOCK_DIRT);
    world_set(&s_w, 5, 100, 3, BLOCK_DIRT);
    check_hit(5.5f, 100.5f, 5.5f, 1, 0, 0, RAY_REACH, 7, 100, 5, FACE_NEG_X, 1.5f);
    check_hit(5.5f, 100.5f, 5.5f, -1, 0, 0, RAY_REACH, 3, 100, 5, FACE_POS_X, 1.5f);
    check_hit(5.5f, 100.5f, 5.5f, 0, 1, 0, RAY_REACH, 5, 102, 5, FACE_NEG_Y, 1.5f);
    check_hit(5.5f, 100.5f, 5.5f, 0, -1, 0, RAY_REACH, 5, 98, 5, FACE_POS_Y, 1.5f);
    check_hit(5.5f, 100.5f, 5.5f, 0, 0, 1, RAY_REACH, 5, 100, 7, FACE_NEG_Z, 1.5f);
    check_hit(5.5f, 100.5f, 5.5f, 0, 0, -1, RAY_REACH, 5, 100, 3, FACE_POS_Z, 1.5f);

    /* Reach: a block 4.5 away is hit, one 5.5 away is not. */
    world_set(&s_w, 10, 100, 5, BLOCK_DIRT);
    world_set(&s_w, 7, 100, 5, BLOCK_AIR);
    check_hit(5.5f, 100.5f, 5.5f, 1, 0, 0, RAY_REACH, 10, 100, 5, FACE_NEG_X, 4.5f);
    world_set(&s_w, 10, 100, 5, BLOCK_AIR);
    world_set(&s_w, 11, 100, 5, BLOCK_DIRT);
    check_miss(5.5f, 100.5f, 5.5f, 1, 0, 0, RAY_REACH);
    check_hit(5.5f, 100.5f, 5.5f, 1, 0, 0, 6.0f, 11, 100, 5, FACE_NEG_X, 5.5f);

    /* A slanted ray: from (0.5, 100.5, 0.75) along (1, 0, -0.25) it crosses
     * x = 1 at z 0.625 (block (1, 100, 0), air) and x = 2 at z 0.375, into
     * block (2, 100, 0) through its west face, 1.5 x |(1, 0, -0.25)| away. */
    world_set(&s_w, 2, 100, 0, BLOCK_DIRT);
    check_hit(0.5f, 100.5f, 0.75f, 1, 0, -0.25f, RAY_REACH, 2, 100, 0, FACE_NEG_X,
              1.5f * sqrtf(1.0f + 0.0625f));

    /* Nothing above the world's top or outside its sides is ever hit. */
    check_miss(0.5f, 140.0f, 0.5f, 0, 1, 0, RAY_REACH);
    check_miss(40.5f, 3.0f, 0.5f, 0, -1, 0, RAY_REACH);
    check_miss(-40.5f, 3.0f, -40.5f, 1, -1, 1, RAY_REACH);
}

/* --- random rays on the real hills, against a slow walk ------------------- */

static uint32_t s_rng = 12345u;

static float rnd(void) /* 0 to 1 */
{
    s_rng = s_rng * 1664525u + 1013904223u;
    return (float)(s_rng >> 8) / 16777216.0f;
}

static int cell(double v)
{
    return (int)floor(v);
}

/* First solid block (not the start block) seen walking the ray in tiny
 * steps from distance t0 to t1, or false. In double: in float a ray almost
 * along a block face rounds onto it early (found 2026-11-23 with a ray
 * 0.0003 off parallel, which met x = 29 at 1.1922 but read 29 at 1.1890). */
static bool slow_walk(double ox, double oy, double oz, double dx, double dy, double dz,
                      double t0, double t1)
{
    int sx = cell(ox), sy = cell(oy), sz = cell(oz);
    double t;

    for (t = t0; t <= t1; t += 0.0005) {
        int x = cell(ox + dx * t), y = cell(oy + dy * t), z = cell(oz + dz * t);
        int b = world_get(&s_w, x, y, z);

        if ((x != sx || y != sy || z != sz) && b != BLOCK_AIR && b != BLOCK_WATER) /* through water */
            return true;
    }
    return false;
}

/* Is the point on the given face of block (x, y, z), within tol? */
static bool on_face(float px, float py, float pz, int x, int y, int z, int face, float tol)
{
    float lo[3] = { (float)x, (float)y, (float)z };
    float p[3] = { px, py, pz };
    int a = face / 2;                 /* FACE_POS_X, NEG_X = 0, 1: axis x, ... */
    float plane = lo[a] + ((face % 2) == 0 ? 1.0f : 0.0f);
    int k;

    if (fabsf(p[a] - plane) > tol)
        return false;
    for (k = 0; k < 3; k++)
        if (k != a && (p[k] < lo[k] - tol || p[k] > lo[k] + 1.0f + tol))
            return false;
    return true;
}

static void test_random(void)
{
    int n, hits = 0, misses = 0, bad_hit = 0, bad_early = 0, bad_miss = 0, flats = 0;

    test_world_small(&s_w, test_land_seed()); /* hills, not seed 2026's sea (row 4-01) */
    for (n = 0; n < 3000; n++) {
        float ox = -30.0f + 60.0f * rnd(), oy = 58.0f + 16.0f * rnd(), oz = -30.0f + 60.0f * rnd();
        float dx = rnd() * 2.0f - 1.0f, dy = rnd() * 2.0f - 1.0f, dz = rnd() * 2.0f - 1.0f;
        float len;
        RayHit h;

        /* Every fourth ray along a plane or an axis: the "never" path. */
        if (n % 4 == 1) {
            dx = 0.0f;
            flats++;
        } else if (n % 4 == 3) {
            dx = 0.0f;
            dz = 0.0f;
            flats++;
        }
        len = sqrtf(dx * dx + dy * dy + dz * dz);
        if (len < 0.05f)
            continue;
        dx /= len;
        dy /= len;
        dz /= len;
        if (raycast(&s_w, ox, oy, oz, dx, dy, dz, RAY_REACH, &h)) {
            hits++;
            /* The block is solid, the ray meets it on the named face, and
             * nothing solid comes before. */
            if (world_get(&s_w, h.x, h.y, h.z) == BLOCK_AIR || world_get(&s_w, h.x, h.y, h.z) == BLOCK_WATER ||
                h.dist < 0.0f || h.dist > RAY_REACH ||
                !on_face(ox + dx * h.dist, oy + dy * h.dist, oz + dz * h.dist, h.x, h.y, h.z, h.face, 1e-3f))
                bad_hit++;
            if (slow_walk(ox, oy, oz, dx, dy, dz, 0.0f, h.dist - 0.002f))
                bad_early++;
        } else {
            misses++;
            if (slow_walk(ox, oy, oz, dx, dy, dz, 0.0f, RAY_REACH - 0.002f))
                bad_miss++;
        }
    }
    CHECK_EQ_INT(bad_hit, 0);
    CHECK_EQ_INT(bad_early, 0);
    CHECK_EQ_INT(bad_miss, 0);
    /* Both kinds happened plenty, so neither check passed by being empty. */
    CHECK_EQ_INT(hits > 500, 1);
    CHECK_EQ_INT(misses > 300, 1);
    CHECK_EQ_INT(flats > 1000, 1);
}

void test_raycast(void)
{
    test_exact();
    test_random();
}
