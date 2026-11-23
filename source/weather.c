#include <math.h>

#include "block.h"
#include "blocktex.h"
#include "chunk.h"
#include "weather.h"
#include "world_coords.h"

#define PI 3.14159265f

static const float k_rain_rgb[3] = { 0.7f, 0.8f, 1.0f }; /* pale blue */
static const float k_snow_rgb[3] = { 1.0f, 1.0f, 1.0f };

static uint32_t mix(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

/* The next number of the random sequence, 0 up to (not including) 1. */
static float rand01(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return (float)(*s >> 8) / 16777216.0f;
}

/* A new spell's length, for the weather it has just turned to. */
static float spell(Weather *w)
{
    float r = rand01(&w->rng);

    return w->raining ? WEATHER_RAIN_MIN + r * (WEATHER_RAIN_MAX - WEATHER_RAIN_MIN)
                      : WEATHER_CLEAR_MIN + r * (WEATHER_CLEAR_MAX - WEATHER_CLEAR_MIN);
}

void weather_init(Weather *w, uint32_t seed)
{
    w->rng = mix(seed ^ 0x57454154u); /* "WEAT" */
    w->raining = WEATHER_NEW_RAIN != 0;
    w->strength = w->raining ? 1.0f : 0.0f;
    w->clock = 0.0f;
    w->left = spell(w);
}

void weather_step(Weather *w, float dt)
{
    float fade;

    if (!(dt > 0.0f))
        return;
    w->clock = fmodf(w->clock + dt, WEATHER_CLOCK_WRAP);
    w->left -= dt;
    while (w->left <= 0.0f) {
        w->raining = !w->raining;
        w->left += spell(w);
    }
    fade = dt / WEATHER_FADE;
    if (w->raining)
        w->strength = w->strength + fade < 1.0f ? w->strength + fade : 1.0f;
    else
        w->strength = w->strength - fade > 0.0f ? w->strength - fade : 0.0f;
}

#ifndef WEATHER_TEST_COLD_X
static int floor_div(int a, int size)
{
    return a >= 0 ? a / size : -((size - 1 - a) / size);
}

/* The cold map's value 0 .. 255 at its point (i, j). */
static int64_t temp_point(uint32_t seed, int i, int j)
{
    uint32_t h = mix(seed ^ 0x434F4C44u); /* "COLD" */

    h = mix(h ^ (uint32_t)i);
    h = mix(h ^ (uint32_t)j);
    return (int64_t)(h >> 24);
}

/* t from 0 to WEATHER_TEMP_CELL, eased at both ends, same range. */
static int64_t smooth(int64_t t)
{
    const int64_t c = WEATHER_TEMP_CELL;

    return t * t * (3 * c - 2 * t) / (c * c);
}
#endif

int weather_temp(uint32_t seed, int x, int z)
{
#ifdef WEATHER_TEST_COLD_X
    (void)seed;
    (void)z;
    return x < WEATHER_TEST_COLD_X ? 0 : 255;
#else
    const int64_t c = WEATHER_TEMP_CELL;
    int i = floor_div(x, WEATHER_TEMP_CELL), j = floor_div(z, WEATHER_TEMP_CELL);
    int64_t sx = smooth(x - (int64_t)i * c), sz = smooth(z - (int64_t)j * c);
    int64_t near = temp_point(seed, i, j) * (c - sx) + temp_point(seed, i + 1, j) * sx;
    int64_t far = temp_point(seed, i, j + 1) * (c - sx) + temp_point(seed, i + 1, j + 1) * sx;

    return (int)((near * (c - sz) + far * sz) / (c * c));
#endif
}

bool weather_cold(uint32_t seed, int x, int z)
{
    return weather_temp(seed, x, z) < WEATHER_COLD;
}

static float clamp01(float v)
{
    return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}

void weather_sky(SkyLook *look, float strength)
{
    float s = clamp01(strength), rgb[3], grey, fade;
    int i;

    if (s <= 0.0f)
        return;
    for (i = 0; i < 3; i++)
        rgb[i] = (float)((look->clear >> (24 - 8 * i)) & 255u);
    grey = 0.6f * (0.3f * rgb[0] + 0.59f * rgb[1] + 0.11f * rgb[2]);
    for (i = 0; i < 3; i++)
        rgb[i] += (grey - rgb[i]) * WEATHER_GREY * s;
    look->clear = ((uint32_t)(rgb[0] + 0.5f) << 24) | ((uint32_t)(rgb[1] + 0.5f) << 16) |
                  ((uint32_t)(rgb[2] + 0.5f) << 8) | (look->clear & 255u);
    look->light *= 1.0f - WEATHER_DIM * s;
    /* The sun and moon fade into the grey sky, gone by half strength. */
    fade = clamp01(2.0f * s);
    for (i = 0; i < 3; i++) {
        look->sun_rgb[i] += (rgb[i] / 255.0f - look->sun_rgb[i]) * fade;
        look->moon_rgb[i] += (rgb[i] / 255.0f - look->moon_rgb[i]) * fade;
    }
    if (fade >= 1.0f)
        look->sun_shown = look->moon_shown = false;
}

/* The y just above the highest block of column (x, z): the rain stops
 * there. A column of a chunk that is not loaded counts as roofed. */
static float column_top(const World *world, int x, int z)
{
    int cx = block_to_chunk(x), cz = block_to_chunk(z), y;
    const Chunk *c = world_chunk(world, cx, cz);
    int lx = x - cx * CHUNK_SIZE, lz = z - cz * CHUNK_SIZE;

    if (c == NULL)
        return (float)WORLD_HEIGHT;
    for (y = WORLD_HEIGHT - 1; y >= 0; y--)
        if (c->blocks[y][lz][lx] != BLOCK_AIR)
            return (float)(y + 1);
    return 0.0f;
}

/* v mod m, from 0 up to m. */
static float wrap(float v, float m)
{
    float r = fmodf(v, m);

    return r < 0.0f ? r + m : r;
}

/* One drop's square: middle bottom at p, across along a, up along b. */
static void drop_quad(MeshVertex out[MESH_FACE_VERTS], const float p[3], const float a[3],
                      const float b[3], const float rgb[3], float u, float v)
{
    static const signed char k_corner[4][2] = { { -1, 0 }, { 1, 0 }, { 1, 2 }, { -1, 2 } };
    static const unsigned char k_order[MESH_FACE_VERTS] = { 0, 1, 2, 0, 2, 3 };
    int i;

    for (i = 0; i < MESH_FACE_VERTS; i++) {
        const signed char *c = k_corner[k_order[i]];
        MeshVertex *o = &out[i];

        o->x = p[0] + c[0] * a[0] + c[1] * b[0];
        o->y = p[1] + c[0] * a[1] + c[1] * b[1];
        o->z = p[2] + c[0] * a[2] + c[1] * b[2];
        o->u = u;
        o->v = v;
        o->r = rgb[0];
        o->g = rgb[1];
        o->b = rgb[2];
        o->t = 0.0f;
    }
}

int weather_build(const Weather *w, const World *world, const Camera *cam,
                  MeshVertex out[WEATHER_VERTS_MAX])
{
    const int r = WEATHER_RADIUS;
    int ex = (int)floorf(cam->x), ez = (int)floorf(cam->z), dx, dz, k, n = 0;
    float lo = cam->y - 0.5f * WEATHER_BAND, f[3], rt[3], up[3], uv[4], u, v;
    float rain_a[3], rain_b[3], snow_a[3], snow_b[3];
    uint32_t limit;

    if (!(w->strength > 0.0f))
        return 0;
    limit = (uint32_t)(clamp01(w->strength) * 256.0f);
    blocktex_tile_uv(TILE_CLOUD, uv);
    u = 0.5f * (uv[0] + uv[2]);
    v = 0.5f * (uv[1] + uv[3]);
    camera_forward(cam, f);
    rt[0] = cosf(cam->yaw);
    rt[1] = 0.0f;
    rt[2] = sinf(cam->yaw);
    up[0] = rt[1] * f[2] - rt[2] * f[1];
    up[1] = rt[2] * f[0] - rt[0] * f[2];
    up[2] = rt[0] * f[1] - rt[1] * f[0];
    for (k = 0; k < 3; k++) {
        rain_a[k] = rt[k] * 0.5f * WEATHER_RAIN_WIDTH; /* a streak stands upright */
        rain_b[k] = k == 1 ? 0.5f * WEATHER_RAIN_LEN : 0.0f;
        snow_a[k] = rt[k] * 0.5f * WEATHER_SNOW_SIZE; /* a flake faces the eye */
        snow_b[k] = up[k] * 0.5f * WEATHER_SNOW_SIZE;
    }
    for (dz = -r; dz <= r; dz++)
        for (dx = -r; dx <= r; dx++) {
            int x = ex + dx, z = ez + dz;
            uint32_t h = mix(mix(0x52414E44u ^ (uint32_t)x) ^ (uint32_t)z); /* "RAND" */
            float top;
            bool cold;

            if (dx * dx + dz * dz > r * r || (h & 255u) >= limit)
                continue;
            top = column_top(world, x, z);
            if (top >= lo + WEATHER_BAND)
                continue; /* roofed all the way up */
            cold = weather_cold(world->seed, x, z);
            for (k = 0; k < WEATHER_PER_COLUMN; k++) {
                uint32_t hk = mix(h + (uint32_t)(k + 1) * 0x9E3779B9u);
                /* A flake drifts, so it starts nearer its column's middle
                 * and stays inside the column. */
                float spread = cold ? 0.3f : 0.8f;
                float phase = (float)((hk >> 16) & 1023u) / 1024.0f;
                float p[3];

                p[0] = (float)x + 0.5f + spread * ((float)(hk & 255u) / 255.0f - 0.5f);
                p[2] = (float)z + 0.5f + spread * ((float)((hk >> 8) & 255u) / 255.0f - 0.5f);
                if (cold) {
                    float a = 2.0f * PI * (w->clock / WEATHER_SNOW_SWAY_TIME + phase);

                    p[1] = lo + wrap(phase * WEATHER_BAND - w->clock * WEATHER_SNOW_SPEED - lo,
                                     WEATHER_BAND);
                    if (p[1] - 0.5f * WEATHER_SNOW_SIZE < top)
                        continue;
                    p[0] += WEATHER_SNOW_SWAY * sinf(a);
                    p[2] += WEATHER_SNOW_SWAY * cosf(a);
                    p[0] -= snow_b[0]; /* drop_quad() takes the bottom middle */
                    p[1] -= snow_b[1];
                    p[2] -= snow_b[2];
                    drop_quad(out + n, p, snow_a, snow_b, k_snow_rgb, u, v);
                } else {
                    p[1] = lo + wrap(phase * WEATHER_BAND - w->clock * WEATHER_RAIN_SPEED - lo,
                                     WEATHER_BAND);
                    if (p[1] < top)
                        continue;
                    drop_quad(out + n, p, rain_a, rain_b, k_rain_rgb, u, v);
                }
                n += MESH_FACE_VERTS;
            }
        }
    return n;
}
