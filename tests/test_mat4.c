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
}
