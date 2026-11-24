#include "biome.h"
#include "block.h"
#include "terrain_noise.h"
#include "trees_internal.h"

/* Short grass, ferns, flowers and dead bushes (row 4-03, see trees.h): one
 * dice roll per ground column decides. */

int plant_for(int biome, int ground, unsigned roll)
{
    if (ground == BLOCK_SAND && biome == BIOME_DESERT)
        return roll < 5 ? BLOCK_DEAD_BUSH : BLOCK_AIR;
    if (biome == BIOME_BADLANDS && (ground == BLOCK_RED_SAND || (ground >= BLOCK_TERRACOTTA && ground <= BLOCK_TERRACOTTA_BROWN)))
        return roll < 8 ? BLOCK_DEAD_BUSH : BLOCK_AIR;
    if (ground != BLOCK_GRASS)
        return BLOCK_AIR;
    switch (biome) {
    case BIOME_PLAINS:
        return roll < 26 ? BLOCK_SHORT_GRASS : roll < 29 ? BLOCK_DANDELION : roll < 31 ? BLOCK_POPPY
             : roll < 33 ? BLOCK_OXEYE_DAISY : BLOCK_AIR;
    case BIOME_FOREST:
        return roll < 18 ? BLOCK_SHORT_GRASS : roll < 20 ? BLOCK_DANDELION : roll < 22 ? BLOCK_POPPY : BLOCK_AIR;
    case BIOME_BIRCH_FOREST:
        return roll < 16 ? BLOCK_SHORT_GRASS : roll < 18 ? BLOCK_OXEYE_DAISY : roll < 19 ? BLOCK_DANDELION : BLOCK_AIR;
    case BIOME_DARK_FOREST:
        return roll < 10 ? BLOCK_SHORT_GRASS : roll < 16 ? BLOCK_FERN : roll < 17 ? BLOCK_POPPY : BLOCK_AIR;
    case BIOME_TAIGA:
        return roll < 8 ? BLOCK_SHORT_GRASS : roll < 30 ? BLOCK_FERN : BLOCK_AIR;
    case BIOME_SWAMP:
        return roll < 20 ? BLOCK_SHORT_GRASS : roll < 24 ? BLOCK_BLUE_ORCHID : BLOCK_AIR;
    case BIOME_SAVANNA:
        return roll < 36 ? BLOCK_SHORT_GRASS : BLOCK_AIR;
    case BIOME_JUNGLE:
        return roll < 10 ? BLOCK_SHORT_GRASS : roll < 24 ? BLOCK_FERN : BLOCK_AIR;
    default:
        return BLOCK_AIR;
    }
}

void trees_plants(const StructCanvas *cv, const TreeCols *s)
{
    int x, z;

    for (z = cv->z0; z < cv->z0 + cv->d; z++)
        for (x = cv->x0; x < cv->x0 + cv->w; x++) {
            TerrainColumn c = tree_col(s, x, z);
            unsigned roll = (noise_hash(s->seed ^ 0xA1A1u, x, z) >> 8) & 255;
            int plant;

            if (roll >= 36 || c.height < TERRAIN_SEA)
                continue; /* 36 is the most any biome gives a plant */
            plant = plant_for(c.biome, structure_canvas_get(cv, x, c.height, z), roll);
            if (plant == BLOCK_AIR || structure_canvas_get(cv, x, c.height + 1, z) != BLOCK_AIR ||
                structure_canvas_get(cv, x, c.height + 2, z) != BLOCK_AIR ||
                trees_blocked(s->seed, x, z, 1))
                continue;
            structure_canvas_set(cv, x, c.height + 1, z, plant);
        }
}
