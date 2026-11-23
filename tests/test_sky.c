#include <math.h>
#include <stdlib.h>

#include "blocktex.h"
#include "mesh.h"
#include "sky.h"
#include "test.h"

/* Row 3-02: day and night (sky.h). */

#define TOL 1e-4

static int red(uint32_t c) { return (int)(c >> 24); }
static int green(uint32_t c) { return (int)((c >> 16) & 0xFF); }
static int blue(uint32_t c) { return (int)((c >> 8) & 0xFF); }

/* The time goes forward by play time and wraps round once a day. */
static void test_advance(void)
{
    float t = SKY_NEW_WORLD;
    int i, bad = 0;

    CHECK_NEAR(sky_advance(0.0f, SKY_DAY_SECONDS / 2.0f), 0.5f, TOL);
    CHECK_NEAR(sky_advance(0.0f, SKY_DAY_SECONDS / 4.0f), 0.25f, TOL);
    CHECK_NEAR(sky_advance(0.9f, SKY_DAY_SECONDS * 0.2f), 0.1f, TOL);
    CHECK_NEAR(sky_advance(0.3f, SKY_DAY_SECONDS), 0.3f, TOL);
    CHECK_NEAR(sky_advance(0.3f, 0.0f), 0.3f, 0.0);
    CHECK_NEAR(SKY_DAY_SECONDS, 1200.0, 0.0); /* 20 minutes, as in the game it follows */
    /* 30 frames a second for a whole day comes back where it began, the
     * time always in 0 .. 1. */
    for (i = 0; i < (int)(SKY_DAY_SECONDS * 30.0f); i++) {
        t = sky_advance(t, 1.0f / 30.0f);
        bad += !(t >= 0.0f && t < 1.0f);
    }
    CHECK_EQ_INT(bad, 0);
    CHECK_NEAR(t, SKY_NEW_WORLD, 2e-3);
    t = sky_advance(0.99999994f, 0.00001f);
    CHECK_EQ_INT(t >= 0.0f && t < 1.0f, 1);
    t = sky_advance(NAN, 1.0f);
    CHECK_EQ_INT(t >= 0.0f && t < 1.0f, 1);
}

/* Where the sun is: east at sunrise, overhead at noon, west at sunset,
 * under at midnight (+x east, +y up). */
static void test_sun_path(void)
{
    SkyLook s;

    sky_look(0.0f, &s);
    CHECK_NEAR(s.sun[0], 1.0, TOL);
    CHECK_NEAR(s.sun[1], 0.0, TOL);
    sky_look(0.25f, &s);
    CHECK_NEAR(s.sun[0], 0.0, TOL);
    CHECK_NEAR(s.sun[1], 1.0, TOL);
    sky_look(0.5f, &s);
    CHECK_NEAR(s.sun[0], -1.0, TOL);
    CHECK_NEAR(s.sun[1], 0.0, TOL);
    sky_look(0.75f, &s);
    CHECK_NEAR(s.sun[1], -1.0, TOL);
    CHECK_NEAR(s.sun[2], 0.0, 0.0);
    CHECK_NEAR(s.sun[0] * s.sun[0] + s.sun[1] * s.sun[1] + s.sun[2] * s.sun[2], 1.0, TOL);
}

/* Day, night and the glow in between. */
static void test_sky_look(void)
{
    SkyLook s;

    /* Noon: the old sky blue, the world as bright as before, the sun up. */
    sky_look(0.25f, &s);
    CHECK_EQ_INT((int)s.clear, (int)((SKY_DAY_RGB << 8) | 0xFF));
    CHECK_EQ_INT((int)s.clear, (int)0x6EAAF0FFu);
    CHECK_NEAR(s.light, 1.0, 0.0);
    CHECK_EQ_INT(s.sun_shown, 1);
    CHECK_EQ_INT(s.moon_shown, 0);
    CHECK_NEAR(s.sun_rgb[0], 1.0, TOL);
    CHECK_NEAR(s.sun_rgb[1], 1.0, TOL);
    CHECK_NEAR(s.sun_rgb[2], 1.0, TOL);

    /* A new world starts in full daylight with the old sky: it looks as
     * it did before row 3-02, and goes on doing so for over 7 minutes. */
    sky_look(SKY_NEW_WORLD, &s);
    CHECK_EQ_INT((int)s.clear, (int)0x6EAAF0FFu);
    CHECK_NEAR(s.light, 1.0, 0.0);
    sky_look(sky_advance(SKY_NEW_WORLD, 7.0f * 60.0f), &s);
    CHECK_EQ_INT((int)s.clear, (int)0x6EAAF0FFu);
    CHECK_NEAR(s.light, 1.0, 0.0);

    /* Midnight: deep blue, the world dimmed, the moon up. */
    sky_look(0.75f, &s);
    CHECK_EQ_INT((int)s.clear, (int)((SKY_NIGHT_RGB << 8) | 0xFF));
    CHECK_NEAR(s.light, SKY_NIGHT_LIGHT, TOL);
    CHECK_EQ_INT(s.sun_shown, 0);
    CHECK_EQ_INT(s.moon_shown, 1);
    CHECK_EQ_INT(blue(s.clear) > red(s.clear) && blue(s.clear) < 64, 1);

    /* Sunrise and sunset: an orange sky (more red than blue), the world
     * half lit, both the sun and moon on the horizon, the sun orange. */
    sky_look(0.0f, &s);
    CHECK_EQ_INT(red(s.clear) > blue(s.clear) + 40, 1);
    CHECK_EQ_INT(red(s.clear) > green(s.clear), 1);
    CHECK_NEAR(s.light, (1.0 + SKY_NIGHT_LIGHT) / 2.0, TOL);
    CHECK_EQ_INT(s.sun_shown && s.moon_shown, 1);
    CHECK_EQ_INT(s.sun_rgb[2] < 0.6f && s.sun_rgb[1] < 0.8f, 1);
    sky_look(0.5f, &s);
    CHECK_EQ_INT(red(s.clear) > blue(s.clear) + 40, 1);
    CHECK_NEAR(s.light, (1.0 + SKY_NIGHT_LIGHT) / 2.0, TOL);
    CHECK_EQ_INT(s.sun_shown && s.moon_shown, 1);

    /* Every alpha is solid. */
    CHECK_EQ_INT((int)(s.clear & 0xFF), 0xFF);
}

/* Nothing jumps: over a whole day in steps of a tenth of a second the light
 * and the sky's colour change only a little each step; the light only falls
 * from noon to midnight and only rises after. The sun and moon come and go
 * once each. */
static void test_smooth(void)
{
    const int steps = (int)(SKY_DAY_SECONDS * 10.0f);
    SkyLook a, b;
    int i, jumps = 0, wrong_way = 0, sun_changes = 0, moon_changes = 0, never_both_hidden = 1;

    sky_look(0.0f, &a);
    for (i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;

        sky_look(t >= 1.0f ? 0.0f : t, &b);
        if (fabsf(b.light - a.light) > 0.002f || abs(red(b.clear) - red(a.clear)) > 1 ||
            abs(green(b.clear) - green(a.clear)) > 1 || abs(blue(b.clear) - blue(a.clear)) > 1)
            jumps++;
        if ((t > 0.25f && t <= 0.75f && b.light > a.light + 1e-6f) ||
            ((t <= 0.25f || t > 0.75f) && b.light < a.light - 1e-6f))
            wrong_way++;
        sun_changes += b.sun_shown != a.sun_shown;
        moon_changes += b.moon_shown != a.moon_shown;
        if (!b.sun_shown && !b.moon_shown)
            never_both_hidden = 0;
        a = b;
    }
    CHECK_EQ_INT(jumps, 0);
    CHECK_EQ_INT(wrong_way, 0);
    CHECK_EQ_INT(sun_changes, 2);
    CHECK_EQ_INT(moon_changes, 2);
    CHECK_EQ_INT(never_both_hidden, 1);
}

/* (b - a) x (c - a) . (eye - a): positive when a, b, c go counter-clockwise
 * seen from the eye at the origin. */
static float facing(const MeshVertex *a, const MeshVertex *b, const MeshVertex *c)
{
    float e[3] = { b->x - a->x, b->y - a->y, b->z - a->z };
    float f[3] = { c->x - a->x, c->y - a->y, c->z - a->z };
    float n[3] = { e[1] * f[2] - e[2] * f[1], e[2] * f[0] - e[0] * f[2], e[0] * f[1] - e[1] * f[0] };

    return -(n[0] * a->x + n[1] * a->y + n[2] * a->z);
}

/* One square of sky_quads(): faces the eye, centred SKY_DIST along dir,
 * `half` to each edge, inside the far plane, showing `tile` whole in colour
 * rgb. */
static void check_quad(const MeshVertex *q, const float dir[3], float half, int tile,
                       const float rgb[3])
{
    float uv[4], umin = 9, umax = -9, vmin = 9, vmax = -9, far = 0;
    int i;

    CHECK_EQ_INT(facing(&q[0], &q[1], &q[2]) > 0.0f, 1);
    CHECK_EQ_INT(facing(&q[3], &q[4], &q[5]) > 0.0f, 1);
    for (i = 0; i < MESH_FACE_VERTS; i++) {
        float d = sqrtf(q[i].x * q[i].x + q[i].y * q[i].y + q[i].z * q[i].z);

        umin = fminf(umin, q[i].u);
        umax = fmaxf(umax, q[i].u);
        vmin = fminf(vmin, q[i].v);
        vmax = fmaxf(vmax, q[i].v);
        far = fmaxf(far, d);
        CHECK_NEAR(q[i].r, rgb[0], TOL);
        CHECK_NEAR(q[i].g, rgb[1], TOL);
        CHECK_NEAR(q[i].b, rgb[2], TOL);
    }
    /* The two triangles share corners 0 and 2, so the six vertices' mean is
     * pulled towards that diagonal; its middle is the square's middle. */
    CHECK_NEAR((q[0].x + q[2].x) / 2, dir[0] * SKY_DIST, 1e-3);
    CHECK_NEAR((q[0].y + q[2].y) / 2, dir[1] * SKY_DIST, 1e-3);
    CHECK_NEAR((q[0].z + q[2].z) / 2, dir[2] * SKY_DIST, 1e-3);
    CHECK_NEAR(sqrtf((q[1].x - q[0].x) * (q[1].x - q[0].x) + (q[1].y - q[0].y) * (q[1].y - q[0].y) +
                     (q[1].z - q[0].z) * (q[1].z - q[0].z)), 2 * half, 1e-3);
    CHECK_NEAR(sqrtf((q[2].x - q[1].x) * (q[2].x - q[1].x) + (q[2].y - q[1].y) * (q[2].y - q[1].y) +
                     (q[2].z - q[1].z) * (q[2].z - q[1].z)), 2 * half, 1e-3);
    CHECK_EQ_INT(far < 159.0f, 1);
    blocktex_tile_uv(tile, uv);
    CHECK_NEAR(umin, uv[0], 0.0);
    CHECK_NEAR(umax, uv[2], 0.0);
    CHECK_NEAR(vmin, uv[1], 0.0);
    CHECK_NEAR(vmax, uv[3], 0.0);
}

static void test_quads(void)
{
    static const float white[3] = { 1, 1, 1 };
    MeshVertex q[SKY_QUADS_MAX];
    SkyLook s;
    float moon[3];
    const float ts[] = { 0.1f, 0.2f, 0.4f, 0.0f, 0.5f, 0.6f, 0.75f, 0.9f };
    int k;

    for (k = 0; k < (int)(sizeof ts / sizeof ts[0]); k++) {
        int n, at = 0;

        sky_look(ts[k], &s);
        n = sky_quads(&s, q);
        CHECK_EQ_INT(n, MESH_FACE_VERTS * (s.sun_shown + s.moon_shown));
        moon[0] = -s.sun[0];
        moon[1] = -s.sun[1];
        moon[2] = -s.sun[2];
        if (s.sun_shown) {
            check_quad(q, s.sun, SKY_SUN_HALF, TILE_SUN, s.sun_rgb);
            at = MESH_FACE_VERTS;
        }
        if (s.moon_shown)
            check_quad(q + at, moon, SKY_MOON_HALF, TILE_MOON, white);
    }
    /* Noon: only the sun, straight up. Midnight: only the moon. */
    sky_look(0.25f, &s);
    CHECK_EQ_INT(sky_quads(&s, q), MESH_FACE_VERTS);
    CHECK_NEAR(q[0].y, SKY_DIST, 1e-3);
    sky_look(0.75f, &s);
    CHECK_EQ_INT(sky_quads(&s, q), MESH_FACE_VERTS);
    CHECK_NEAR(q[0].y, SKY_DIST, 1e-3);
    CHECK_NEAR(q[0].r, 1.0, 0.0);
}

/* The sun and moon pictures (blocktex.h): the sun warm and bright, the moon
 * pale with darker craters. */
static void test_tiles(void)
{
    static uint32_t atlas[BLOCKTEX_SIZE * BLOCKTEX_SIZE];
    int x, y, sun_ok = 1, moon_pale = 0, moon_dark = 0, moon_grey = 1;
    int ox = (TILE_SUN % BLOCKTEX_PER_ROW) * BLOCKTEX_TILE, oy = (TILE_SUN / BLOCKTEX_PER_ROW) * BLOCKTEX_TILE;
    int mx = (TILE_MOON % BLOCKTEX_PER_ROW) * BLOCKTEX_TILE, my = (TILE_MOON / BLOCKTEX_PER_ROW) * BLOCKTEX_TILE;

    blocktex_build(atlas);
    for (y = 0; y < BLOCKTEX_TILE; y++)
        for (x = 0; x < BLOCKTEX_TILE; x++) {
            uint32_t s = atlas[(oy + y) * BLOCKTEX_SIZE + ox + x];
            uint32_t m = atlas[(my + y) * BLOCKTEX_SIZE + mx + x];

            if (!(red(s) >= 240 && green(s) >= 180 && blue(s) < green(s) && (s & 0xFF) == 0xFF))
                sun_ok = 0;
            if (red(m) >= 190)
                moon_pale++;
            else if (red(m) < 180)
                moon_dark++;
            if (abs(red(m) - green(m)) > 16 || blue(m) < red(m) || (m & 0xFF) != 0xFF)
                moon_grey = 0;
        }
    CHECK_EQ_INT(sun_ok, 1);
    CHECK_EQ_INT(moon_grey, 1);
    CHECK_EQ_INT(moon_pale > 150, 1);
    CHECK_EQ_INT(moon_dark >= 12 && moon_dark < 80, 1);
    CHECK_EQ_INT(TILE_SUN < BLOCKTEX_PER_ROW * BLOCKTEX_PER_ROW, 1);
    CHECK_EQ_INT(TILE_MOON != TILE_SUN, 1);
}

void test_sky(void)
{
    test_advance();
    test_sun_path();
    test_sky_look();
    test_smooth();
    test_quads();
    test_tiles();
}
