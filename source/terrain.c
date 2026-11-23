#include "terrain.h"

#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "terrain_noise.h"

/* Noise values are fractions written as whole numbers out of ONE. Heights
 * are in blocks times ONE, measured from TERRAIN_SEA + 1. */
#define ONE NOISE_ONE
#define PCT(n) ((n) * ONE / 100)
#define BLK(n) ((n) * ONE)

/* The slow layers are worked out every CELL blocks and blended in between,
 * which is much quicker and still smooth: a straight blend between points
 * is never steeper than the ground it was taken from. */
#define CELL 4

/* How each column is made, from the inputs below:
 *
 * - The coast: `cont` (continents, slope noise about 800 and 300 blocks
 *   across) below COAST is sea. s = cont - COAST: how far inland. The sea
 *   floor drops K_SEA blocks per whole s until it meets the deep floor
 *   (TERRAIN_FLOOR, rolling up to SEA_SWELL + SEA_HILL blocks higher); the
 *   land rises K_LAND per whole s until it meets the plains. Low land (up to
 *   BEACH_UP above the sea) within about BEACH_W blocks of the coast is beach.
 * - Plains: PLAINS_UP blocks above the sea, plus a slow swell of up to 2,
 *   plus bumps of up to 1.5 blocks every 16.
 * - Hills: `eros` (about 280 blocks) sets how hilly; above EROS0 the hills
 *   grow K_EROS blocks per whole eros, up to HILL_UP high, shaped by `hill`
 *   (bumps about 85 and 40 blocks apart). HILLS where they may reach
 *   HILLS_KIND blocks.
 * - Rivers: where `river` (slope noise, about 270 and 110 blocks) is near
 *   0, the ground is cut down to a sand bed under the water, rising K_RIVER
 *   blocks per whole step away from 0.
 *
 * Walkable: next-door columns differ by at most the sum of how steep each
 * part can get (terrain_noise.h). Coast 0.7 * 0.00222 + 0.3 * 0.00583 =
 * 0.0033 per block, times K_SEA or K_LAND (260): 0.86 blocks. Deep sea
 * floor SEA_SWELL * 0.0161 + SEA_HILL * 0.0302 = 0.34. Plains and hills
 * K_EROS * 0.0066 + HILL_UP * (0.7 * 0.0205 + 0.3 * 0.0527) = 0.69, swell
 * 2 * 0.016 = 0.03, bumps 1.5 * 1.5 / 16 = 0.14: 0.86 in all. Rivers
 * K_RIVER * (5 * 0.00662 + 0.01603) / 6 = 0.93. The highest (max) or
 * lowest (min) of them climbs no faster than the steepest, so all under one
 * block per block: whole-block heights of next-door columns differ by at
 * most one.
 *
 * - Mountains: only inland, where `mount` (about 360 and 130 blocks) is
 *   above MOUNT0. They rise by up to MOUNT_BASE + MOUNT_PEAK blocks, shaped
 *   by sharp ridges (about 52 and 27 blocks apart) and bumps (22). Where
 *   `crag` is high, and away from their feet, they are cut into ledges
 *   TERRACE blocks tall: each one rises gently for most of its width, then
 *   steeply, which leaves cliffs (bare stone). Rivers stop at the
 *   mountains' feet. Nothing above keeps mountains walkable, and nothing
 *   else is changed where there are none. */
#define COAST (-PCT(3))
#define K_SEA 260
#define K_LAND 260
#define SEA_SWELL 6 /* the deep sea floor rolls up to this... */
#define SEA_HILL 8  /* ...plus this many blocks above TERRAIN_FLOOR */
#define BEACH_UP 3
#define BEACH_W 4 /* beaches reach about this many blocks inland */
#define PLAINS_UP 2
#define EROS0 PCT(40)
#define K_EROS 40
#define HILL_UP 14
#define HILLS_KIND 5
#define RIVER_BED (-BLK(2) - ONE / 2) /* top of the bed at TERRAIN_SEA - 2 */
#define RIVER_EDGE (ONE * 2 / 1000)   /* half-width of the flat bed */
#define K_RIVER 113
#define K_RIVER_MOUNT 80 /* rivers rise this many blocks per whole mountain */
#define MOUNT0 PCT(62)
#define MOUNT_RANGE PCT(12) /* from no mountain to a whole one */
#define MOUNT_S PCT(5)      /* mountains start this far inland */
#define MOUNT_BASE 10
#define MOUNT_PEAK 50
#define TERRACE BLK(7)
#define LEDGE (TERRACE * 3 / 4)      /* the gentle part of each ledge... */
#define LEDGE_RISE (TERRACE / 5)     /* ...climbs this much, the cliff the rest */
#define CLIFF_A0 PCT(30)             /* ledges start this far into the mountains */
#define CLIFF_RANGE PCT(30)          /* and are whole this much further */
#define CRAG0 PCT(45)                /* ledges only where `crag` is above this */
#define CRAG_RANGE PCT(15)
#define SOFT_TOP BLK(105 - TERRAIN_SEA - 1) /* above this, peaks grow half as fast */
#define STONE_TOP 100                /* bare stone from here up */

enum { IN_CONT, IN_EROS, IN_HILL, IN_SWELL, IN_RIVER, IN_MOUNT, IN_PHASE, IN_CRAG, IN_BLEND,
       IN_COAST_SLOPE = IN_BLEND, IN_COUNT };

/* The slow layers: { id, a, b, shift } (terrain_noise.h). */
static const NoiseLayer k_cont1 = { 11, 1, 5, 12 };  /* 803 blocks, slope noise */
static const NoiseLayer k_cont2 = { 12, 3, 6, 11 };  /* 305, slope noise */
static const NoiseLayer k_eros = { 13, 2, 7, 11 };   /* 281 */
static const NoiseLayer k_hill1 = { 14, 1, 6, 9 };   /* 84 */
static const NoiseLayer k_hill2 = { 15, 4, 5, 8 };   /* 40 */
static const NoiseLayer k_swell = { 16, 5, 6, 10 };  /* 131 */
static const NoiseLayer k_river1 = { 17, 3, 7, 11 }; /* 269, slope noise */
static const NoiseLayer k_river2 = { 18, 9, 2, 10 }; /* 111, slope noise: the bends */
static const NoiseLayer k_mount1 = { 19, 1, 1, 9 };  /* 362 */
static const NoiseLayer k_mount2 = { 20, 6, 5, 10 }; /* 131 */
static const NoiseLayer k_phase = { 21, 5, 1, 8 };   /* 50: where the ledges fall */
static const NoiseLayer k_crag = { 26, 2, 5, 9 };    /* 95: where there are ledges */
/* Worked out for every column. */
static const NoiseLayer k_bumps = { 22, 1, 0, 4 };   /* 16 */
static const NoiseLayer k_ridge1 = { 23, 4, 9, 9 };  /* 52 */
static const NoiseLayer k_ridge2 = { 25, 7, 6, 8 };  /* 27 */
static const NoiseLayer k_peak = { 24, 3, 5, 7 };    /* 22 */

static int min_int(int a, int b)
{
    return a < b ? a : b;
}

static int clamp_int(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

/* Rounds down for negative a too (C division rounds toward zero). */
static int floor_div(int a, int size)
{
    return a >= 0 ? a / size : -((size - 1 - a) / size);
}

/* The slow inputs at one grid point. Continents and rivers -0.71..0.71 of
 * ONE, the rest 0..ONE-1. */
static void slow_inputs(uint32_t seed, int x, int z, int in[IN_COUNT])
{
    in[IN_CONT] = (7 * noise_slope(seed, &k_cont1, x, z) + 3 * noise_slope(seed, &k_cont2, x, z)) / 10;
    in[IN_EROS] = noise_value(seed, &k_eros, x, z);
    in[IN_HILL] = (7 * noise_value(seed, &k_hill1, x, z) + 3 * noise_value(seed, &k_hill2, x, z)) / 10;
    in[IN_SWELL] = noise_value(seed, &k_swell, x, z);
    in[IN_RIVER] = (5 * noise_slope(seed, &k_river1, x, z) + noise_slope(seed, &k_river2, x, z)) / 6;
    in[IN_MOUNT] = (13 * noise_value(seed, &k_mount1, x, z) + 7 * noise_value(seed, &k_mount2, x, z)) / 20;
    in[IN_PHASE] = noise_value(seed, &k_phase, x, z);
    in[IN_CRAG] = noise_value(seed, &k_crag, x, z);
}

/* The inputs at (fx, fz) blocks into the cell with grid points g00 (west,
 * north), g10 (east), g01 (south) and g11. Also how fast `cont` changes
 * there, per block (the bigger of the two ways plus half the smaller: a
 * little over the true rate), so beaches can be measured in blocks. */
static void blend(const int *g00, const int *g10, const int *g01, const int *g11,
                  int fx, int fz, int in[IN_COUNT])
{
    int i, dx, dz;

    for (i = 0; i < IN_BLEND; i++)
        in[i] = (g00[i] * (CELL - fx) * (CELL - fz) + g10[i] * fx * (CELL - fz) +
                 g01[i] * (CELL - fx) * fz + g11[i] * fx * fz) / (CELL * CELL);
    dx = abs((g10[IN_CONT] - g00[IN_CONT]) * (CELL - fz) + (g11[IN_CONT] - g01[IN_CONT]) * fz) / (CELL * CELL);
    dz = abs((g01[IN_CONT] - g00[IN_CONT]) * (CELL - fx) + (g11[IN_CONT] - g10[IN_CONT]) * fx) / (CELL * CELL);
    in[IN_COAST_SLOPE] = dx > dz ? dx + dz / 2 : dz + dx / 2;
}

/* 0..ONE: highest along narrow ridges where v is a half, falling away
 * fast either side. */
static int ridged(int v)
{
    int r = ONE - abs(2 * v - ONE);

    return (int)((int64_t)r * r / ONE);
}

static int ramp(int v, int from, int range)
{
    return clamp_int((int)((int64_t)(v - from) * ONE / range), 0, ONE);
}

/* How far the mountains raise column (x, z), a its mountain amount (above
 * zero). *cliff: on a cliff face. */
static int mountain_raise(uint32_t seed, int x, int z, const int in[IN_COUNT], int a, int *cliff)
{
    int shape = (5 * ridged(noise_value(seed, &k_ridge1, x, z)) +
                 3 * ridged(noise_value(seed, &k_ridge2, x, z)) +
                 2 * noise_value(seed, &k_peak, x, z)) / 10;
    int foot = noise_smooth(a);
    int64_t up = BLK(MOUNT_BASE) + (int64_t)MOUNT_PEAK * shape;
    int raise = (int)(foot * up / ONE);
    int w = (int)((int64_t)ramp(a, CLIFF_A0, CLIFF_RANGE) * ramp(in[IN_CRAG], CRAG0, CRAG_RANGE) / ONE);

    *cliff = 0;
    if (w > 0) {
        int off = (int)((int64_t)in[IN_PHASE] * TERRACE / ONE);
        int t = raise + off, f = t % TERRACE;
        int64_t g = f < LEDGE ? (int64_t)f * LEDGE_RISE / LEDGE
                              : LEDGE_RISE + (int64_t)(f - LEDGE) * (TERRACE - LEDGE_RISE) / (TERRACE - LEDGE);
        int terraced = t - f + (int)g - off;

        raise += (int)((int64_t)w * (terraced - raise) / ONE);
        *cliff = w > ONE / 2 && f >= LEDGE;
    }
    return raise;
}

static TerrainColumn make_column(uint32_t seed, int x, int z, const int in[IN_COUNT])
{
    TerrainColumn col;
    int s = in[IN_CONT] - COAST;
    int off_mid = abs(in[IN_RIVER]) - RIVER_EDGE;
    int amp = 0, hills = 0, land, raise = 0, cliff = 0, river, rel, h, a;

    if (s < 0) {
        int floor = BLK(TERRAIN_FLOOR - TERRAIN_SEA - 1) + SEA_SWELL * in[IN_SWELL] + SEA_HILL * in[IN_HILL];

        land = s * K_SEA > floor ? s * K_SEA : floor;
    } else {
        amp = clamp_int(K_EROS * (in[IN_EROS] - EROS0), 0, BLK(HILL_UP));
        hills = (int)((int64_t)amp * in[IN_HILL] / ONE);
        land = min_int(s * K_LAND, BLK(PLAINS_UP) + 2 * in[IN_SWELL] + hills +
                                       3 * noise_value(seed, &k_bumps, x, z) / 2);
    }

    a = min_int(in[IN_MOUNT], MOUNT0 + s - MOUNT_S) - MOUNT0;
    a = a > 0 ? clamp_int((int)((int64_t)a * ONE / MOUNT_RANGE), 1, ONE) : 0;
    if (a > 0) /* mountains take the place of the hills */
        raise = mountain_raise(seed, x, z, in, a, &cliff) - (int)((int64_t)noise_smooth(a) * hills / ONE);

    river = RIVER_BED + K_RIVER * (off_mid > 0 ? off_mid : 0) + K_RIVER_MOUNT * a;
    rel = min_int(land + raise, river);
    if (rel > SOFT_TOP)
        rel = SOFT_TOP + (rel - SOFT_TOP) / 2;
    h = min_int(TERRAIN_SEA + 1 + floor_div(rel, ONE), TERRAIN_PEAK);

    col.height = h;
    col.surface = BLOCK_GRASS;
    if (a > 0) {
        col.kind = TERRAIN_MOUNTAIN;
        if (h <= TERRAIN_SEA)
            col.surface = BLOCK_SAND;
        else if (cliff || h >= STONE_TOP)
            col.surface = BLOCK_STONE;
    } else if (h <= TERRAIN_SEA) {
        col.surface = BLOCK_SAND;
        col.kind = s >= 0 ? TERRAIN_RIVER : h == TERRAIN_SEA ? TERRAIN_BEACH : TERRAIN_OCEAN;
    } else if (h <= TERRAIN_SEA + BEACH_UP && s < BEACH_W * in[IN_COAST_SLOPE]) {
        col.surface = BLOCK_SAND;
        col.kind = TERRAIN_BEACH;
    } else {
        col.kind = amp >= BLK(HILLS_KIND) ? TERRAIN_HILLS : TERRAIN_PLAINS;
    }
    return col;
}

TerrainColumn terrain_column(uint32_t seed, int x, int z)
{
    int gx = floor_div(x, CELL) * CELL, gz = floor_div(z, CELL) * CELL;
    int g[4][IN_COUNT], in[IN_COUNT];

    slow_inputs(seed, gx, gz, g[0]);
    slow_inputs(seed, gx + CELL, gz, g[1]);
    slow_inputs(seed, gx, gz + CELL, g[2]);
    slow_inputs(seed, gx + CELL, gz + CELL, g[3]);
    blend(g[0], g[1], g[2], g[3], x - gx, z - gz, in);
    return make_column(seed, x, z, in);
}

int terrain_height(uint32_t seed, int x, int z)
{
    return terrain_column(seed, x, z).height;
}

void terrain_chunk_columns(uint32_t seed, int cx, int cz,
                           TerrainColumn out[CHUNK_SIZE][CHUNK_SIZE])
{
    enum { N = CHUNK_SIZE / CELL + 1 };
    int g[N][N][IN_COUNT];
    int x0 = cx * CHUNK_SIZE, z0 = cz * CHUNK_SIZE, i, j, x, z;

    for (j = 0; j < N; j++)
        for (i = 0; i < N; i++)
            slow_inputs(seed, x0 + i * CELL, z0 + j * CELL, g[j][i]);
    for (z = 0; z < CHUNK_SIZE; z++)
        for (x = 0; x < CHUNK_SIZE; x++) {
            int in[IN_COUNT];

            i = x / CELL;
            j = z / CELL;
            blend(g[j][i], g[j][i + 1], g[j + 1][i], g[j + 1][i + 1], x % CELL, z % CELL, in);
            out[z][x] = make_column(seed, x0 + x, z0 + z, in);
        }
}

void terrain_fill_chunk(Chunk *c, uint32_t seed, int cx, int cz)
{
    TerrainColumn col[CHUNK_SIZE][CHUNK_SIZE];
    int x, y, z, low = WORLD_HEIGHT;

    terrain_chunk_columns(seed, cx, cz, col);
    for (z = 0; z < CHUNK_SIZE; z++)
        for (x = 0; x < CHUNK_SIZE; x++)
            low = min_int(low, col[z][x].height - TERRAIN_DIRT);
    /* Stone up to the lowest dirt, air above; then each column. */
    memset(c->blocks, BLOCK_STONE, (size_t)low * sizeof c->blocks[0]);
    memset(c->blocks[low], BLOCK_AIR, (size_t)(WORLD_HEIGHT - low) * sizeof c->blocks[0]);
    for (z = 0; z < CHUNK_SIZE; z++)
        for (x = 0; x < CHUNK_SIZE; x++) {
            int h = col[z][x].height, top = col[z][x].surface;
            int under = top == BLOCK_GRASS ? BLOCK_DIRT : top;

            for (y = low; y < h - TERRAIN_DIRT; y++)
                c->blocks[y][z][x] = BLOCK_STONE;
            for (; y < h; y++)
                c->blocks[y][z][x] = (uint8_t)under;
            c->blocks[h][z][x] = (uint8_t)top;
            for (y = h + 1; y <= TERRAIN_SEA; y++)
                c->blocks[y][z][x] = BLOCK_WATER;
        }
}
