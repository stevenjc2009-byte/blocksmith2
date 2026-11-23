#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include "blocktex.h"
#include "block.h"
#include "cloud.h"
#include "mesh.h"
#include "test.h"

/* Row 3-04: the clouds (cloud.h). */

#define TOL 1e-3
#define FAR_PLANE 160.0f /* main.c's projection */

static MeshVertex s_out[CLOUD_VERTS_MAX];

/* The pattern: the same every time, repeating every CLOUD_PERIOD cells both
 * ways, a quarter to a half of the sky cloud, and in clumps: next-door
 * cells agree far more often than a speckle with the same share would. */
static void test_pattern(void)
{
    int i, j, cloud = 0, agree = 0, pairs = 0, rep_bad = 0;
    double p, chance, seen;

    for (j = 0; j < CLOUD_PERIOD; j++)
        for (i = 0; i < CLOUD_PERIOD; i++) {
            bool c = cloud_at(i, j);

            cloud += c;
            if (c != cloud_at(i, j) || c != cloud_at(i + CLOUD_PERIOD, j) ||
                c != cloud_at(i, j - CLOUD_PERIOD) ||
                c != cloud_at(i - 7 * CLOUD_PERIOD, j + 3 * CLOUD_PERIOD))
                rep_bad++;
            agree += (c == cloud_at(i + 1, j)) + (c == cloud_at(i, j + 1));
            pairs += 2;
        }
    p = (double)cloud / (CLOUD_PERIOD * CLOUD_PERIOD);
    chance = p * p + (1.0 - p) * (1.0 - p);
    seen = (double)agree / pairs;
    printf("     (clouds: %.1f%% of the sky; next-door cells agree %.2f, a speckle %.2f)\n",
           100.0 * p, seen, chance);
    CHECK_EQ_INT(rep_bad, 0);
    CHECK_EQ_INT(p >= 0.25 && p <= 0.5, 1);
    CHECK_EQ_INT(seen >= chance + 0.1, 1);
}

/* The drift: none at t 0, one whole period at t 1, evenly between, so the
 * day wrapping round from just under 1 to 0 leaves every cloud where it
 * was. */
static void test_shift(void)
{
    static MeshVertex late[CLOUD_VERTS_MAX];
    const float eye[3][2] = { { 5.3f, -2.7f }, { -1234.6f, 777.1f }, { 40000.4f, -9000.2f } };
    int k, v, n0, n1, bad = 0;

    CHECK_NEAR(cloud_shift(0.0f), 0.0, 0.0);
    CHECK_NEAR(cloud_shift(1.0f), CLOUD_PERIOD * CLOUD_CELL, TOL);
    CHECK_NEAR(cloud_shift(0.5f), CLOUD_PERIOD * CLOUD_CELL / 2.0, TOL);
    CHECK_NEAR(cloud_shift(0.25f), CLOUD_PERIOD * CLOUD_CELL / 4.0, TOL);
    for (k = 0; k < 3; k++) {
        n0 = cloud_build(eye[k][0], eye[k][1], 0.0f, s_out);
        n1 = cloud_build(eye[k][0], eye[k][1], 1.0f - 1e-7f, late);
        CHECK_EQ_INT(n1, n0);
        CHECK_EQ_INT(n0 > 0, 1);
        for (v = 0; v < n0 && v < n1; v++)
            if (fabsf(s_out[v].x - late[v].x) > 0.01f || s_out[v].y != late[v].y ||
                s_out[v].z != late[v].z)
                bad++;
        CHECK_EQ_INT(bad, 0);
    }
}

static void sub3(const MeshVertex *a, const MeshVertex *b, float out[3])
{
    out[0] = a->x - b->x;
    out[1] = a->y - b->y;
    out[2] = a->z - b->z;
}

/* Everything cloud_build() writes, for many eyes and times: whole faces of
 * whole cells, each cell cloud in the pattern (after the drift) and within
 * CLOUD_RADIUS, every such cell there, its bottom and top always, a side
 * just where the next cell is clear; faces facing out (counter-clockwise
 * seen from outside), flat white tile, the brightness by face, no torch. */
static void check_build(float ex, float ez, float t, int *worst)
{
    float uv[4], shift = cloud_shift(t), px = ex - shift;
    int n = cloud_build(ex, ez, t, s_out);
    int f, c, cells = 0, want = 0, bad_face = 0, bad_cell = 0, sides = 0, want_sides = 0;
    int bad_side = 0;
    int i, j, ci = 0, cj = 0;
    float x0 = 0.0f, z0 = 0.0f;

    blocktex_tile_uv(TILE_CLOUD, uv);
    if (n > *worst)
        *worst = n;
    CHECK_EQ_INT(n % MESH_FACE_VERTS, 0);
    CHECK_EQ_INT(n <= CLOUD_VERTS_MAX, 1);
    for (f = 0; f + MESH_FACE_VERTS <= n; f += MESH_FACE_VERTS) {
        const MeshVertex *v = &s_out[f];
        float e1[3], e2[3], nrm[3], mid[3] = { 0, 0, 0 }, out;
        float shade = v[0].y == v[1].y && v[1].y == v[2].y
                          ? (v[0].y > CLOUD_Y ? 1.0f : 0.8f) : 0.9f;

        for (c = 0; c < MESH_FACE_VERTS; c++) {
            mid[0] += v[c].x / MESH_FACE_VERTS;
            mid[1] += v[c].y / MESH_FACE_VERTS;
            mid[2] += v[c].z / MESH_FACE_VERTS;
        }
        if (shade == 0.8f) { /* a bottom: a new cell */
            float dx, dz;

            x0 = fminf(v[0].x, v[2].x);
            z0 = fminf(v[0].z, v[2].z);
            ci = (int)floorf((x0 - shift) / CLOUD_CELL + 0.5f);
            cj = (int)floorf(z0 / CLOUD_CELL + 0.5f);
            dx = fminf(fmaxf(px, ci * (float)CLOUD_CELL), (ci + 1) * (float)CLOUD_CELL) - px;
            dz = fminf(fmaxf(ez, cj * (float)CLOUD_CELL), (cj + 1) * (float)CLOUD_CELL) - ez;
            if (fabsf(x0 - shift - ci * CLOUD_CELL) > 0.01f || fabsf(z0 - cj * CLOUD_CELL) > 0.01f ||
                !cloud_at(ci, cj) || dx * dx + dz * dz > CLOUD_RADIUS * CLOUD_RADIUS + 0.01f)
                bad_cell++;
            cells++;
            want_sides += !cloud_at(ci + 1, cj) + !cloud_at(ci - 1, cj) + !cloud_at(ci, cj + 1) +
                          !cloud_at(ci, cj - 1);
        } else if (shade == 0.9f) {
            /* Which side, by where it stands; the cell across it is clear. */
            int di = fabsf(mid[0] - x0 - CLOUD_CELL) < 1.0f ? 1 : fabsf(mid[0] - x0) < 1.0f ? -1 : 0;
            int dj = fabsf(mid[2] - z0 - CLOUD_CELL) < 1.0f ? 1 : fabsf(mid[2] - z0) < 1.0f ? -1 : 0;

            bad_side += (di != 0) == (dj != 0) || cloud_at(ci + di, cj + dj);
            sides++;
        }
        for (c = 0; c < MESH_FACE_VERTS; c++) {
            if (v[c].x < x0 - TOL || v[c].x > x0 + CLOUD_CELL + TOL || v[c].z < z0 - TOL ||
                v[c].z > z0 + CLOUD_CELL + TOL ||
                (v[c].y != CLOUD_Y && v[c].y != CLOUD_Y + CLOUD_THICK) || v[c].t != 0.0f ||
                v[c].r != shade || v[c].g != shade || v[c].b != shade ||
                v[c].u < uv[0] - TOL || v[c].u > uv[2] + TOL || v[c].v < uv[1] - TOL ||
                v[c].v > uv[3] + TOL)
                bad_face++;
        }
        /* Both triangles face away from the middle of the cell's slab. */
        for (c = 0; c < MESH_FACE_VERTS; c += 3) {
            sub3(&v[c + 1], &v[c], e1);
            sub3(&v[c + 2], &v[c], e2);
            nrm[0] = e1[1] * e2[2] - e1[2] * e2[1];
            nrm[1] = e1[2] * e2[0] - e1[0] * e2[2];
            nrm[2] = e1[0] * e2[1] - e1[1] * e2[0];
            out = nrm[0] * (mid[0] - (x0 + CLOUD_CELL / 2.0f)) +
                  nrm[1] * (mid[1] - (CLOUD_Y + CLOUD_THICK / 2.0f)) +
                  nrm[2] * (mid[2] - (z0 + CLOUD_CELL / 2.0f));
            if (!(out > 1.0f))
                bad_face++;
        }
    }
    /* Every cloud cell within the radius is there. */
    for (j = (int)floorf((ez - CLOUD_RADIUS) / CLOUD_CELL) - 1;
         j <= (int)floorf((ez + CLOUD_RADIUS) / CLOUD_CELL) + 1; j++)
        for (i = (int)floorf((px - CLOUD_RADIUS) / CLOUD_CELL) - 1;
             i <= (int)floorf((px + CLOUD_RADIUS) / CLOUD_CELL) + 1; i++) {
            float dx = fminf(fmaxf(px, i * (float)CLOUD_CELL), (i + 1) * (float)CLOUD_CELL) - px;
            float dz = fminf(fmaxf(ez, j * (float)CLOUD_CELL), (j + 1) * (float)CLOUD_CELL) - ez;

            if (cloud_at(i, j) && dx * dx + dz * dz <= CLOUD_RADIUS * CLOUD_RADIUS)
                want++;
        }
    CHECK_EQ_INT(bad_face, 0);
    CHECK_EQ_INT(bad_cell, 0);
    CHECK_EQ_INT(cells, want);
    CHECK_EQ_INT(sides, want_sides);
    CHECK_EQ_INT(bad_side, 0);
    CHECK_EQ_INT(n, (2 * cells + sides) * MESH_FACE_VERTS);
}

static void test_build(void)
{
    unsigned r = 12345;
    int k, worst = 0;

    check_build(0.5f, 0.5f, 0.06f, &worst);
    check_build(-0.5f, 0.5f, 0.0f, &worst);
    for (k = 0; k < 60; k++) {
        float ex, ez, t;

        r = r * 1103515245u + 12345u;
        ex = (float)((int)(r >> 8) % 200000 - 100000) / 10.0f;
        r = r * 1103515245u + 12345u;
        ez = (float)((int)(r >> 8) % 200000 - 100000) / 10.0f;
        r = r * 1103515245u + 12345u;
        t = (float)((r >> 8) % 10000) / 10000.0f;
        check_build(ex, ez, t, &worst);
    }
    printf("     (clouds: at most %d vertices over the eyes tried)\n", worst);
}

/* CLOUD_VERTS_MAX is enough wherever the eye is: for each cell the eye can
 * be in (the pattern repeats, so CLOUD_PERIOD x CLOUD_PERIOD of them), every
 * cloud cell any eye in it could draw (its nearest point no further than
 * CLOUD_RADIUS from that cell) with all its faces it could have. */
static void test_most(void)
{
    const int reach = (int)(CLOUD_RADIUS / CLOUD_CELL) + 2;
    int a, b, i, j, most = 0;

    for (b = 0; b < CLOUD_PERIOD; b++)
        for (a = 0; a < CLOUD_PERIOD; a++) {
            int verts = 0;

            for (j = b - reach; j <= b + reach; j++)
                for (i = a - reach; i <= a + reach; i++) {
                    float gx = (float)((abs(i - a) - 1 > 0 ? abs(i - a) - 1 : 0) * CLOUD_CELL);
                    float gz = (float)((abs(j - b) - 1 > 0 ? abs(j - b) - 1 : 0) * CLOUD_CELL);

                    if (!cloud_at(i, j) || gx * gx + gz * gz > CLOUD_RADIUS * CLOUD_RADIUS)
                        continue;
                    verts += (2 + !cloud_at(i + 1, j) + !cloud_at(i - 1, j) + !cloud_at(i, j + 1) +
                              !cloud_at(i, j - 1)) * MESH_FACE_VERTS;
                }
            if (verts > most)
                most = verts;
        }
    printf("     (clouds: at most %d vertices from any eye, room for %d)\n", most, CLOUD_VERTS_MAX);
    CHECK_EQ_INT(most <= CLOUD_VERTS_MAX, 1);
    CHECK_EQ_INT(most > CLOUD_VERTS_MAX / 2, 1); /* not wasting much memory either */
}

/* The layer: above the highest ground, every drawn corner inside the far
 * plane even from the bottom of the world, and the fog done by the radius. */
static void test_layer(void)
{
    float far_x = CLOUD_RADIUS + CLOUD_CELL * 1.4143f, far_y = CLOUD_Y + CLOUD_THICK;

    CHECK_EQ_INT(CLOUD_Y > 71.0f + 20.0f, 1);
    CHECK_EQ_INT(CLOUD_Y + CLOUD_THICK < (float)WORLD_HEIGHT, 1);
    CHECK_EQ_INT(sqrtf(far_x * far_x + far_y * far_y) < FAR_PLANE, 1);
    CHECK_EQ_INT(CLOUD_FOG_START > 0.0f && CLOUD_FOG_START < CLOUD_RADIUS, 1);
}

void test_cloud(void)
{
    test_pattern();
    test_shift();
    test_build();
    test_most();
    test_layer();
    test_cloud_cells();
}
