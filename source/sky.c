#include <math.h>

#include "blocktex.h"
#include "sky.h"
#include "stream.h"
#include "world_coords.h"

#define TWO_PI 6.28318531f

static float clamp01(float v)
{
    return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}

/* 0 up to lo, 1 from hi, an S-shaped ramp between. */
static float smooth(float lo, float hi, float v)
{
    float s = clamp01((v - lo) / (hi - lo));

    return s * s * (3.0f - 2.0f * s);
}

float sky_advance(float t, float seconds)
{
    float n = t + seconds / SKY_DAY_SECONDS;

    n -= floorf(n);
    if (!(n >= 0.0f) || n >= 1.0f) /* not a number; or -tiny + 1 rounded up to 1 */
        n = 0.0f;
    return n;
}

static float channel(uint32_t rgb, int shift)
{
    return (float)((rgb >> shift) & 0xFF);
}

void sky_look(float t, SkyLook *out)
{
    float a = TWO_PI * t, h = sinf(a);
    float day = smooth(-0.25f, 0.3f, h);             /* night blue .. day blue */
    float glow = 0.6f * clamp01(1.0f - fabsf(h) / 0.25f); /* orange, most at the horizon */
    float k = clamp01((0.3f - h) / 0.3f);            /* the sun's orange */
    uint32_t clear = 0xFF;
    int shift;

    out->sun[0] = cosf(a);
    out->sun[1] = h;
    out->sun[2] = 0.0f;
    out->light = SKY_NIGHT_LIGHT + (1.0f - SKY_NIGHT_LIGHT) * smooth(-0.2f, 0.2f, h);
    for (shift = 16; shift >= 0; shift -= 8) {
        float v = channel(SKY_NIGHT_RGB, shift);

        v += day * (channel(SKY_DAY_RGB, shift) - v);
        v += glow * (channel(SKY_GLOW_RGB, shift) - v);
        clear |= (uint32_t)(v + 0.5f) << (shift + 8);
    }
    out->clear = clear;
    out->sun_rgb[0] = 1.0f;
    out->sun_rgb[1] = 1.0f - 0.3f * k;
    out->sun_rgb[2] = 1.0f - 0.55f * k;
    /* Shown while the square's top edge is above the horizon. */
    out->sun_shown = h > -SKY_SUN_HALF / SKY_DIST;
    out->moon_shown = -h > -SKY_MOON_HALF / SKY_DIST;
}

/* A square `half` from its middle to each edge, SKY_DIST along unit vector
 * d (which lies in the x-y plane), facing the origin. */
static void quad(MeshVertex out[MESH_FACE_VERTS], const float d[3], float half, int tile,
                 const float rgb[3])
{
    static const int k_order[MESH_FACE_VERTS] = { 0, 1, 2, 0, 2, 3 };
    const float u[3] = { 0.0f, 0.0f, 1.0f };   /* across the path: north-south */
    const float v[3] = { -d[1], d[0], 0.0f }; /* along the path */
    const float su[4] = { -1.0f, 1.0f, 1.0f, -1.0f }, sv[4] = { -1.0f, -1.0f, 1.0f, 1.0f };
    float uv[4];
    int i, k;

    blocktex_tile_uv(tile, uv);
    for (i = 0; i < MESH_FACE_VERTS; i++) {
        MeshVertex *o = &out[i];
        float p[3];

        k = k_order[i];
        p[0] = d[0] * SKY_DIST + half * (su[k] * u[0] + sv[k] * v[0]);
        p[1] = d[1] * SKY_DIST + half * (su[k] * u[1] + sv[k] * v[1]);
        p[2] = d[2] * SKY_DIST + half * (su[k] * u[2] + sv[k] * v[2]);
        o->x = p[0];
        o->y = p[1];
        o->z = p[2];
        o->u = su[k] < 0.0f ? uv[0] : uv[2];
        o->v = sv[k] < 0.0f ? uv[1] : uv[3];
        o->r = rgb[0];
        o->g = rgb[1];
        o->b = rgb[2];
        o->t = 0.0f; /* no torch light (row 3-03) */
    }
}

int sky_quads(const SkyLook *look, MeshVertex out[SKY_QUADS_MAX])
{
    static const float white[3] = { 1.0f, 1.0f, 1.0f };
    const float moon[3] = { -look->sun[0], -look->sun[1], -look->sun[2] };
    int n = 0;

    if (look->sun_shown) {
        quad(out + n, look->sun, SKY_SUN_HALF, TILE_SUN, look->sun_rgb);
        n += MESH_FACE_VERTS;
    }
    if (look->moon_shown) {
        quad(out + n, moon, SKY_MOON_HALF, TILE_MOON, white);
        n += MESH_FACE_VERTS;
    }
    return n;
}

void sky_fog(int view, float *start, float *end)
{
    /* Rings up to view are drawn round the player's chunk, so the edge is
     * at least view whole chunks away from anywhere in it. */
    *end = (float)(stream_view(view) * CHUNK_SIZE);
    *start = SKY_FOG_START * *end;
}
