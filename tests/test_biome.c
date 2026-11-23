#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "biome.h"
#include "block.h"
#include "chunk.h"
#include "terrain.h"
#include "test.h"
#include "weather.h"

/* Biomes (row 4-02): where each one is, the blocks it makes, the snow line,
 * the ice, the weather, and that the heights did not move. */

static const uint32_t k_seeds[] = { 2026u, 7u, 0xC0FFEEu, 1u };
#define SEEDS ((int)(sizeof k_seeds / sizeof k_seeds[0]))

/* The chunks looked at round the origin, each way. */
#define REACH 96

typedef struct {
    long count[BIOME_COUNT];
    long hot_cold;     /* next-door columns, one warm-ish biome, one snowy */
    long slivers;      /* a land biome one column wide between two of another */
    long bad_blocks;   /* surface or under block the biome should not have */
    long bad_snow;     /* a mountain top above the snow line without snow, or snow below it */
    long bad_weather;  /* weather_cold() or weather_dry() against the biome */
    long snow_tops;    /* mountain columns with snow */
    long swamp_high;   /* swamp land more than 8 blocks over the sea */
    long bad_biome;    /* terrain_biome() not the column's, or a biome out of range */
} Scan;

static Scan s_scan[SEEDS];

static int cold(int b)
{
    return b == BIOME_SNOWY_PLAINS || b == BIOME_SNOWY_TAIGA || b == BIOME_FROZEN_OCEAN ||
           b == BIOME_SNOWY_BEACH || b == BIOME_FROZEN_RIVER;
}

static int warm(int b)
{
    return b == BIOME_SAVANNA || b == BIOME_JUNGLE || b == BIOME_DESERT || b == BIOME_BADLANDS ||
           b == BIOME_WARM_OCEAN;
}

static int land(int b)
{
    return b <= BIOME_BADLANDS;
}

/* Is (surface, under, depth) what this biome makes on a column of this kind
 * (mountain tops above sea level are checked on their own)? */
static int blocks_ok(const TerrainColumn *c)
{
    int s = c->surface, u = c->under, k = c->kind;

    switch ((Biome)c->biome) {
    case BIOME_SNOWY_PLAINS:
    case BIOME_SNOWY_TAIGA:
        return s == BLOCK_SNOWY_GRASS && u == BLOCK_DIRT && c->depth == TERRAIN_DIRT;
    case BIOME_DESERT:
        return s == BLOCK_SAND && u == BLOCK_SANDSTONE && c->depth == TERRAIN_DIRT;
    case BIOME_BADLANDS: /* flat: red sand, else the band of its height */
        return (s == BLOCK_RED_SAND || (k != TERRAIN_PLAINS && k != TERRAIN_BEACH && s == terrain_band_block(c->height))) &&
               u == BLOCK_TERRACOTTA && c->depth == TERRAIN_BADLANDS_DEPTH;
    case BIOME_SWAMP: /* grass on the land, a sand bed under its rivers */
        if (k != TERRAIN_RIVER)
            return s == BLOCK_GRASS && u == BLOCK_DIRT && c->depth == TERRAIN_DIRT;
        return s == BLOCK_SAND && u == BLOCK_SAND;
    case BIOME_OCEAN:
    case BIOME_WARM_OCEAN:
    case BIOME_RIVER:
    case BIOME_BEACH:
        return s == BLOCK_SAND && u == BLOCK_SAND;
    case BIOME_COLD_OCEAN:
    case BIOME_FROZEN_OCEAN:
        return s == BLOCK_GRAVEL && u == BLOCK_GRAVEL;
    case BIOME_FROZEN_RIVER:
        return s == BLOCK_SAND && u == BLOCK_SAND;
    case BIOME_SNOWY_BEACH:
        return s == BLOCK_SNOW && u == BLOCK_SAND;
    default: /* the grass biomes */
        return s == BLOCK_GRASS && u == BLOCK_DIRT && c->depth == TERRAIN_DIRT;
    }
}

static void scan_seed(int si)
{
    Scan *r = &s_scan[si];
    TerrainColumn col[CHUNK_SIZE][CHUNK_SIZE];
    int cx, cz, x, z;

    memset(r, 0, sizeof *r);
    for (cz = -REACH; cz < REACH; cz++)
        for (cx = -REACH; cx < REACH; cx++) {
            terrain_chunk_columns(k_seeds[si], cx, cz, col);
            for (z = 0; z < CHUNK_SIZE; z++)
                for (x = 0; x < CHUNK_SIZE; x++) {
                    const TerrainColumn *c = &col[z][x];
                    int b = c->biome;

                    if (b >= BIOME_COUNT) {
                        r->bad_biome++;
                        continue;
                    }
                    r->count[b]++;
                    if (c->kind != TERRAIN_MOUNTAIN || c->height <= TERRAIN_SEA)
                        r->bad_blocks += !blocks_ok(c);
                    if (b == BIOME_SWAMP && c->kind != TERRAIN_RIVER && c->height > TERRAIN_SEA + 8)
                        r->swamp_high++;
                    if (x + 1 < CHUNK_SIZE) {
                        int o = col[z][x + 1].biome;

                        r->hot_cold += (cold(b) && warm(o)) || (warm(b) && cold(o));
                    }
                    if (z + 1 < CHUNK_SIZE) {
                        int o = col[z + 1][x].biome;

                        r->hot_cold += (cold(b) && warm(o)) || (warm(b) && cold(o));
                    }
                    if (x > 0 && x + 1 < CHUNK_SIZE && land(b)) {
                        int l = col[z][x - 1].biome, o = col[z][x + 1].biome;

                        r->slivers += l == o && l != b && land(l);
                    }
                    if (z > 0 && z + 1 < CHUNK_SIZE && land(b)) {
                        int l = col[z - 1][x].biome, o = col[z + 1][x].biome;

                        r->slivers += l == o && l != b && land(l);
                    }
                    if (x == 8 && z == 8) { /* one column a chunk: the weather and the snow line */
                        int wx = cx * CHUNK_SIZE + x, wz = cz * CHUNK_SIZE + z, temp, humid;

                        biome_climate(k_seeds[si], wx, wz, &temp, &humid);
                        r->bad_weather += weather_cold(k_seeds[si], wx, wz) != (biome_snowy((Biome)b) ? 1 : 0);
                        if (land(b) && b != BIOME_SWAMP)
                            r->bad_weather += weather_dry(k_seeds[si], wx, wz) != (biome_dry((Biome)b) ? 1 : 0);
                        if (b == BIOME_SWAMP)
                            r->bad_weather += weather_dry(k_seeds[si], wx, wz) || weather_cold(k_seeds[si], wx, wz);
                        r->bad_biome += terrain_biome(k_seeds[si], wx, wz) != (Biome)b;
                        /* the land biome is the climate's (biome_climate() is terrain's own blend) */
                        if (land(b) && b != BIOME_SWAMP && c->kind >= TERRAIN_PLAINS && c->height > TERRAIN_SEA)
                            r->bad_biome += biome_land(temp, humid) != (Biome)b;
                    }
                    if (c->kind == TERRAIN_MOUNTAIN && c->height > TERRAIN_SEA) {
                        int wx = cx * CHUNK_SIZE + x, wz = cz * CHUNK_SIZE + z, temp, humid;
                        int above;

                        biome_climate(k_seeds[si], wx, wz, &temp, &humid);
                        above = c->height >= biome_snow_line(temp);
                        r->bad_snow += (c->surface == BLOCK_SNOW) != above;
                        r->snow_tops += c->surface == BLOCK_SNOW;
                        r->bad_weather += weather_snows(k_seeds[si], wx, wz, c->height + 1) !=
                                          (above || biome_snowy((Biome)b));
                    }
                }
        }
}

/* Every biome appears over a large area, for at least 3 of the 4 seeds. */
static void test_every_biome(void)
{
    int si, b;

    for (si = 0; si < SEEDS; si++)
        scan_seed(si);
    for (b = 0; b < BIOME_COUNT; b++) {
        int seeds = 0;

        for (si = 0; si < SEEDS; si++)
            seeds += s_scan[si].count[b] >= 500; /* at least two chunks' worth */
        if (seeds < 3)
            printf("     (biome %s in %d of %d seeds)\n", biome_name((Biome)b), seeds, SEEDS);
        CHECK_EQ_INT(seeds >= 3, 1);
    }
    for (si = 0; si < SEEDS; si++)
        CHECK_EQ_INT(s_scan[si].bad_biome, 0);
}

static void test_scan_rules(void)
{
    int si;

    for (si = 0; si < SEEDS; si++) {
        const Scan *r = &s_scan[si];

        CHECK_EQ_INT(r->hot_cold, 0);      /* a hot biome never touches a cold one */
        /* No one-column strips of a land biome, but for a few tiny lenses where
         * a smooth climate map just touches a band edge (known limit: about 1
         * column in 100000; real strips would be 100 times as many). */
        CHECK_EQ_INT(r->slivers * 40000 < (long)4 * REACH * REACH * CHUNK_SIZE * CHUNK_SIZE, 1);
        CHECK_EQ_INT(r->bad_blocks, 0);    /* the blocks of each biome */
        CHECK_EQ_INT(r->bad_snow, 0);      /* snow exactly from the snow line up */
        CHECK_EQ_INT(r->snow_tops > 0, 1); /* and there are snowy peaks */
        CHECK_EQ_INT(r->bad_weather, 0);   /* the weather follows the biome */
        CHECK_EQ_INT(r->swamp_high, 0);    /* swamps are low land */
    }
}

/* Temperature bands change by one at a time along a line, either way. */
static void test_bands(void)
{
    int si, x, bad = 0, steps = 0;

    for (si = 0; si < SEEDS; si++) {
        int t, h, was;

        biome_climate(k_seeds[si], -9000, 41, &t, &h);
        was = biome_temp_band(t);
        for (x = -8999; x < 9000; x++) {
            int band;

            biome_climate(k_seeds[si], x, 41, &t, &h);
            band = biome_temp_band(t);
            bad += abs(band - was) > 1 || t < 0 || h < 0 || t >= 65536 || h >= 65536;
            steps += band != was;
            was = band;
        }
    }
    CHECK_EQ_INT(bad, 0);
    CHECK_EQ_INT(steps > 8, 1); /* but the bands do change */
    CHECK_EQ_INT(biome_temp_band(0), BIOME_TEMP_SNOWY);
    CHECK_EQ_INT(biome_temp_band(BIOME_T_COOL), BIOME_TEMP_COOL);
    CHECK_EQ_INT(biome_temp_band(BIOME_T_COOL - 1), BIOME_TEMP_SNOWY);
    CHECK_EQ_INT(biome_temp_band(65535), BIOME_TEMP_HOT);
}

/* The snow line is lower in the cold and never goes down as it warms. */
static void test_snow_line(void)
{
    int t, bad = 0, prev = biome_snow_line(0);

    for (t = 1; t < 65536; t += 64) {
        int line = biome_snow_line(t);

        bad += line < prev;
        prev = line;
    }
    CHECK_EQ_INT(bad, 0);
    CHECK_EQ_INT(biome_snow_line(0) < biome_snow_line(65535) - 30, 1);
    CHECK_EQ_INT(biome_snow_line(0) > TERRAIN_SEA + 8, 1);
    CHECK_EQ_INT(biome_snow_line(65535) > TERRAIN_PEAK - 6, 1); /* hot places: almost none */
}

/* Find the chunk of the first column of one of these biomes (seas and
 * rivers) over a wide look; false if none. */
static bool find_biome(uint32_t seed, Biome b, int *cx, int *cz)
{
    TerrainColumn col[CHUNK_SIZE][CHUNK_SIZE];
    int x, z, i, j;

    for (j = -REACH; j < REACH; j++)
        for (i = -REACH; i < REACH; i++) {
            terrain_chunk_columns(seed, i, j, col);
            for (z = 0; z < CHUNK_SIZE; z++)
                for (x = 0; x < CHUNK_SIZE; x++)
                    if (col[z][x].biome == b && col[z][x].height < TERRAIN_SEA - 1) {
                        *cx = i;
                        *cz = j;
                        return true;
                    }
        }
    return false;
}

/* Frozen seas and rivers have ice at TERRAIN_SEA over water, warm ones
 * have water to the top; the biome array of a filled chunk is its columns'. */
static void test_ice(void)
{
    static const Biome k_water[] = { BIOME_FROZEN_OCEAN, BIOME_FROZEN_RIVER, BIOME_OCEAN, BIOME_COLD_OCEAN,
                                     BIOME_WARM_OCEAN, BIOME_RIVER };
    static Chunk c;
    int i, si;

    for (si = 0; si < 2; si++)
        for (i = 0; i < (int)(sizeof k_water / sizeof k_water[0]); i++) {
            int cx = 0, cz = 0, x, z, ice = 0, water = 0, wrong = 0, biomes = 0;
            Biome b = k_water[i];

            if (!find_biome(k_seeds[si], b, &cx, &cz)) {
                printf("     (no %s found for seed %u)\n", biome_name(b), (unsigned)k_seeds[si]);
                CHECK_EQ_INT(0, 1);
                continue;
            }
            memset(&c, 0xFF, sizeof c);
            terrain_fill_chunk(&c, k_seeds[si], cx, cz);
            for (z = 0; z < CHUNK_SIZE; z++)
                for (x = 0; x < CHUNK_SIZE; x++) {
                    TerrainColumn t = terrain_column(k_seeds[si], cx * CHUNK_SIZE + x, cz * CHUNK_SIZE + z);
                    int top = chunk_get(&c, x, TERRAIN_SEA, z);

                    biomes += c.biome[z][x] != t.biome;
                    if (t.height >= TERRAIN_SEA)
                        continue;
                    if (biome_frozen((Biome)t.biome)) {
                        ice += top == BLOCK_ICE;
                        wrong += top != BLOCK_ICE;
                        wrong += t.height < TERRAIN_SEA - 1 && chunk_get(&c, x, TERRAIN_SEA - 1, z) != BLOCK_WATER;
                    } else {
                        water += top == BLOCK_WATER;
                        wrong += top != BLOCK_WATER;
                    }
                    wrong += chunk_get(&c, x, TERRAIN_SEA + 1, z) != BLOCK_AIR;
                }
            CHECK_EQ_INT(wrong, 0);
            CHECK_EQ_INT(biomes, 0);
            CHECK_EQ_INT(i < 2 ? ice > 0 : water > 0, 1);
        }
}

/* terrain_fill_biomes() gives what filling the chunk gave. */
static void test_fill_biomes(void)
{
    static Chunk a, b;
    int k, bad = 0;

    for (k = 0; k < 6; k++) {
        uint32_t seed = k_seeds[k % SEEDS];
        int cx = -7 + 5 * k, cz = 9 - 4 * k;

        memset(&b, 0xFF, sizeof b);
        terrain_fill_chunk(&a, seed, cx, cz);
        terrain_fill_biomes(&b, seed, cx, cz);
        bad += memcmp(a.biome, b.biome, sizeof a.biome) != 0;
    }
    CHECK_EQ_INT(bad, 0);
}

void test_biome(void)
{
    test_biome_tables();
    test_snow_line();
    test_bands();
    test_every_biome();
    test_scan_rules();
    test_ice();
    test_fill_biomes();
}
