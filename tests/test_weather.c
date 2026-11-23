#include <math.h>
#include <string.h>

#include "sky.h"
#include "test.h"
#include "weather.h"

/* Weather (row 3-06): the spells of rain and clear, the cold map and the
 * sky it greys; the drops it builds are in test_weather_drops.c. */

static int in_range(float v, float lo, float hi)
{
    return v >= lo && v <= hi;
}

static void test_spells(void)
{
    Weather w, w2;
    float lo = 1e9f, hi = 0.0f, left[20];
    int s, i, distinct = 0;
    const float bad[3] = { 0.0f, -1.0f, NAN };

    weather_init(&w, 1234u);
    CHECK_EQ_INT(w.raining, 0);
    CHECK_NEAR(w.strength, 0.0, 0.0);
    CHECK_NEAR(w.clock, 0.0, 0.0);
    CHECK_EQ_INT(in_range(w.left, WEATHER_CLEAR_MIN, WEATHER_CLEAR_MAX), 1);
    weather_init(&w2, 1234u);
    CHECK_EQ_INT(w2.left == w.left && w2.rng == w.rng, 1);
    for (s = 0; s < 200; s++) {
        weather_init(&w2, (uint32_t)s);
        lo = w2.left < lo ? w2.left : lo;
        hi = w2.left > hi ? w2.left : hi;
        if (s < 20) {
            left[s] = w2.left;
            for (i = 0; i < s && left[i] != left[s]; i++)
                ;
            distinct += i == s;
        }
    }
    CHECK_EQ_INT(distinct >= 18, 1);
    CHECK_EQ_INT(lo < WEATHER_CLEAR_MIN + 100.0f && hi > WEATHER_CLEAR_MAX - 100.0f, 1);
    /* No time, time going back, or not a number: nothing changes. */
    for (i = 0; i < 3; i++) {
        w2 = w;
        weather_step(&w2, bad[i]);
        CHECK_EQ_INT(memcmp(&w2, &w, sizeof w), 0);
    }
}

static void test_turns(void)
{
    Weather w;
    int i;

    weather_init(&w, 7u);
    w.left = 5.0f;
    weather_step(&w, 5.0f); /* the spell ends: rain, coming in */
    CHECK_EQ_INT(w.raining, 1);
    CHECK_EQ_INT(in_range(w.left, WEATHER_RAIN_MIN, WEATHER_RAIN_MAX), 1);
    CHECK_NEAR(w.strength, 5.0f / WEATHER_FADE, 1e-6);
    for (i = 0; i < 4; i++)
        weather_step(&w, 1.0f);
    CHECK_NEAR(w.strength, 9.0f / WEATHER_FADE, 1e-5);
    weather_step(&w, 1.0f);
    weather_step(&w, 1.0f);
    CHECK_NEAR(w.strength, 1.0, 0.0); /* full, and no further */
    w.left = 2.0f;
    weather_step(&w, 3.0f); /* clear again, going */
    CHECK_EQ_INT(w.raining, 0);
    CHECK_EQ_INT(in_range(w.left, WEATHER_CLEAR_MIN - 1.0f, WEATHER_CLEAR_MAX - 1.0f), 1);
    CHECK_NEAR(w.strength, 1.0f - 3.0f / WEATHER_FADE, 1e-6);
    weather_step(&w, 20.0f);
    CHECK_NEAR(w.strength, 0.0, 0.0);
    weather_step(&w, 5000.0f); /* several spells at once */
    CHECK_EQ_INT(w.left > 0.0f, 1);
    /* The clock comes round to 0. */
    w.clock = WEATHER_CLOCK_WRAP - 0.05f;
    weather_step(&w, 0.1f);
    CHECK_NEAR(w.clock, 0.05, 1e-3);
}

/* 100 hours of play, a tenth of a second at a time. */
static void test_long_run(void)
{
    const float dt = 0.1f;
    Weather w;
    long i, steps = (long)(100.0f * 3600.0f / dt), wet = 0, turns = 0;
    int was, bad_spell = 0, bad_value = 0;
    float spell = 0.0f;

    weather_init(&w, 99u);
    was = w.raining;
    for (i = 0; i < steps; i++) {
        weather_step(&w, dt);
        spell += dt;
        wet += w.raining;
        if (w.raining != was) {
            float lo = was ? WEATHER_RAIN_MIN : WEATHER_CLEAR_MIN;
            float hi = was ? WEATHER_RAIN_MAX : WEATHER_CLEAR_MAX;

            if (turns > 0 && !in_range(spell, lo - 0.2f, hi + 0.2f))
                bad_spell++;
            turns++;
            spell = 0.0f;
            was = w.raining;
        }
        bad_value += !in_range(w.strength, 0.0f, 1.0f) ||
                     !(w.clock >= 0.0f && w.clock < WEATHER_CLOCK_WRAP);
    }
    CHECK_EQ_INT(bad_spell, 0);
    CHECK_EQ_INT(bad_value, 0);
    CHECK_EQ_INT(turns > 400, 1); /* about 545 */
    CHECK_NEAR((double)wet / (double)steps, 0.2045, 0.035);
}

static void test_cold(void)
{
    uint32_t s;
    int x, z, both = 0;

    for (s = 1; s <= 4; s++) {
        long cold = 0, all = 0, other = 0;
        int changes = 0, was = weather_cold(s, -10000, 37);

        for (z = -8192; z < 8192; z += 32)
            for (x = -8192; x < 8192; x += 32) {
                int c = weather_cold(s, x, z);

                cold += c;
                all++;
                other += c != weather_cold(s + 100, x, z);
                if (c != weather_cold(s, x, z)) /* the same answer each time */
                    other = -1000000;
            }
        CHECK_NEAR((double)cold / (double)all, 0.14, 0.08); /* the snowy temperature band */
        CHECK_EQ_INT(other > 0, 1);
        /* Along a line the map crosses over at most once a cell: wide
         * patches, no speckles, across x = 0 too. */
        for (x = -9999; x < 10000; x++) {
            int c = weather_cold(s, x, 37);

            changes += c != was;
            was = c;
        }
        CHECK_EQ_INT(changes > 0 && changes <= 20000 / 128, 1);
        /* The warmth stays 0 .. 255 and eases from column to column (at
         * most 2 a block), both ways along x and z, either side of 0. */
        {
            int bad = 0, t, tx, tz, px = weather_temp(s, -10000, 37), pz = weather_temp(s, -37, -10000);

            for (x = -9999; x < 10000; x++) {
                t = weather_temp(s, x, 37);
                tx = t - px;
                tz = weather_temp(s, -37, x) - pz;
                bad += t < 0 || t > 255 || tx < -2 || tx > 2 || tz < -2 || tz > 2;
                bad += weather_cold(s, x, 37) != (t < WEATHER_COLD);
                px = t;
                pz += tz;
            }
            CHECK_EQ_INT(bad, 0);
        }
    }
    /* Snow to find not far from spawn, and rain. */
    for (s = 1; s <= 20; s++) {
        int c = 0, n = 0;

        for (z = -1024; z <= 1024; z += 16)
            for (x = -1024; x <= 1024; x += 16, n++)
                c += weather_cold(s, x, z);
        both += c > 0 && c < n;
    }
    CHECK_EQ_INT(both, 20);
}

static void test_rain_sky(void)
{
    SkyLook a, b;
    MeshVertex quads[SKY_QUADS_MAX];
    float sky[3];
    int i, spread_a, spread_b;

    sky_look(0.25f, &a); /* noon */
    b = a;
    weather_sky(&b, 0.0f);
    CHECK_EQ_INT(memcmp(&a, &b, sizeof a), 0);
    weather_sky(&b, -1.0f);
    CHECK_EQ_INT(memcmp(&a, &b, sizeof a), 0);
    weather_sky(&b, 1.0f);
    CHECK_NEAR(b.light, a.light * (1.0f - WEATHER_DIM), 1e-6);
    CHECK_EQ_INT(a.sun_shown && !b.sun_shown && !b.moon_shown, 1);
    CHECK_EQ_INT(b.clear & 255u, a.clear & 255u);
    spread_a = (int)((a.clear >> 8) & 255u) - (int)(a.clear >> 24); /* blue - red */
    spread_b = (int)((b.clear >> 8) & 255u) - (int)(b.clear >> 24);
    CHECK_EQ_INT(spread_a > 40 && spread_b * 3 < spread_a, 1);
    CHECK_EQ_INT((b.clear >> 16 & 255u) < (a.clear >> 16 & 255u), 1); /* darker */
    /* A quarter of the way in: the sun half faded, still there. */
    b = a;
    weather_sky(&b, 0.25f);
    CHECK_EQ_INT(b.sun_shown, 1);
    for (i = 0; i < 3; i++) {
        sky[i] = (float)((b.clear >> (24 - 8 * i)) & 255u) / 255.0f;
        CHECK_NEAR(b.sun_rgb[i], a.sun_rgb[i] + (sky[i] - a.sun_rgb[i]) * 0.5f, 0.003);
    }
    sky_look(0.75f, &a); /* midnight: the moon goes too */
    b = a;
    weather_sky(&b, 1.0f);
    CHECK_EQ_INT(a.moon_shown && !b.moon_shown, 1);
    /* The moon is white, and drawn in its own colour (rain fades it). */
    CHECK_EQ_INT(a.moon_rgb[0] == 1.0f && a.moon_rgb[1] == 1.0f && a.moon_rgb[2] == 1.0f, 1);
    b = a;
    b.moon_rgb[0] = 0.25f;
    b.moon_rgb[1] = 0.5f;
    b.moon_rgb[2] = 0.75f;
    i = sky_quads(&b, quads);
    CHECK_EQ_INT(i == MESH_FACE_VERTS && !b.sun_shown, 1);
    CHECK_EQ_INT(quads[0].r == 0.25f && quads[5].g == 0.5f && quads[3].b == 0.75f, 1);
}

void test_weather(void)
{
    test_spells();
    test_turns();
    test_long_run();
    test_cold();
    test_rain_sky();
}
