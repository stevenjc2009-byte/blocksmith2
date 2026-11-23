#include "biome.h"

#include "terrain_noise.h"

#define ONE NOISE_ONE

/* The slow maps, each two layers (terrain_noise.h): a wide one and a finer
 * one that bends the borders a little. About 285 and 175 blocks across for
 * temperature, 285 and 190 for humidity. */
static const NoiseLayer k_temp1 = { 31, 2, 3, 10 };
static const NoiseLayer k_temp2 = { 33, 3, 5, 10 };
static const NoiseLayer k_humid1 = { 32, 3, 2, 10 };
static const NoiseLayer k_humid2 = { 34, 5, 2, 10 };

/* The layers' values bunch round the middle; stretch them so the bands at
 * the ends get their share. */
#define STRETCH_NUM 5
#define STRETCH_DEN 3

static int stretch(int v)
{
    int s = ONE / 2 + (int)((int64_t)(v - ONE / 2) * STRETCH_NUM / STRETCH_DEN);

    return s < 0 ? 0 : s > ONE - 1 ? ONE - 1 : s;
}

/* Humidity bands: from dry (0) to wet (3), starting at these. */
#define H_1 (BIOME_UNIT * 85)
#define H_2 (BIOME_UNIT * 138)
#define H_3 (BIOME_UNIT * 194)
#define H_SWAMP (BIOME_UNIT * 218)

void biome_climate_point(uint32_t seed, int x, int z, int *temp, int *humid)
{
    *temp = stretch((3 * noise_value(seed, &k_temp1, x, z) + noise_value(seed, &k_temp2, x, z)) / 4);
    *humid = stretch((3 * noise_value(seed, &k_humid1, x, z) + noise_value(seed, &k_humid2, x, z)) / 4);
}

void biome_climate_blend(const int g00[2], const int g10[2], const int g01[2], const int g11[2],
                         int fx, int fz, int out[2])
{
    int i;

    for (i = 0; i < 2; i++)
        out[i] = (g00[i] * (BIOME_CELL - fx) * (BIOME_CELL - fz) + g10[i] * fx * (BIOME_CELL - fz) +
                  g01[i] * (BIOME_CELL - fx) * fz + g11[i] * fx * fz) / (BIOME_CELL * BIOME_CELL);
}

/* Rounds down for negative a too. */
static int floor_div(int a, int size)
{
    return a >= 0 ? a / size : -((size - 1 - a) / size);
}

void biome_climate(uint32_t seed, int x, int z, int *temp, int *humid)
{
    int gx = floor_div(x, BIOME_CELL) * BIOME_CELL, gz = floor_div(z, BIOME_CELL) * BIOME_CELL;
    int g[4][2], out[2];

    biome_climate_point(seed, gx, gz, &g[0][0], &g[0][1]);
    biome_climate_point(seed, gx + BIOME_CELL, gz, &g[1][0], &g[1][1]);
    biome_climate_point(seed, gx, gz + BIOME_CELL, &g[2][0], &g[2][1]);
    biome_climate_point(seed, gx + BIOME_CELL, gz + BIOME_CELL, &g[3][0], &g[3][1]);
    biome_climate_blend(g[0], g[1], g[2], g[3], x - gx, z - gz, out);
    *temp = out[0];
    *humid = out[1];
}

int biome_temp_band(int temp)
{
    return temp < BIOME_T_COOL ? BIOME_TEMP_SNOWY
         : temp < BIOME_T_TEMPERATE ? BIOME_TEMP_COOL
         : temp < BIOME_T_WARM ? BIOME_TEMP_TEMPERATE
         : temp < BIOME_T_HOT ? BIOME_TEMP_WARM : BIOME_TEMP_HOT;
}

/* The land biome of each temperature band (rows) and humidity band, dry to
 * wet (columns). */
static const unsigned char k_land[BIOME_TEMP_BANDS][4] = {
    { BIOME_SNOWY_PLAINS, BIOME_SNOWY_PLAINS, BIOME_SNOWY_TAIGA, BIOME_SNOWY_TAIGA },
    { BIOME_PLAINS, BIOME_BIRCH_FOREST, BIOME_TAIGA, BIOME_TAIGA },
    { BIOME_PLAINS, BIOME_FOREST, BIOME_BIRCH_FOREST, BIOME_DARK_FOREST },
    { BIOME_SAVANNA, BIOME_SAVANNA, BIOME_FOREST, BIOME_JUNGLE },
    { BIOME_DESERT, BIOME_DESERT, BIOME_BADLANDS, BIOME_SAVANNA },
};

Biome biome_land(int temp, int humid)
{
    int h = humid < H_1 ? 0 : humid < H_2 ? 1 : humid < H_3 ? 2 : 3;

    return (Biome)k_land[biome_temp_band(temp)][h];
}

bool biome_swamp_climate(int temp, int humid)
{
    int band = biome_temp_band(temp);

    return humid >= H_SWAMP && (band == BIOME_TEMP_TEMPERATE || band == BIOME_TEMP_WARM);
}

Biome biome_ocean(int temp)
{
    switch (biome_temp_band(temp)) {
    case BIOME_TEMP_SNOWY:
        return BIOME_FROZEN_OCEAN;
    case BIOME_TEMP_COOL:
        return BIOME_COLD_OCEAN;
    case BIOME_TEMP_HOT:
        return BIOME_WARM_OCEAN;
    default:
        return BIOME_OCEAN;
    }
}

Biome biome_river(int temp)
{
    return biome_temp_band(temp) == BIOME_TEMP_SNOWY ? BIOME_FROZEN_RIVER : BIOME_RIVER;
}

/* Deserts and badlands keep their own sand to the water's edge. */
Biome biome_beach(int temp, Biome land)
{
    if (land == BIOME_DESERT || land == BIOME_BADLANDS)
        return land;
    return biome_temp_band(temp) == BIOME_TEMP_SNOWY ? BIOME_SNOWY_BEACH : BIOME_BEACH;
}

bool biome_dry(Biome b)
{
    return b == BIOME_DESERT || b == BIOME_SAVANNA || b == BIOME_BADLANDS;
}

bool biome_snowy(Biome b)
{
    return b == BIOME_SNOWY_PLAINS || b == BIOME_SNOWY_TAIGA || b == BIOME_FROZEN_OCEAN ||
           b == BIOME_SNOWY_BEACH || b == BIOME_FROZEN_RIVER;
}

bool biome_frozen(Biome b)
{
    return b == BIOME_FROZEN_OCEAN || b == BIOME_FROZEN_RIVER;
}

/* 74 at the coldest up to 130 at the hottest: past the highest peak (118)
 * from a temperature of 0.79 up. */
int biome_snow_line(int temp)
{
    return 74 + (int)((int64_t)temp * 56 / ONE);
}

static const char *const k_names[BIOME_COUNT] = {
    "plains", "forest", "birch_forest", "dark_forest", "taiga", "snowy_plains", "snowy_taiga",
    "swamp", "savanna", "jungle", "desert", "badlands", "ocean", "warm_ocean", "cold_ocean",
    "frozen_ocean", "beach", "snowy_beach", "river", "frozen_river",
};

const char *biome_name(Biome b)
{
    return (int)b >= 0 && (int)b < BIOME_COUNT ? k_names[b] : "?";
}

/* How each biome tints grass seen from above, red, green, blue each 0 to 1
 * (the grass tile is a light green, 118, 178, 84, so plains give back the
 * green it was before row 4-02). Biomes with no grass use plains. */
static const float k_grass[BIOME_COUNT][3] = {
    { 0.75f, 0.84f, 0.60f }, /* plains */
    { 0.68f, 0.80f, 0.55f }, /* forest: a little darker */
    { 0.84f, 0.93f, 0.58f }, /* birch forest: light, yellower */
    { 0.52f, 0.68f, 0.50f }, /* dark forest */
    { 0.55f, 0.80f, 0.66f }, /* taiga: bluish */
    { 0.52f, 0.78f, 0.72f }, /* snowy plains: bluish */
    { 0.52f, 0.78f, 0.72f }, /* snowy taiga */
    { 0.50f, 0.60f, 0.42f }, /* swamp: dull and dark */
    { 0.98f, 0.93f, 0.50f }, /* savanna: yellowish */
    { 0.52f, 1.00f, 0.45f }, /* jungle: bright */
    { 0.95f, 0.90f, 0.50f }, /* desert */
    { 0.90f, 0.82f, 0.45f }, /* badlands: yellowish */
};

/* The colour of each biome's water, as what the water's blue is multiplied
 * by (0 to 1 each): warm seas lose red and go turquoise against the blue,
 * cold ones darken, a swamp's is dark and green. */
static const float k_water[BIOME_COUNT][3] = {
    [BIOME_SWAMP] = { 0.45f, 0.62f, 0.48f },
    [BIOME_WARM_OCEAN] = { 0.55f, 1.00f, 0.92f },
    [BIOME_COLD_OCEAN] = { 0.75f, 0.85f, 0.95f },
    [BIOME_FROZEN_OCEAN] = { 0.62f, 0.76f, 0.90f },
    [BIOME_FROZEN_RIVER] = { 0.62f, 0.76f, 0.90f },
};

static int valid(int biome)
{
    return biome >= 0 && biome < BIOME_COUNT;
}

void biome_grass_rgb(int biome, float rgb[3])
{
    int i, b = valid(biome) && biome <= BIOME_BADLANDS ? biome : BIOME_PLAINS;

    for (i = 0; i < 3; i++)
        rgb[i] = k_grass[b][i];
}

void biome_foliage_rgb(int biome, float rgb[3])
{
    int i;

    biome_grass_rgb(biome, rgb);
    for (i = 0; i < 3; i++)
        rgb[i] *= 0.92f; /* leaves are a little darker than grass */
}

void biome_water_rgb(int biome, float rgb[3])
{
    int i, b = valid(biome) ? biome : BIOME_PLAINS;

    for (i = 0; i < 3; i++)
        rgb[i] = k_water[b][i] > 0.0f ? k_water[b][i] : 1.0f;
}
