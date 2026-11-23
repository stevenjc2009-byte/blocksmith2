#include <math.h>
#include <string.h>

#include "mat4.h"
#include "test.h"

#define TOL 1e-5

/* Send screen point (x, y, z) through the projection and check where it
 * lands in clip space. */
static void check_point(const Mat4 *p, float x, float y, float z,
                        float cx, float cy, float cz)
{
    float in[4] = { x, y, z, 1.0f };
    float out[4];
    int fails_before = g_fails;

    mat4_mul_vec4(p, in, out);
    CHECK_NEAR(out[0], cx, TOL);
    CHECK_NEAR(out[1], cy, TOL);
    CHECK_NEAR(out[2], cz, TOL);
    CHECK_NEAR(out[3], 1.0, TOL);
    if (g_fails != fails_before)
        printf("     (screen point %g, %g, %g)\n", x, y, z);
}

/* out = m * (x, y, z, 1). */
static void apply(const Mat4 *m, float x, float y, float z, float out[4])
{
    float in[4] = { x, y, z, 1.0f };
    mat4_mul_vec4(m, in, out);
}

static void check_vec(const float v[4], float x, float y, float z, float w)
{
    CHECK_NEAR(v[0], x, TOL);
    CHECK_NEAR(v[1], y, TOL);
    CHECK_NEAR(v[2], z, TOL);
    CHECK_NEAR(v[3], w, TOL);
}

static void test_mat4_transforms(void)
{
    const float half_pi = 1.57079632679f;
    Mat4 a, b, ab, t;
    float v[4];
    int r, c;

    mat4_identity(&a);
    apply(&a, 1.0f, -2.0f, 3.0f, v);
    check_vec(v, 1.0f, -2.0f, 3.0f, 1.0f);

    mat4_translate(&a, 1.0f, 2.0f, 3.0f);
    apply(&a, 10.0f, 20.0f, 30.0f, v);
    check_vec(v, 11.0f, 22.0f, 33.0f, 1.0f);

    /* Right-handed turns, counter-clockwise seen from the +axis end. */
    mat4_rotate_y(&a, half_pi);
    apply(&a, 1.0f, 0.0f, 0.0f, v);            /* +x turns to -z */
    check_vec(v, 0.0f, 0.0f, -1.0f, 1.0f);
    apply(&a, 0.0f, 0.0f, 1.0f, v);            /* +z turns to +x */
    check_vec(v, 1.0f, 0.0f, 0.0f, 1.0f);
    apply(&a, 0.0f, 5.0f, 0.0f, v);            /* y is left alone */
    check_vec(v, 0.0f, 5.0f, 0.0f, 1.0f);

    mat4_rotate_x(&a, half_pi);
    apply(&a, 0.0f, 1.0f, 0.0f, v);            /* +y turns to +z */
    check_vec(v, 0.0f, 0.0f, 1.0f, 1.0f);
    apply(&a, 0.0f, 0.0f, 1.0f, v);            /* +z turns to -y */
    check_vec(v, 0.0f, -1.0f, 0.0f, 1.0f);

    /* a * b applies b first: turn, then move. */
    mat4_translate(&a, 5.0f, 0.0f, 0.0f);
    mat4_rotate_y(&b, half_pi);
    mat4_mul(&ab, &a, &b);
    apply(&ab, 1.0f, 0.0f, 0.0f, v);
    check_vec(v, 5.0f, 0.0f, -1.0f, 1.0f);
    /* The other order moves first, then turns. */
    mat4_mul(&ab, &b, &a);
    apply(&ab, 1.0f, 0.0f, 0.0f, v);
    check_vec(v, 0.0f, 0.0f, -6.0f, 1.0f);

    /* Writing the result over an input is allowed. */
    t = a;
    mat4_mul(&t, &t, &b);
    mat4_mul(&ab, &a, &b);
    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++)
            CHECK_NEAR(t.m[r][c], ab.m[r][c], TOL);
}

static void test_mat4_perspective(void)
{
    const float fovy = 1.04719755f;            /* 60 degrees */
    const float aspect = 400.0f / 240.0f;
    const float n = 0.1f, f = 100.0f;
    float half_h = tanf(fovy * 0.5f);          /* frustum half height at distance 1 */
    Mat4 p;
    float v[4];

    mat4_persp_tilt(&p, fovy, aspect, n, f);

    /* Straight ahead on the near plane: centre of the screen, depth -1. */
    apply(&p, 0.0f, 0.0f, -n, v);
    CHECK_NEAR(v[3], n, TOL);
    CHECK_NEAR(v[0] / v[3], 0.0, TOL);
    CHECK_NEAR(v[1] / v[3], 0.0, TOL);
    CHECK_NEAR(v[2] / v[3], -1.0, 1e-4);

    /* On the far plane: depth 0. */
    apply(&p, 0.0f, 0.0f, -f, v);
    CHECK_NEAR(v[2] / v[3], 0.0, 1e-4);

    /* Halfway in distance is much nearer the far end of the depth range. */
    apply(&p, 0.0f, 0.0f, -10.0f, v);
    CHECK_NEAR(v[2] / v[3], -0.00900901, 1e-6);

    /* The top edge of the view (view y up) feeds clip x = +1, because the
     * top screen buffer is turned a quarter turn. */
    apply(&p, 0.0f, 2.0f * half_h, -2.0f, v);
    CHECK_NEAR(v[0] / v[3], 1.0, 1e-4);
    CHECK_NEAR(v[1] / v[3], 0.0, 1e-4);

    /* The right edge (view x right) feeds clip y = -1. */
    apply(&p, 3.0f * half_h * aspect, 0.0f, -3.0f, v);
    CHECK_NEAR(v[0] / v[3], 0.0, 1e-4);
    CHECK_NEAR(v[1] / v[3], -1.0, 1e-4);

    /* The left edge feeds clip y = +1, the bottom edge clip x = -1. */
    apply(&p, -half_h * aspect, -half_h, -1.0f, v);
    CHECK_NEAR(v[0] / v[3], -1.0, 1e-4);
    CHECK_NEAR(v[1] / v[3], 1.0, 1e-4);
}

void test_mat4(void)
{
    Mat4 p;
    uint32_t words[16];
    int r, c;

    /* Top screen in pixels: x 0..400 left to right, y 0..240 top to bottom,
     * depth 0 (near) to 1 (far). */
    mat4_ortho_tilt(&p, 0.0f, 400.0f, 240.0f, 0.0f, 0.0f, 1.0f);

    /* Screen y feeds clip x: top +1, bottom -1.
     * Screen x feeds clip y: left +1, right -1. */
    check_point(&p,   0.0f,   0.0f, 0.0f,  1.0f,  1.0f, -1.0f); /* top-left */
    check_point(&p, 400.0f,   0.0f, 0.0f,  1.0f, -1.0f, -1.0f); /* top-right */
    check_point(&p,   0.0f, 240.0f, 0.0f, -1.0f,  1.0f, -1.0f); /* bottom-left */
    check_point(&p, 400.0f, 240.0f, 0.0f, -1.0f, -1.0f, -1.0f); /* bottom-right */
    check_point(&p, 200.0f, 120.0f, 0.5f,  0.0f,  0.0f, -0.5f); /* centre, half depth */
    check_point(&p, 100.0f,  60.0f, 1.0f,  0.5f,  0.5f,  0.0f); /* on the far plane */

    /* A depth range that does not start at zero: near 2 -> -1, far 10 -> 0. */
    mat4_ortho_tilt(&p, 0.0f, 400.0f, 240.0f, 0.0f, 2.0f, 10.0f);
    check_point(&p, 200.0f, 120.0f,  2.0f, 0.0f, 0.0f, -1.0f);
    check_point(&p, 200.0f, 120.0f, 10.0f, 0.0f, 0.0f,  0.0f);
    check_point(&p, 200.0f, 120.0f,  6.0f, 0.0f, 0.0f, -0.5f);

    /* GPU uniform order: each row is sent w, z, y, x. */
    mat4_uniform_words(&p, words);
    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++) {
            uint32_t bits;
            memcpy(&bits, &p.m[r][c], sizeof bits);
            CHECK_EQ_INT((int)words[r * 4 + (3 - c)], (int)bits);
        }

    test_mat4_transforms();
    test_mat4_perspective();
}
