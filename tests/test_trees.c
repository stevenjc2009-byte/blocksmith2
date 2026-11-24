#include <stdlib.h>

#include "biome.h"
#include "block.h"
#include "terrain.h"
#include "test.h"
#include "trees.h"

/* Row 4-03: which trees grow in which biomes, how thick, and which plants
 * grow on which ground (trees.h). The ground and the seamless writing are
 * in test_trees_place.c, the blocks and meshes in test_trees_mesh.c. */

static const uint32_t k_seeds[] = { 2026u, 1u, 77u, 12345u, 999u, 31337u };
#define N_SEEDS ((int)(sizeof k_seeds / sizeof k_seeds[0]))

#define WIN 128   /* a window of columns */
#define GRID 8    /* windows a side, 256 apart: a 2048-block square round the origin */
#define STEP 4    /* columns sampled for the biome areas: every STEP-th on x and z */

/* Is this kind allowed in the trunk column's biome (the rules of trees.h)? A
 * mountain with grass keeps a few spruce and oak whatever its biome. */
static int allowed(int kind, int biome, int terrain_kind)
{
    if (terrain_kind == TERRAIN_MOUNTAIN && (kind == TREE_SPRUCE || kind == TREE_OAK))
        return biome != BIOME_DESERT && biome != BIOME_BADLANDS;
    switch (kind) {
    case TREE_OAK:
        return biome == BIOME_PLAINS || biome == BIOME_FOREST || biome == BIOME_DARK_FOREST ||
               biome == BIOME_SWAMP || biome == BIOME_SAVANNA;
    case TREE_BIRCH:
        return biome == BIOME_FOREST || biome == BIOME_BIRCH_FOREST;
    case TREE_SPRUCE:
        return biome == BIOME_TAIGA || biome == BIOME_SNOWY_TAIGA || biome == BIOME_SNOWY_PLAINS;
    case TREE_DARK_OAK:
        return biome == BIOME_DARK_FOREST;
    case TREE_JUNGLE:
    case TREE_JUNGLE_GIANT:
    case TREE_JUNGLE_BUSH:
        return biome == BIOME_JUNGLE;
    case TREE_ACACIA:
        return biome == BIOME_SAVANNA;
    case TREE_CACTUS:
        return biome == BIOME_DESERT;
    default:
        return 0;
    }
}

static int s_seen[TREE_KIND_COUNT];
static int s_trees[BIOME_COUNT];  /* trees by their trunk's biome */
static int s_cols[BIOME_COUNT];   /* sampled columns by biome */
static int s_bad_kind, s_bad_ground, s_total;

static void scan_window(uint32_t seed, int x0, int z0)
{
    static Tree t[4096];
    int n = trees_in_area(seed, x0, z0, WIN, WIN, t, 4096), i, x, z;

    for (i = 0; i < n; i++) {
        TerrainColumn c = terrain_column(seed, t[i].x, t[i].z);
        int want = t[i].kind == TREE_CACTUS ? BLOCK_SAND : -1;

        s_seen[t[i].kind]++;
        s_trees[c.biome]++;
        s_total++;
        s_bad_kind += t[i].kind <= TREE_NONE || t[i].kind >= TREE_KIND_COUNT || !allowed(t[i].kind, c.biome, c.kind);
        s_bad_ground += c.height < TERRAIN_SEA ||
                        (t[i].kind == TREE_DARK_OAK || t[i].kind == TREE_JUNGLE_GIANT ? t[i].y < c.height || t[i].y > c.height + 1
                                                                                      : t[i].y != c.height) ||
                        (want >= 0 ? c.surface != want : c.surface != BLOCK_GRASS && c.surface != BLOCK_SNOWY_GRASS);
    }
    for (z = z0; z < z0 + WIN; z += STEP)
        for (x = x0; x < x0 + WIN; x += STEP)
            s_cols[terrain_column(seed, x, z).biome]++;
}

static double per_1000(int biome)
{
    return s_cols[biome] ? 1000.0 * s_trees[biome] / (16.0 * s_cols[biome]) : 0.0;
}

static void test_kinds_and_density(void)
{
    int s, gx, gz, k, no_tree_biomes = 0;
    static const int k_order[] = { BIOME_JUNGLE, BIOME_DARK_FOREST, BIOME_FOREST, BIOME_PLAINS, BIOME_SAVANNA,
                                   BIOME_DESERT };

    for (s = 0; s < N_SEEDS; s++)
        for (gz = 0; gz < GRID; gz++)
            for (gx = 0; gx < GRID; gx++)
                scan_window(k_seeds[s], -1024 + gx * 256, -1024 + gz * 256);
    CHECK_EQ_INT(s_bad_kind, 0);   /* every kind only in its biomes (and spruce, oak on grassy mountains) */
    CHECK_EQ_INT(s_bad_ground, 0); /* on grass (cactus: sand), at the ground height, above the sea */
    CHECK_EQ_INT(s_total > 3000, 1);
    for (k = TREE_OAK; k < TREE_KIND_COUNT; k++) {
        if (s_seen[k] == 0)
            printf("     (no tree of kind %d in the scan)\n", k);
        CHECK_EQ_INT(s_seen[k] > 0, 1);
    }
    /* No tree on a biome without trees: beach, river, sea, ice, badlands. */
    for (k = 0; k < BIOME_COUNT; k++)
        if (s_trees[k] > 0 && (k == BIOME_BEACH || k == BIOME_SNOWY_BEACH || k == BIOME_RIVER ||
                               k == BIOME_FROZEN_RIVER || k == BIOME_OCEAN || k == BIOME_WARM_OCEAN ||
                               k == BIOME_COLD_OCEAN || k == BIOME_FROZEN_OCEAN || k == BIOME_BADLANDS))
            no_tree_biomes++;
    CHECK_EQ_INT(no_tree_biomes, 0);
    /* Each biome of the order was sampled enough to tell, and the density
     * (trees to a thousand columns) falls along the order. */
    for (k = 0; k < 6; k++)
        CHECK_EQ_INT(s_cols[k_order[k]] > 400, 1);
    for (k = 0; k + 1 < 6; k++)
        if (s_cols[k_order[k]] > 400 && s_cols[k_order[k + 1]] > 400)
            CHECK_EQ_INT(per_1000(k_order[k]) > per_1000(k_order[k + 1]), 1);
    printf("     (trees per 1000 columns: jungle %.1f, dark forest %.1f, forest %.1f, plains %.1f, savanna %.1f, "
           "desert %.1f; taiga %.1f, swamp %.1f; %d trees)\n",
           per_1000(BIOME_JUNGLE), per_1000(BIOME_DARK_FOREST), per_1000(BIOME_FOREST), per_1000(BIOME_PLAINS),
           per_1000(BIOME_SAVANNA), per_1000(BIOME_DESERT), per_1000(BIOME_TAIGA), per_1000(BIOME_SWAMP), s_total);
}

/* Trees come out of trees_in_area() the same however the area is cut. */
static void test_area_cuts(void)
{
    static Tree whole[2048], left[2048], right[2048];
    uint32_t seed = k_seeds[0];
    int ox = -100, nw, nl, nr, i, bad = 0;

    while (trees_in_area(seed, ox, -90, 200, 180, whole, 2048) <= 20) /* the first woody stretch east */
        ox += 200;
    nw = trees_in_area(seed, ox, -90, 200, 180, whole, 2048);
    nl = trees_in_area(seed, ox, -90, 77, 180, left, 2048);
    nr = trees_in_area(seed, ox + 77, -90, 123, 180, right, 2048);

    CHECK_EQ_INT(nw > 20, 1);
    CHECK_EQ_INT(nl + nr, nw);
    for (i = 0; i < nl; i++)
        bad += left[i].x >= ox + 77;
    for (i = 0; i < nr; i++)
        bad += right[i].x < ox + 77;
    for (i = 0; i < nw; i++) {
        int j, found = 0;

        for (j = 0; j < nl + nr; j++) {
            const Tree *o = j < nl ? &left[j] : &right[j - nl];

            found += o->x == whole[i].x && o->z == whole[i].z && o->kind == whole[i].kind &&
                     o->salt == whole[i].salt && o->y == whole[i].y;
        }
        bad += found != 1;
    }
    CHECK_EQ_INT(bad, 0);
    /* Two trees are never closer than 3 columns on both axes (the rule in trees.h, written out). */
    for (i = 0; i < nw; i++) {
        int j;

        for (j = i + 1; j < nw; j++)
            bad += abs(whole[i].x - whole[j].x) < 3 && abs(whole[i].z - whole[j].z) < 3;
    }
    CHECK_EQ_INT(bad, 0);
}

/* Which plants grow on which ground, from the dice roll up. */
static void test_plant_rules(void)
{
    int roll, biome, grass_plants = 0, bad = 0;

    for (biome = 0; biome < BIOME_COUNT; biome++)
        for (roll = 0; roll < 256; roll++) {
            int on_grass = plant_for(biome, BLOCK_GRASS, (unsigned)roll);
            int on_sand = plant_for(biome, BLOCK_SAND, (unsigned)roll);
            int on_stone = plant_for(biome, BLOCK_STONE, (unsigned)roll);
            int on_snow = plant_for(biome, BLOCK_SNOWY_GRASS, (unsigned)roll);
            int on_dirt = plant_for(biome, BLOCK_DIRT, (unsigned)roll);

            bad += on_stone != BLOCK_AIR || on_snow != BLOCK_AIR || on_dirt != BLOCK_AIR;
            bad += on_grass != BLOCK_AIR && !block_is_plant(on_grass);
            bad += on_grass == BLOCK_DEAD_BUSH;                      /* no dead bush on grass */
            bad += on_sand != BLOCK_AIR && (on_sand != BLOCK_DEAD_BUSH || biome != BIOME_DESERT);
            bad += (biome == BIOME_BEACH || biome == BIOME_RIVER || biome == BIOME_OCEAN || biome == BIOME_SNOWY_PLAINS ||
                    biome == BIOME_SNOWY_TAIGA) && on_grass != BLOCK_AIR;
            grass_plants += on_grass != BLOCK_AIR;
        }
    CHECK_EQ_INT(bad, 0);
    CHECK_EQ_INT(grass_plants > 150, 1);
    /* The flowers and the short plants each exist in their biomes. */
    CHECK_EQ_INT(plant_for(BIOME_PLAINS, BLOCK_GRASS, 0), BLOCK_SHORT_GRASS);
    CHECK_EQ_INT(plant_for(BIOME_PLAINS, BLOCK_GRASS, 27), BLOCK_DANDELION);
    CHECK_EQ_INT(plant_for(BIOME_PLAINS, BLOCK_GRASS, 30), BLOCK_POPPY);
    CHECK_EQ_INT(plant_for(BIOME_PLAINS, BLOCK_GRASS, 32), BLOCK_OXEYE_DAISY);
    CHECK_EQ_INT(plant_for(BIOME_PLAINS, BLOCK_GRASS, 200), BLOCK_AIR);
    CHECK_EQ_INT(plant_for(BIOME_TAIGA, BLOCK_GRASS, 20), BLOCK_FERN);
    CHECK_EQ_INT(plant_for(BIOME_SWAMP, BLOCK_GRASS, 22), BLOCK_BLUE_ORCHID);
    CHECK_EQ_INT(plant_for(BIOME_DESERT, BLOCK_SAND, 2), BLOCK_DEAD_BUSH);
    CHECK_EQ_INT(plant_for(BIOME_DESERT, BLOCK_SAND, 100), BLOCK_AIR);
    CHECK_EQ_INT(plant_for(BIOME_BADLANDS, BLOCK_RED_SAND, 2), BLOCK_DEAD_BUSH);
    CHECK_EQ_INT(plant_for(BIOME_BADLANDS, BLOCK_TERRACOTTA, 2), BLOCK_DEAD_BUSH);
}

void test_trees(void)
{
    test_kinds_and_density();
    test_area_cuts();
    test_plant_rules();
}
