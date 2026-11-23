#include <math.h>
#include <string.h>

#include "block.h"
#include "blocktex.h"
#include "test.h"
#include "weather.h"

/* Weather (row 3-06): the rain and snow drops built round the eye. */

static World s_w;
static MeshVertex s_a[WEATHER_VERTS_MAX], s_b[WEATHER_VERTS_MAX];

static int in_range(float v, float lo, float hi)
{
    return v >= lo && v <= hi;
}

/* A seed whose climate is all cold (or all warm), and never dry, round column (ex, ez). */
static uint32_t seed_where(bool cold, int ex, int ez)
{
    uint32_t s;
    int dx, dz, ok;

    for (s = 1; s < 5000; s++) {
        ok = 1;
        for (dz = -WEATHER_RADIUS; dz <= WEATHER_RADIUS && ok; dz++)
            for (dx = -WEATHER_RADIUS; dx <= WEATHER_RADIUS && ok; dx++)
                ok = weather_cold(s, ex + dx, ez + dz) == cold && !weather_dry(s, ex + dx, ez + dz);
        if (ok)
            return s;
    }
    return 0;
}

static int circle_columns(void)
{
    int dx, dz, n = 0;

    for (dz = -WEATHER_RADIUS; dz <= WEATHER_RADIUS; dz++)
        for (dx = -WEATHER_RADIUS; dx <= WEATHER_RADIUS; dx++)
            n += dx * dx + dz * dz <= WEATHER_RADIUS * WEATHER_RADIUS;
    return n;
}

/* Quad q's middle (its four corners' average) and lowest y. */
static void quad_mid(const MeshVertex *v, int q, float m[3], float *bottom)
{
    static const int corner[4] = { 0, 1, 2, 5 };
    int i;

    m[0] = m[1] = m[2] = 0.0f;
    *bottom = 1e9f;
    for (i = 0; i < 4; i++) {
        const MeshVertex *c = &v[q * MESH_FACE_VERTS + corner[i]];
        m[0] += 0.25f * c->x;
        m[1] += 0.25f * c->y;
        m[2] += 0.25f * c->z;
        if (c->y < *bottom)
            *bottom = c->y;
    }
}

static float dist3(const MeshVertex *a, const MeshVertex *b)
{
    return sqrtf((a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y) +
                 (a->z - b->z) * (a->z - b->z));
}

static void eye(Camera *c, float x, float y, float z, float yaw, float pitch)
{
    c->x = x;
    c->y = y;
    c->z = z;
    c->yaw = yaw;
    c->pitch = pitch;
}

/* Every quad's shape, colour and place, and that it faces the eye. */
static void check_quads(const MeshVertex *v, int n, const Camera *cam, bool snow)
{
    float uv[4], f[3], m[3], bottom, lo = cam->y - 0.5f * WEATHER_BAND;
    int q, bad = 0, ex = (int)floorf(cam->x), ez = (int)floorf(cam->z);

    blocktex_tile_uv(TILE_CLOUD, uv);
    camera_forward(cam, f);
    for (q = 0; q < n / MESH_FACE_VERTS; q++) {
        const MeshVertex *c = &v[q * MESH_FACE_VERTS];
        int dx, dz, i;
        float e1[3], e2[3], nm[3], len;

        quad_mid(v, q, m, &bottom);
        dx = (int)floorf(m[0]) - ex;
        dz = (int)floorf(m[2]) - ez;
        bad += dx * dx + dz * dz > WEATHER_RADIUS * WEATHER_RADIUS;
        bad += !in_range(bottom, lo - 0.001f, lo + WEATHER_BAND);
        for (i = 0; i < MESH_FACE_VERTS; i++)
            bad += c[i].t != 0.0f || fabsf(c[i].u - 0.5f * (uv[0] + uv[2])) > 1e-6f ||
                   fabsf(c[i].v - 0.5f * (uv[1] + uv[3])) > 1e-6f ||
                   c[i].r != (snow ? 1.0f : 0.7f) || c[i].g != (snow ? 1.0f : 0.8f) ||
                   c[i].b != 1.0f;
        bad += fabsf(dist3(&c[0], &c[1]) - (snow ? WEATHER_SNOW_SIZE : WEATHER_RAIN_WIDTH)) > 1e-4f;
        bad += fabsf(dist3(&c[1], &c[2]) - (snow ? WEATHER_SNOW_SIZE : WEATHER_RAIN_LEN)) > 1e-4f;
        if (!snow)
            bad += fabsf(c[2].y - c[1].y - WEATHER_RAIN_LEN) > 1e-4f; /* upright */
        e1[0] = c[1].x - c[0].x, e1[1] = c[1].y - c[0].y, e1[2] = c[1].z - c[0].z;
        e2[0] = c[2].x - c[0].x, e2[1] = c[2].y - c[0].y, e2[2] = c[2].z - c[0].z;
        nm[0] = e1[1] * e2[2] - e1[2] * e2[1];
        nm[1] = e1[2] * e2[0] - e1[0] * e2[2];
        nm[2] = e1[0] * e2[1] - e1[1] * e2[0];
        len = sqrtf(nm[0] * nm[0] + nm[1] * nm[1] + nm[2] * nm[2]);
        /* Counter-clockwise from the eye: the normal points back at it.
         * A flake faces the eye square on. */
        bad += !((nm[0] * f[0] + nm[1] * f[1] + nm[2] * f[2]) < 0.0f);
        if (snow)
            bad += fabsf((nm[0] * f[0] + nm[1] * f[1] + nm[2] * f[2]) / len + 1.0f) > 1e-3f;
    }
    CHECK_EQ_INT(bad, 0);
}

static void test_build(void)
{
    Weather w;
    Camera cam;
    int n, cols = circle_columns();
    uint32_t warm = seed_where(false, 0, 0), cold = seed_where(true, 0, 0);

    CHECK_EQ_INT(warm != 0 && cold != 0, 1);
    test_world_cleared(&s_w);
    s_w.seed = warm;
    weather_init(&w, 1u);
    w.strength = 1.0f;
    w.clock = 1.0f;
    eye(&cam, 0.5f, 70.0f, 0.5f, 0.0f, 0.0f);
    n = weather_build(&w, &s_w, &cam, s_a);
    CHECK_EQ_INT(n, cols * WEATHER_PER_COLUMN * MESH_FACE_VERTS); /* every column */
    check_quads(s_a, n, &cam, false);
    eye(&cam, 0.5f, 70.0f, 0.5f, 2.0f, 0.9f); /* turned and looking up */
    check_quads(s_a, weather_build(&w, &s_w, &cam, s_a), &cam, false);
    s_w.seed = cold;
    eye(&cam, 0.5f, 70.0f, 0.5f, 4.0f, -0.6f);
    n = weather_build(&w, &s_w, &cam, s_a);
    CHECK_EQ_INT(n, cols * WEATHER_PER_COLUMN * MESH_FACE_VERTS);
    check_quads(s_a, n, &cam, true);
    /* Half strength: about half the columns; none when clear. */
    w.strength = 0.5f;
    n = weather_build(&w, &s_w, &cam, s_a) / (WEATHER_PER_COLUMN * MESH_FACE_VERTS);
    CHECK_EQ_INT(n > cols * 35 / 100 && n < cols * 65 / 100, 1);
    w.strength = 0.0f;
    CHECK_EQ_INT(weather_build(&w, &s_w, &cam, s_a), 0);
}

/* The drops fall at their speeds, and the clock coming round moves none. */
static void test_motion(void)
{
    Weather w;
    Camera cam;
    int n, i, k, bad;
    const float speed[2] = { WEATHER_RAIN_SPEED, WEATHER_SNOW_SPEED };

    CHECK_NEAR(fmodf(WEATHER_CLOCK_WRAP * WEATHER_RAIN_SPEED, WEATHER_BAND), 0.0, 0.0);
    CHECK_NEAR(fmodf(WEATHER_CLOCK_WRAP * WEATHER_SNOW_SPEED, WEATHER_BAND), 0.0, 0.0);
    CHECK_NEAR(fmodf(WEATHER_CLOCK_WRAP, WEATHER_SNOW_SWAY_TIME), 0.0, 0.0);
    test_world_cleared(&s_w);
    weather_init(&w, 1u);
    w.strength = 1.0f;
    eye(&cam, 0.5f, 70.0f, 0.5f, 0.3f, 0.0f);
    for (k = 0; k < 2; k++) {
        s_w.seed = seed_where(k == 1, 0, 0);
        w.clock = 1.0f;
        n = weather_build(&w, &s_w, &cam, s_a);
        w.clock = 1.01f;
        CHECK_EQ_INT(weather_build(&w, &s_w, &cam, s_b), n);
        for (i = bad = 0; i < n; i++) {
            float dy = s_b[i].y - s_a[i].y, fall = 0.01f * speed[k];

            bad += fabsf(dy + fall) > 1e-3f && fabsf(dy + fall - WEATHER_BAND) > 1e-3f;
            bad += fabsf(s_b[i].x - s_a[i].x) > (k ? 0.01f : 0.0f) ||
                   fabsf(s_b[i].z - s_a[i].z) > (k ? 0.01f : 0.0f);
        }
        CHECK_EQ_INT(bad, 0);
        w.clock = WEATHER_CLOCK_WRAP - 0.005f;
        n = weather_build(&w, &s_w, &cam, s_a);
        w.clock = -0.005f; /* the same moment, the clock come round */
        CHECK_EQ_INT(weather_build(&w, &s_w, &cam, s_b), n);
        for (i = bad = 0; i < n; i++)
            bad += dist3(&s_a[i], &s_b[i]) > 0.02f;
        CHECK_EQ_INT(bad, 0);
    }
}

/* A drop belongs to its column of the world: moving the eye leaves it. */
static void test_world_fixed(void)
{
    Weather w;
    Camera a, c;
    int n, m, q, r, matched, wanted, bad;
    float ma[3], mc[3], ba, bc;

    test_world_cleared(&s_w);
    s_w.seed = seed_where(false, 0, 0);
    weather_init(&w, 1u);
    w.strength = 1.0f;
    w.clock = 2.0f;
    eye(&a, 0.5f, 70.0f, 0.5f, 0.0f, 0.0f);
    n = weather_build(&w, &s_w, &a, s_a);
    eye(&c, 0.9f, 70.0f, 0.2f, 0.0f, 0.0f); /* the same block */
    CHECK_EQ_INT(weather_build(&w, &s_w, &c, s_b), n);
    CHECK_EQ_INT(memcmp(s_a, s_b, n * sizeof *s_a), 0);
    eye(&c, 1.5f, 70.3f, 0.5f, 0.0f, 0.0f); /* a block east, a little up */
    m = weather_build(&w, &s_w, &c, s_b);
    matched = wanted = 0;
    for (q = 0; q < n / MESH_FACE_VERTS; q++) {
        int dx, dz;

        quad_mid(s_a, q, ma, &ba);
        dx = (int)floorf(ma[0]) - 1;
        dz = (int)floorf(ma[2]);
        if (dx * dx + dz * dz > WEATHER_RADIUS * WEATHER_RADIUS || ba < c.y - 8.0f + 0.01f)
            continue;
        wanted++;
        for (r = 0; r < m / MESH_FACE_VERTS; r++) {
            quad_mid(s_b, r, mc, &bc);
            if (fabsf(ma[0] - mc[0]) + fabsf(ma[1] - mc[1]) + fabsf(ma[2] - mc[2]) < 1e-3f) {
                matched++;
                break;
            }
        }
    }
    CHECK_EQ_INT(wanted > 200 && matched == wanted, 1);
    /* Turning turns each streak, not where it is. */
    eye(&c, 0.5f, 70.0f, 0.5f, 1.0f, 0.4f);
    CHECK_EQ_INT(weather_build(&w, &s_w, &c, s_b), n);
    for (q = bad = 0; q < n / MESH_FACE_VERTS; q++) {
        quad_mid(s_a, q, ma, &ba);
        quad_mid(s_b, q, mc, &bc);
        bad += fabsf(ma[0] - mc[0]) + fabsf(ma[1] - mc[1]) + fabsf(ma[2] - mc[2]) > 1e-4f;
    }
    CHECK_EQ_INT(bad, 0);
}

static int quads_in(const MeshVertex *v, int n, int x, int z, float *lowest)
{
    float m[3], bottom;
    int q, k = 0;

    *lowest = 1e9f;
    for (q = 0; q < n / MESH_FACE_VERTS; q++) {
        quad_mid(v, q, m, &bottom);
        if ((int)floorf(m[0]) == x && (int)floorf(m[2]) == z) {
            k++;
            *lowest = bottom < *lowest ? bottom : *lowest;
        }
    }
    return k;
}

/* Never under a roof, never in the ground, never in a chunk not loaded. */
static void test_roof(void)
{
    Weather w;
    Camera cam;
    int k, n, q, x, z, y, bad;
    float lowest, m[3], bottom;

    weather_init(&w, 1u);
    w.strength = 1.0f;
    for (k = 0; k < 2; k++) {
        test_world_cleared(&s_w);
        s_w.seed = seed_where(k == 1, 0, 0);
        world_set(&s_w, 2, 74, 0, BLOCK_STONE);   /* a roof low over the eye */
        world_set(&s_w, -3, 90, 1, BLOCK_STONE);  /* one higher than the drops reach */
        eye(&cam, 0.5f, 70.0f, 0.5f, 0.0f, 0.0f);
        for (w.clock = 0.0f, bad = 0; w.clock < 8.0f; w.clock += 0.25f) {
            n = weather_build(&w, &s_w, &cam, s_a);
            bad += quads_in(s_a, n, 2, 0, &lowest) > 0 && lowest < 75.0f;
            bad += quads_in(s_a, n, -3, 1, &lowest) != 0;
        }
        CHECK_EQ_INT(bad, 0);
    }
    eye(&cam, -30.5f, 70.0f, 0.5f, 0.0f, 0.0f); /* at the small world's edge */
    n = weather_build(&w, &s_w, &cam, s_a);
    for (q = bad = 0; q < n / MESH_FACE_VERTS; q++) {
        quad_mid(s_a, q, m, &bottom);
        bad += m[0] < -32.0f;
    }
    CHECK_EQ_INT(bad == 0 && n > 0, 1);
    /* The seed's hills: every drop above its column's ground. */
    test_world_small(&s_w, 5u);
    eye(&cam, 0.5f, 72.0f, 0.5f, 0.0f, 0.0f);
    for (w.clock = 0.0f, bad = 0; w.clock < 4.0f; w.clock += 0.5f) {
        n = weather_build(&w, &s_w, &cam, s_a);
        for (q = 0; q < n / MESH_FACE_VERTS; q++) {
            quad_mid(s_a, q, m, &bottom);
            x = (int)floorf(m[0]);
            z = (int)floorf(m[2]);
            for (y = WORLD_HEIGHT - 1; y > 0 && world_get(&s_w, x, y, z) == BLOCK_AIR; y--)
                ;
            bad += bottom < (float)(y + 1) - 0.001f;
        }
    }
    CHECK_EQ_INT(bad == 0 && n > 0, 1);
}

void test_weather_drops(void)
{
    test_build();
    test_motion();
    test_world_fixed();
    test_roof();
    test_weather_climate();
}
