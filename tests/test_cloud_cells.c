#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cloud.h"
#include "mesh.h"
#include "test.h"

/* Fix batch, problem 9: skydraw.c builds the clouds once, in the pattern
 * before the drift (cloud_build(eye x - shift, z, 0)), moves them on by the
 * drift with the matrix, and builds them again only when cloud_cells()
 * changes. Here: that draws what cloud_build() at the time of day drew, and
 * equal cells always mean an equal build. And cloud_at(), which now reads a
 * table of the pattern, answers as row 3-04's sum of hashes did. */

static MeshVertex s_a[CLOUD_VERTS_MAX], s_b[CLOUD_VERTS_MAX];

static unsigned s_r = 98765u;

static float rnd(float lo, float hi)
{
    s_r = s_r * 1103515245u + 12345u;
    return lo + (hi - lo) * (float)((s_r >> 8) % 100000u) / 100000.0f;
}

/* Row 3-04's cloud_at(), as it was before the table. */
static uint32_t ref_wrap(int i)
{
    i %= CLOUD_PERIOD;
    return (uint32_t)(i < 0 ? i + CLOUD_PERIOD : i);
}

static float ref_value(int i, int j)
{
    uint32_t h = ref_wrap(i) * 0x8DA6B343u ^ ref_wrap(j) * 0xD8163841u ^ 0x3C6EF372u;

    h ^= h >> 13;
    h *= 0x5BD1E995u;
    h ^= h >> 15;
    return (float)(h >> 8) / 16777216.0f;
}

static bool ref_cloud_at(int i, int j)
{
    float v = 2.0f * ref_value(i, j) + ref_value(i - 1, j) + ref_value(i + 1, j) +
              ref_value(i, j - 1) + ref_value(i, j + 1);

    return v > 6.0f * 0.56f;
}

/* Every cell of the pattern, and some far out either way. */
static void test_table(void)
{
    int i, j, bad = 0, cloud = 0;

    for (j = -CLOUD_PERIOD - 3; j < 2 * CLOUD_PERIOD + 3; j++)
        for (i = -CLOUD_PERIOD - 3; i < 2 * CLOUD_PERIOD + 3; i++) {
            bad += cloud_at(i, j) != ref_cloud_at(i, j);
            cloud += cloud_at(i, j);
        }
    for (i = 0; i < 200; i++) {
        int a = (int)rnd(-1e6f, 1e6f), b = (int)rnd(-1e6f, 1e6f);

        bad += cloud_at(a, b) != ref_cloud_at(a, b);
    }
    CHECK_EQ_INT(bad, 0);
    CHECK_EQ_INT(cloud > 0, 1);
}

/* Built at time of day t round (ex, ez), and built in the pattern then
 * moved east by the drift: the same faces, the same order. */
static void test_moved(void)
{
    int k, bad = 0, diff_n = 0, tried = 0;

    for (k = 0; k < 40; k++) {
        float ex = rnd(-5000.0f, 5000.0f), ez = rnd(-5000.0f, 5000.0f), t = rnd(0.0f, 1.0f);
        float shift = cloud_shift(t);
        int n = cloud_build(ex, ez, t, s_a), m = cloud_build(ex - shift, ez, 0.0f, s_b), i;

        diff_n += n != m;
        tried += n > 0;
        for (i = 0; i < n && i < m; i++) {
            const MeshVertex *a = &s_a[i], *b = &s_b[i];

            bad += fabsf(b->x + shift - a->x) > 0.01f || b->y != a->y || b->z != a->z ||
                   b->u != a->u || b->v != a->v || b->r != a->r || b->g != a->g ||
                   b->b != a->b || b->t != a->t;
        }
    }
    CHECK_EQ_INT(diff_n, 0);
    CHECK_EQ_INT(bad, 0);
    CHECK_EQ_INT(tried > 30, 1); /* clouds over most of those eyes */
}

/* Two eyes whose cells match build the very same vertices; two whose
 * builds differ have cells that differ. Pairs a little way apart, and a
 * whole pattern apart east-west or north-south (the same cloud, but not
 * the same place). */
static void test_same_cells(void)
{
    static const float k_step[][2] = {
        { 0.01f, 0.0f }, { 0.5f, 0.0f }, { 0.0f, 0.5f }, { 3.0f, 2.0f },
        { (float)(CLOUD_PERIOD * CLOUD_CELL), 0.0f }, { 0.0f, (float)(CLOUD_PERIOD * CLOUD_CELL) },
    };
    int k, s, same = 0, moved = 0, bad = 0;

    for (k = 0; k < 60; k++) {
        float px = rnd(-3000.0f, 3000.0f), ez = rnd(-3000.0f, 3000.0f);

        for (s = 0; s < (int)(sizeof k_step / sizeof k_step[0]); s++) {
            float qx = px + k_step[s][0], qz = ez + k_step[s][1];
            CloudCells c1, c2;
            int n = cloud_build(px, ez, 0.0f, s_a), m = cloud_build(qx, qz, 0.0f, s_b);
            bool built_same = n == m && memcmp(s_a, s_b, (size_t)n * sizeof s_a[0]) == 0;
            bool cells_same;

            cloud_cells(px, ez, &c1);
            cloud_cells(qx, qz, &c2);
            cells_same = memcmp(&c1, &c2, sizeof c1) == 0;
            bad += cells_same && !built_same;
            same += cells_same;
            moved += !cells_same;
        }
    }
    CHECK_EQ_INT(bad, 0);
    CHECK_EQ_INT(same > 40, 1);  /* the small steps mostly keep the cells */
    CHECK_EQ_INT(moved > 100, 1); /* a whole pattern away never does */
}

/* Every bit cloud_cells() does not use is 0, whatever was there before, so
 * memcmp() compares only the cells. */
static void test_cells_cleared(void)
{
    CloudCells a, b;
    int k, bad = 0;

    for (k = 0; k < 20; k++) {
        float px = rnd(-2000.0f, 2000.0f), ez = rnd(-2000.0f, 2000.0f);

        memset(&a, 0x00, sizeof a);
        memset(&b, 0xFF, sizeof b);
        cloud_cells(px, ez, &a);
        cloud_cells(px, ez, &b);
        bad += memcmp(&a, &b, sizeof a) != 0;
    }
    CHECK_EQ_INT(bad, 0);
}

void test_cloud_cells(void)
{
    test_table();
    test_moved();
    test_same_cells();
    test_cells_cleared();
}
