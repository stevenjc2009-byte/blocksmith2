#include <stdlib.h>
#include <string.h>

#include "biome.h"
#include "block.h"
#include "mesh.h"
#include "terrain.h"
#include "test.h"

/* Biomes (row 4-02), the fixed parts: the heights, the tables, the tint. */

/* The grass tint of a biome on the six vertices of a reference grass top
 * (what chunk_mesh.c does; the reference meshers call this). */
void test_ref_grass_tint(MeshVertex *face, int biome)
{
    float g[3];
    int q;

    biome_grass_rgb(biome, g);
    for (q = 0; q < MESH_FACE_VERTS; q++) {
        face[q].r *= g[0];
        face[q].g *= g[1];
        face[q].b *= g[2];
    }
}

static int is_band(int b)
{
    return b >= BLOCK_TERRACOTTA && b <= BLOCK_TERRACOTTA_BROWN;
}

/* terrain_height() is exactly what it was before biomes (at e9c296c, the
 * mountain-wall terrain): a table of columns and a hash of whole squares. */
static void test_heights_pinned(void)
{
    static const struct { uint32_t seed; int h[16]; uint32_t hash; } k_pin[] = {
        { 2026u, { 55, 85, 46, 43, 48, 62, 60, 46, 53, 64, 66, 46, 43, 107, 51, 47 }, 0x12b36651u },
        { 7u, { 66, 52, 47, 48, 67, 62, 48, 71, 62, 73, 66, 66, 49, 68, 63, 82 }, 0xa25c9272u },
        { 0xC0FFEEu, { 69, 67, 72, 64, 84, 71, 60, 45, 43, 46, 60, 48, 61, 67, 55, 48 }, 0xbe356bbbu },
    };
    int i, k, x, z, bad = 0;

    for (i = 0; i < 3; i++) {
        uint32_t h = 2166136261u;

        for (k = 0; k < 16; k++) {
            int px = (k * 7919) % 4001 - 2000, pz = (k * 104729) % 4001 - 2000;

            bad += terrain_height(k_pin[i].seed, px, pz) != k_pin[i].h[k];
            bad += terrain_column(k_pin[i].seed, px, pz).height != k_pin[i].h[k];
        }
        for (z = -300; z < 300; z++)
            for (x = -300; x < 300; x++)
                h = (h ^ (uint32_t)terrain_height(k_pin[i].seed, x, z)) * 16777619u;
        if (h != k_pin[i].hash)
            printf("     (seed %u: height hash %08x, wanted %08x)\n", (unsigned)k_pin[i].seed, (unsigned)h,
                   (unsigned)k_pin[i].hash);
        bad += h != k_pin[i].hash;
    }
    CHECK_EQ_INT(bad, 0);
}

/* The tables: names, tints, water colours. */
void test_biome_tables(void)
{
    test_heights_pinned();
    int b, bad = 0, i;
    float g[3], f[3], w[3];

    CHECK_EQ_INT(BIOME_COUNT, 20);
    for (b = 0; b < BIOME_COUNT; b++) {
        biome_grass_rgb(b, g);
        biome_foliage_rgb(b, f);
        biome_water_rgb(b, w);
        for (i = 0; i < 3; i++)
            bad += !(g[i] > 0.0f && g[i] <= 1.0f && f[i] > 0.0f && f[i] <= 1.0f && w[i] > 0.0f && w[i] <= 1.0f);
        bad += strcmp(biome_name((Biome)b), "?") == 0;
        for (i = 0; i < b; i++)
            bad += strcmp(biome_name((Biome)b), biome_name((Biome)i)) == 0;
    }
    CHECK_EQ_INT(bad, 0);
    CHECK_EQ_INT(strcmp(biome_name(BIOME_COUNT), "?"), 0);
    /* Plains keep the colour the grass had before biomes: (88, 150, 50). */
    biome_grass_rgb(BIOME_PLAINS, g);
    CHECK_NEAR(118.0f * g[0], 88.0f, 2.0);
    CHECK_NEAR(178.0f * g[1], 150.0f, 2.0);
    CHECK_NEAR(84.0f * g[2], 50.0f, 2.0);
    /* A value out of range gives plains, and the biomes' greens differ. */
    biome_grass_rgb(200, f);
    CHECK_EQ_INT(memcmp(f, g, sizeof f), 0);
    biome_grass_rgb(BIOME_SAVANNA, f);
    CHECK_EQ_INT(f[0] > g[0] && f[2] < g[2], 1); /* yellower */
    biome_water_rgb(BIOME_SWAMP, w);
    CHECK_EQ_INT(w[0] < 1.0f && w[2] < 1.0f, 1);
    biome_water_rgb(BIOME_PLAINS, w);
    CHECK_EQ_INT(w[0] == 1.0f && w[1] == 1.0f && w[2] == 1.0f, 1);
    CHECK_EQ_INT(biome_dry(BIOME_DESERT) && biome_dry(BIOME_SAVANNA) && biome_dry(BIOME_BADLANDS), 1);
    CHECK_EQ_INT(biome_dry(BIOME_PLAINS) || biome_dry(BIOME_JUNGLE) || biome_dry(BIOME_OCEAN), 0);
    CHECK_EQ_INT(biome_snowy(BIOME_SNOWY_TAIGA) && biome_snowy(BIOME_FROZEN_RIVER) && !biome_snowy(BIOME_TAIGA), 1);
    CHECK_EQ_INT(biome_frozen(BIOME_FROZEN_OCEAN) && biome_frozen(BIOME_FROZEN_RIVER) && !biome_frozen(BIOME_COLD_OCEAN), 1);
    for (i = 0; i < 32; i++)
        bad += !is_band(terrain_band_block(i - 7)) || terrain_band_block(i) != terrain_band_block(i + 32);
    CHECK_EQ_INT(bad, 0);
    /* The five band blocks all appear in a period, the plain one most. */
    {
        int seen[BLOCK_COUNT] = { 0 }, kinds = 0;

        for (i = 0; i < 32; i++)
            seen[terrain_band_block(i)]++;
        for (i = 0; i < BLOCK_COUNT; i++)
            kinds += seen[i] > 0;
        CHECK_EQ_INT(kinds, 5);
        CHECK_EQ_INT(seen[BLOCK_TERRACOTTA] > 12 && seen[BLOCK_TERRACOTTA_BROWN] > 0, 1);
    }
}
