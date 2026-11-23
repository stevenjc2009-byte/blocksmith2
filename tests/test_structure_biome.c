#include <stdio.h>

#include "biome.h"
#include "block.h"
#include "chunk.h"
#include "structure.h"
#include "structure_templates.h"
#include "terrain.h"
#include "test.h"
#include "village.h"
#include "worldgen.h"

/* Row 4-05 on top of the biomes (row 4-02): a village stands only where its
 * middle column is a Minecraft 1.17 village biome (plains, savanna, desert,
 * taiga, snowy plains), and a desert village is built of sandstone. */

static const uint32_t k_seeds[] = { 2026u, 1u, 77u, 12345u, 999u, 31337u, 5u, 42u };
#define N_SEEDS ((int)(sizeof k_seeds / sizeof k_seeds[0]))

static void check_tables(void)
{
    static const StructPart k_built[] = { PART_WALL, PART_FRAME, PART_FLOOR, PART_ROOF, PART_FOUND };
    int b, i, n = 0;

    for (b = 0; b < BIOME_COUNT; b++)
        n += village_biome(b);
    CHECK_EQ_INT(n, 5);
    CHECK_EQ_INT(village_biome(BIOME_PLAINS) && village_biome(BIOME_SAVANNA) &&
                 village_biome(BIOME_DESERT) && village_biome(BIOME_TAIGA) &&
                 village_biome(BIOME_SNOWY_PLAINS), 1);
    for (i = 0; i < (int)(sizeof k_built / sizeof k_built[0]); i++)
        CHECK_EQ_INT(style_block(STYLE_DESERT, k_built[i]), BLOCK_SANDSTONE);
    CHECK_EQ_INT(style_block(STYLE_DESERT, PART_AIR), BLOCK_AIR);
    CHECK_EQ_INT(style_block(STYLE_DESERT, PART_SOIL), BLOCK_DIRT);
    CHECK_EQ_INT(style_block(STYLE_DESERT, PART_WATER), BLOCK_WATER);
    CHECK_EQ_INT(style_block(STYLE_DESERT, PART_TORCH), BLOCK_TORCH);
    CHECK_EQ_INT(style_block(STYLE_PLAINS, PART_ROOF), BLOCK_COBBLESTONE);
    CHECK_EQ_INT(style_path_block(STYLE_PLAINS), BLOCK_DIRT);
    CHECK_EQ_INT(style_path_block(STYLE_DESERT), BLOCK_SANDSTONE);
}

/* Every village in regions -4 .. 3 of each seed: a village biome at its
 * middle and the style that biome asks for. Gives back the first desert
 * village found. */
static bool check_plans(uint32_t *desert_seed, VillagePlan *desert)
{
    int s, rx, rz, n = 0, bad = 0, deserts = 0, per[BIOME_COUNT] = { 0 };

    for (s = 0; s < N_SEEDS; s++)
        for (rz = -4; rz < 4; rz++)
            for (rx = -4; rx < 4; rx++) {
                VillagePlan p;
                Biome b;

                if (!village_plan(k_seeds[s], rx, rz, &p))
                    continue;
                b = terrain_biome(k_seeds[s], p.cx, p.cz);
                n++;
                per[b]++;
                bad += !village_biome(b);
                bad += p.style != (b == BIOME_DESERT ? STYLE_DESERT : STYLE_PLAINS);
                if (p.style == STYLE_DESERT && deserts++ == 0) {
                    *desert_seed = k_seeds[s];
                    *desert = p;
                }
            }
    printf("     (village biomes: %d villages; plains %d, savanna %d, desert %d, taiga %d, snowy plains %d)\n",
           n, per[BIOME_PLAINS], per[BIOME_SAVANNA], per[BIOME_DESERT], per[BIOME_TAIGA],
           per[BIOME_SNOWY_PLAINS]);
    CHECK_EQ_INT(bad, 0);
    CHECK_EQ_INT(n >= 40, 1);
    CHECK_EQ_INT(deserts >= 1, 1);
    return deserts > 0;
}

/* The desert village as the game makes it (world_gen_chunk): every path
 * column sandstone, its buildings sandstone with no oak or cobblestone. */
static void check_desert_world(uint32_t seed, const VillagePlan *p)
{
    static Chunk c;
    int cx, cz, i, k, x, y, z, bad_path = 0, sand = 0, other = 0, paths = 0;

    test_check_village(seed, p); /* every check a plains village gets */
    for (cz = structure_floor_div(p->z0, CHUNK_SIZE); cz <= structure_floor_div(p->z1, CHUNK_SIZE); cz++)
        for (cx = structure_floor_div(p->x0, CHUNK_SIZE); cx <= structure_floor_div(p->x1, CHUNK_SIZE); cx++) {
            world_gen_chunk(&c, seed, cx, cz);
            for (k = 0; k < p->npath; k++)
                if (structure_floor_div(p->path[k].x, CHUNK_SIZE) == cx &&
                    structure_floor_div(p->path[k].z, CHUNK_SIZE) == cz) {
                    paths++;
                    bad_path += chunk_get(&c, p->path[k].x - cx * CHUNK_SIZE, p->path[k].y,
                                          p->path[k].z - cz * CHUNK_SIZE) != BLOCK_SANDSTONE;
                }
            for (i = 0; i < p->nbuild; i++) {
                const VillageBuilding *b = &p->b[i];
                int top = b->fy + template_get(b->tmpl)->h;

                for (z = b->z0; z < b->z0 + b->depth; z++)
                    for (x = b->x0; x < b->x0 + b->width; x++) {
                        if (structure_floor_div(x, CHUNK_SIZE) != cx || structure_floor_div(z, CHUNK_SIZE) != cz)
                            continue;
                        for (y = b->fy; y < top && y < WORLD_HEIGHT; y++) {
                            int v = chunk_get(&c, x - cx * CHUNK_SIZE, y, z - cz * CHUNK_SIZE);
                            sand += v == BLOCK_SANDSTONE;
                            other += v == BLOCK_PLANKS || v == BLOCK_LOG || v == BLOCK_COBBLESTONE;
                        }
                    }
            }
        }
    printf("     (desert village at %d, %d: %d path columns, %d sandstone blocks in its buildings)\n",
           p->cx, p->cz, paths, sand);
    CHECK_EQ_INT(paths, p->npath);
    CHECK_EQ_INT(bad_path, 0);
    CHECK_EQ_INT(sand >= 50, 1);
    CHECK_EQ_INT(other, 0);
}

void test_structure_biome(void)
{
    uint32_t seed = 0;
    VillagePlan desert;

    check_tables();
    if (check_plans(&seed, &desert))
        check_desert_world(seed, &desert);
}
