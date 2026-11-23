#include "terrain_noise.h"

#define ONE NOISE_ONE

/* Added to u and v so that negative positions round down too: a multiple
 * of every grid spacing, and bigger than any position a layer reads
 * (40 * a million), so u and v never go below 0. */
#define BIAS 0x40000000u

/* 16 slopes, one every 22.5 degrees, each one whole ONE long. */
static const int k_slope[16][2] = {
    { 65536, 0 }, { 60547, 25080 }, { 46341, 46341 }, { 25080, 60547 },
    { 0, 65536 }, { -25080, 60547 }, { -46341, 46341 }, { -60547, 25080 },
    { -65536, 0 }, { -60547, -25080 }, { -46341, -46341 }, { -25080, -60547 },
    { 0, -65536 }, { 25080, -60547 }, { 46341, -46341 }, { 60547, -25080 },
};

static uint32_t mix(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

/* 3t^2 - 2t^3: starts and ends flat, so the ground has no creases. */
static int64_t smooth(int64_t t)
{
    return t * t / ONE * (3 * ONE - 2 * t) / ONE;
}

static int64_t lerp(int64_t a, int64_t b, int64_t t)
{
    return a + (b - a) * t / ONE;
}

int noise_smooth(int t)
{
    return (int)smooth(t);
}

/* Where (x, z) falls in layer l: its grid cell, how far across it
 * (0..ONE-1 each way), and a random number for each corner, west-north,
 * east-north, west-south, east-south. */
static void cell_of(uint32_t seed, const NoiseLayer *l, int x, int z,
                    int64_t *fu, int64_t *fv, uint32_t corner[4])
{
    uint32_t h = mix(seed ^ (l->id * 0x9E3779B9u));
    /* Each layer's grid starts somewhere different, so no place (like the
     * origin) is on a grid point of every layer. */
    uint32_t u = (uint32_t)(l->a * x - l->b * z) + BIAS + (h & 0xFFFFFu);
    uint32_t v = (uint32_t)(l->b * x + l->a * z) + BIAS + ((h >> 12) & 0xFFFFFu);
    uint32_t part = (1u << l->shift) - 1u;
    uint32_t iu = u >> l->shift, iv = v >> l->shift;
    uint32_t h0 = mix(h ^ iu), h1 = mix(h ^ (iu + 1u));

    *fu = (int64_t)((u & part) << (16 - l->shift));
    *fv = (int64_t)((v & part) << (16 - l->shift));
    corner[0] = mix(h0 ^ iv);
    corner[1] = mix(h1 ^ iv);
    corner[2] = mix(h0 ^ (iv + 1u));
    corner[3] = mix(h1 ^ (iv + 1u));
}

int noise_value(uint32_t seed, const NoiseLayer *l, int x, int z)
{
    int64_t fu, fv, su, sv;
    uint32_t c[4];

    cell_of(seed, l, x, z, &fu, &fv, c);
    su = smooth(fu);
    sv = smooth(fv);
    return (int)lerp(lerp(c[0] & (ONE - 1), c[1] & (ONE - 1), su),
                     lerp(c[2] & (ONE - 1), c[3] & (ONE - 1), su), sv);
}

/* Corner c's slope times the way from that corner to (du, dv). */
static int64_t slope_at(uint32_t c, int64_t du, int64_t dv)
{
    const int *g = k_slope[c & 15];

    return (g[0] * du + g[1] * dv) / ONE;
}

int noise_slope(uint32_t seed, const NoiseLayer *l, int x, int z)
{
    int64_t fu, fv, su, sv;
    uint32_t c[4];

    cell_of(seed, l, x, z, &fu, &fv, c);
    su = smooth(fu);
    sv = smooth(fv);
    return (int)lerp(lerp(slope_at(c[0], fu, fv), slope_at(c[1], fu - ONE, fv), su),
                     lerp(slope_at(c[2], fu, fv - ONE), slope_at(c[3], fu - ONE, fv - ONE), su), sv);
}
