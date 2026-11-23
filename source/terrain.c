#include "terrain.h"

#include "block.h"

/* Noise values are fractions written as whole numbers out of ONE. */
#define ONE 65536

/* Steepest slope: a smooth step climbs at most 1.5 times its average, so
 * 1.5 * 8 / 32 + 1.5 * 4 / 16 = 0.75 blocks per block. Under one, so the
 * whole-block heights of next-door columns differ by at most one. */
#define BIG_SIZE 32
#define BIG_HEIGHT 8
#define SMALL_SIZE 16
#define SMALL_HEIGHT 4

/* Rounds down for negative a too (C division rounds toward zero). */
static int floor_div(int a, int size)
{
    return a >= 0 ? a / size : -((size - 1 - a) / size);
}

static uint32_t mix(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

/* A fixed random value 0..ONE-1 for each grid point of each layer. */
static int64_t grid_value(uint32_t seed, uint32_t layer, int ix, int iz)
{
    uint32_t h = mix(seed ^ (layer * 0x9E3779B9u));
    h = mix(h ^ (uint32_t)ix);
    h = mix(h ^ (uint32_t)iz);
    return (int64_t)(h & (ONE - 1));
}

/* 3t^2 - 2t^3: starts and ends flat, so the hills have no creases. */
static int64_t smooth(int64_t t)
{
    return t * t / ONE * (3 * ONE - 2 * t) / ONE;
}

static int64_t lerp(int64_t a, int64_t b, int64_t t)
{
    return a + (b - a) * t / ONE;
}

/* One layer at (x, z): the four grid values around it, blended smoothly.
 * 0..ONE-1. */
static int64_t layer_value(uint32_t seed, uint32_t layer, int x, int z, int size)
{
    int ix = floor_div(x, size);
    int iz = floor_div(z, size);
    int64_t tx = smooth((int64_t)(x - ix * size) * ONE / size);
    int64_t tz = smooth((int64_t)(z - iz * size) * ONE / size);
    int64_t north = lerp(grid_value(seed, layer, ix, iz), grid_value(seed, layer, ix + 1, iz), tx);
    int64_t south = lerp(grid_value(seed, layer, ix, iz + 1), grid_value(seed, layer, ix + 1, iz + 1), tx);

    return lerp(north, south, tz);
}

int terrain_height(uint32_t seed, int x, int z)
{
    int64_t n = layer_value(seed, 1, x, z, BIG_SIZE) * BIG_HEIGHT +
                layer_value(seed, 2, x, z, SMALL_SIZE) * SMALL_HEIGHT;

    return TERRAIN_BASE + (int)(n / ONE);
}

void terrain_fill_chunk(Chunk *c, uint32_t seed, int cx, int cz)
{
    int x, y, z;

    chunk_clear(c);
    for (z = 0; z < CHUNK_SIZE; z++)
        for (x = 0; x < CHUNK_SIZE; x++) {
            int h = terrain_height(seed, cx * CHUNK_SIZE + x, cz * CHUNK_SIZE + z);

            for (y = 0; y <= h; y++)
                chunk_set(c, x, y, z,
                          y == h ? BLOCK_GRASS : y >= h - TERRAIN_DIRT ? BLOCK_DIRT : BLOCK_STONE);
        }
}
