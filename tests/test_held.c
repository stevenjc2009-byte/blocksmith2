#include <math.h>
#include <string.h>

#include "block.h"
#include "held.h"
#include "mat4.h"
#include "mesh.h"
#include "test.h"

/* Row 3-05: the held block (held.h). */

#define TOL 1e-4
#define PI 3.14159265f

static MeshVertex s_v[MESH_BLOCK_VERTS];

/* Still, then dipping to 1 halfway through HELD_SWING_TIME and back to 0
 * at its end; a new press mid-swing starts it again. */
static void test_swing(void)
{
    Held h;

    held_init(&h);
    CHECK_NEAR(held_dip(&h), 0.0, 0.0);
    held_step(&h, 0.1f);
    CHECK_NEAR(held_dip(&h), 0.0, 0.0);
    held_swing(&h);
    CHECK_NEAR(held_dip(&h), 0.0, TOL);
    held_step(&h, HELD_SWING_TIME / 4.0f);
    CHECK_NEAR(held_dip(&h), sinf(PI / 4.0f), TOL);
    held_step(&h, HELD_SWING_TIME / 4.0f);
    CHECK_NEAR(held_dip(&h), 1.0, TOL);
    held_step(&h, HELD_SWING_TIME / 4.0f);
    CHECK_NEAR(held_dip(&h), sinf(PI / 4.0f), TOL);
    held_swing(&h);
    CHECK_NEAR(held_dip(&h), 0.0, TOL);
    held_step(&h, HELD_SWING_TIME / 2.0f);
    CHECK_NEAR(held_dip(&h), 1.0, TOL);
    held_step(&h, HELD_SWING_TIME / 2.0f + 0.001f);
    CHECK_NEAR(held_dip(&h), 0.0, 0.0);
    CHECK_NEAR(h.swing, 0.0, 0.0);
    held_step(&h, 1.0f);
    CHECK_NEAR(held_dip(&h), 0.0, 0.0);
}

/* held_build() against mesh_block(): the same vertices moved by half a
 * block on each axis, torch light raised to at least t. Returns the
 * vertices written, or -1 if any is wrong. */
static int build_checked(int block, float t)
{
    static MeshVertex ref[MESH_BLOCK_VERTS];
    int n = held_build(block, t, s_v), m = mesh_block(ref, 0.0f, 0.0f, 0.0f, block), k;

    if (n != m)
        return -1;
    for (k = 0; k < n; k++) {
        const MeshVertex *a = &s_v[k], *b = &ref[k];
        float want_t = b->t < t ? t : b->t;

        if (fabsf(a->x - (b->x - 0.5f)) > TOL || fabsf(a->y - (b->y - 0.5f)) > TOL ||
            fabsf(a->z - (b->z - 0.5f)) > TOL || a->u != b->u || a->v != b->v ||
            a->r != b->r || a->g != b->g || a->b != b->b || a->t != want_t)
            return -1;
    }
    return n;
}

static void test_build(void)
{
    int k;
    float lo = 9.0f, hi = -9.0f;

    CHECK_EQ_INT(held_build(BLOCK_AIR, 0.0f, s_v), 0);
    CHECK_EQ_INT(held_build(BLOCK_COUNT, 0.0f, s_v), 0);
    CHECK_EQ_INT(held_build(-1, 0.0f, s_v), 0);
    CHECK_EQ_INT(build_checked(BLOCK_GRASS, 0.3f), MESH_BLOCK_VERTS);
    CHECK_EQ_INT(build_checked(BLOCK_DIRT, 0.0f), MESH_BLOCK_VERTS);
    CHECK_EQ_INT(build_checked(BLOCK_STONE, 1.0f), MESH_BLOCK_VERTS);
    CHECK_EQ_INT(build_checked(BLOCK_TORCH, 0.2f), MESH_TORCH_VERTS);
    for (k = 0; k < MESH_TORCH_VERTS; k++) /* a torch keeps its glow */
        CHECK_EQ_INT(s_v[k].t > 0.2f, 1);
    build_checked(BLOCK_STONE, 0.6f);
    for (k = 0; k < MESH_BLOCK_VERTS; k++) {
        CHECK_NEAR(s_v[k].t, 0.6f, 0.0);
        lo = s_v[k].x < lo ? s_v[k].x : lo;
        lo = s_v[k].y < lo ? s_v[k].y : lo;
        lo = s_v[k].z < lo ? s_v[k].z : lo;
        hi = s_v[k].x > hi ? s_v[k].x : hi;
        hi = s_v[k].y > hi ? s_v[k].y : hi;
        hi = s_v[k].z > hi ? s_v[k].z : hi;
    }
    CHECK_NEAR(lo, -0.5, TOL); /* its middle at the origin */
    CHECK_NEAR(hi, 0.5, TOL);
}

/* Main.c's projection, and point p (view space) through it to clip space. */
static void project(const Mat4 *place, const float p[3], float out[4])
{
    Mat4 proj, m;
    float v[4] = { p[0], p[1], p[2], 1.0f };

    mat4_persp_tilt(&proj, 60.0f * PI / 180.0f, 400.0f / 240.0f, 0.1f, 160.0f);
    mat4_mul(&m, &proj, place);
    mat4_mul_vec4(&m, v, out);
}

/* The still held block: every corner on the screen and in front of the
 * eye, all in the bottom-right quarter (the 3DS picture lies on its side:
 * clip x below 0 is the bottom half, clip y below 0 the right half, as the
 * first two checks show), a sixth to a half of the screen tall; its top
 * and two sides face the eye, its bottom does not. */
static void test_still_place(void)
{
    static const float k_right[3] = { 1.0f, 0.0f, -2.0f }, k_down[3] = { 0.0f, -1.0f, -2.0f };
    Mat4 still, ident;
    Held h;
    float o[4], xlo = 9.0f, xhi = -9.0f;
    int c, f, off = 0, quarter = 0, facing = 0, top = 0, bottom = 0;

    mat4_identity(&ident);
    project(&ident, k_right, o);
    CHECK_EQ_INT(o[1] < 0.0f && fabsf(o[0]) < TOL, 1);
    project(&ident, k_down, o);
    CHECK_EQ_INT(o[0] < 0.0f && fabsf(o[1]) < TOL, 1);

    held_init(&h);
    held_place(&still, &h);
    for (c = 0; c < 8; c++) {
        float p[3] = { c & 1 ? 0.5f : -0.5f, c & 2 ? 0.5f : -0.5f, c & 4 ? 0.5f : -0.5f };

        project(&still, p, o);
        off += !(o[3] > 0.1f && fabsf(o[0]) < o[3] && fabsf(o[1]) < o[3]);
        quarter += !(o[0] < 0.0f && o[1] < 0.0f);
        xlo = o[0] / o[3] < xlo ? o[0] / o[3] : xlo;
        xhi = o[0] / o[3] > xhi ? o[0] / o[3] : xhi;
    }
    CHECK_EQ_INT(off, 0);
    CHECK_EQ_INT(quarter, 0);
    CHECK_EQ_INT((xhi - xlo) / 2.0f > 1.0f / 6.0f && (xhi - xlo) / 2.0f < 0.5f, 1);
    printf("     (held block: %.0f of the 240 pixels tall)\n", (xhi - xlo) / 2.0f * 240.0f);

    CHECK_EQ_INT(held_build(BLOCK_STONE, 0.0f, s_v), MESH_BLOCK_VERTS);
    for (f = 0; f < MESH_BLOCK_VERTS / MESH_FACE_VERTS; f++) {
        float a[4], b[4], q[4], e1[3], e2[3], n[3], up;
        const MeshVertex *v = &s_v[f * MESH_FACE_VERTS];
        float pa[4] = { v[0].x, v[0].y, v[0].z, 1.0f }, pb[4] = { v[1].x, v[1].y, v[1].z, 1.0f },
              pq[4] = { v[2].x, v[2].y, v[2].z, 1.0f };
        int k, faces;

        up = (v[0].y + v[1].y + v[2].y) / 3.0f; /* +0.5 the top, -0.5 the bottom */
        mat4_mul_vec4(&still, pa, a);
        mat4_mul_vec4(&still, pb, b);
        mat4_mul_vec4(&still, pq, q);
        for (k = 0; k < 3; k++) {
            e1[k] = b[k] - a[k];
            e2[k] = q[k] - a[k];
        }
        n[0] = e1[1] * e2[2] - e1[2] * e2[1];
        n[1] = e1[2] * e2[0] - e1[0] * e2[2];
        n[2] = e1[0] * e2[1] - e1[1] * e2[0];
        faces = -(n[0] * a[0] + n[1] * a[1] + n[2] * a[2]) > 0.0f; /* towards the eye at 0 */
        facing += faces;
        top += faces && up > 0.49f;
        bottom += faces && up < -0.49f;
    }
    CHECK_EQ_INT(facing, 3);
    CHECK_EQ_INT(top, 1);
    CHECK_EQ_INT(bottom, 0);
}

/* At the swing's deepest the block is lower and further forward, its top
 * tipped forward; when the swing ends it is back exactly where it was. */
static void test_swing_place(void)
{
    Mat4 still, deep, end;
    Held h;
    float top[4] = { 0.0f, 0.5f, 0.0f, 1.0f }, ts[4], td[4];

    held_init(&h);
    held_place(&still, &h);
    held_swing(&h);
    held_step(&h, HELD_SWING_TIME / 2.0f);
    held_place(&deep, &h);
    CHECK_EQ_INT(deep.m[1][3] < still.m[1][3] - 0.1f, 1);
    CHECK_EQ_INT(deep.m[2][3] < still.m[2][3] - 0.05f, 1);
    mat4_mul_vec4(&still, top, ts);
    mat4_mul_vec4(&deep, top, td);
    CHECK_EQ_INT(td[2] - deep.m[2][3] < ts[2] - still.m[2][3] - 0.05f, 1);
    held_step(&h, HELD_SWING_TIME);
    held_place(&end, &h);
    CHECK_EQ_INT(memcmp(&end, &still, sizeof end), 0);
}

void test_held(void)
{
    test_swing();
    test_build();
    test_still_place();
    test_swing_place();
}
