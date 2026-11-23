#include "terrain_ground.h"

#include "block.h"

/* The terracotta bands of the badlands, by height (row 4-02): O the plain
 * orange-brown, W white, Y yellow, R red, B brown. Thin and thick layers,
 * repeating every 32 blocks, so a cliff shows the same stripes all along. */
static const char k_bands[33] = "OOOWOOYYOORRBOOWWOOOYOOORROOWOBO";

int terrain_band_block(int y)
{
    switch (k_bands[((y % 32) + 32) % 32]) {
    case 'W': return BLOCK_TERRACOTTA_WHITE;
    case 'Y': return BLOCK_TERRACOTTA_YELLOW;
    case 'R': return BLOCK_TERRACOTTA_RED;
    case 'B': return BLOCK_TERRACOTTA_BROWN;
    default:  return BLOCK_TERRACOTTA;
    }
}

static void set(TerrainColumn *c, Biome b, int surface, int under, int depth)
{
    c->biome = (uint8_t)b;
    c->surface = (uint8_t)surface;
    c->under = (uint8_t)under;
    c->depth = (uint8_t)depth;
}

/* The floor of a sea or river bed: gravel in cold seas, sand elsewhere. */
static void water_floor(TerrainColumn *c, Biome b)
{
    if (b == BIOME_COLD_OCEAN || b == BIOME_FROZEN_OCEAN)
        set(c, b, BLOCK_GRAVEL, BLOCK_GRAVEL, TERRAIN_DIRT);
    else
        set(c, b, BLOCK_SAND, BLOCK_SAND, TERRAIN_DIRT);
}

/* Ground above the water for a land (or shore) biome. `flat`: plains-kind
 * ground, where badlands are red sand, not bare bands. */
static void land_ground(TerrainColumn *c, Biome b, bool flat)
{
    switch (b) {
    case BIOME_SNOWY_PLAINS:
    case BIOME_SNOWY_TAIGA:
        set(c, b, BLOCK_SNOWY_GRASS, BLOCK_DIRT, TERRAIN_DIRT);
        break;
    case BIOME_DESERT:
        set(c, b, BLOCK_SAND, BLOCK_SANDSTONE, TERRAIN_DIRT);
        break;
    case BIOME_BADLANDS:
        set(c, b, flat ? BLOCK_RED_SAND : terrain_band_block(c->height), BLOCK_TERRACOTTA,
            TERRAIN_BADLANDS_DEPTH);
        break;
    default:
        set(c, b, BLOCK_GRASS, BLOCK_DIRT, TERRAIN_DIRT);
        break;
    }
}

/* The shore: snow over sand in the cold, the desert's and badlands' own
 * sand, otherwise plain sand. */
static void shore_ground(TerrainColumn *c, Biome b)
{
    if (b == BIOME_SNOWY_BEACH)
        set(c, b, BLOCK_SNOW, BLOCK_SAND, TERRAIN_DIRT);
    else if (b == BIOME_DESERT || b == BIOME_BADLANDS)
        land_ground(c, b, true);
    else
        set(c, b, BLOCK_SAND, BLOCK_SAND, TERRAIN_DIRT);
}

void terrain_ground(TerrainColumn *c, const GroundIn *g)
{
    Biome land = biome_land(g->temp, g->humid);

    if (c->kind == TERRAIN_MOUNTAIN) {
        if (c->height <= TERRAIN_SEA) { /* a river or sea at its foot */
            water_floor(c, g->inland ? biome_river(g->temp) : biome_ocean(g->temp));
            return;
        }
        land_ground(c, land, false);
        if (g->cliff || c->height >= TERRAIN_STONE_TOP) {
            if (land == BIOME_BADLANDS) {
                c->surface = (uint8_t)terrain_band_block(c->height);
            } else {
                c->surface = BLOCK_STONE;
                c->under = BLOCK_STONE;
            }
        }
        if (c->height >= biome_snow_line(g->temp)) {
            c->surface = BLOCK_SNOW;
            if (c->under == BLOCK_SAND || c->under == BLOCK_SANDSTONE)
                c->under = BLOCK_STONE; /* snow on rock, not on desert */
        }
        return;
    }
    switch (c->kind) {
    case TERRAIN_OCEAN:
        water_floor(c, biome_ocean(g->temp));
        break;
    case TERRAIN_RIVER: /* in a swamp the river is the swamp's: murky water */
        water_floor(c, g->flat && biome_swamp_climate(g->temp, g->humid) ? BIOME_SWAMP
                                                                         : biome_river(g->temp));
        break;
    case TERRAIN_BEACH:
        shore_ground(c, biome_beach(g->temp, land));
        break;
    default:
        if (g->flat && c->kind == TERRAIN_PLAINS && biome_swamp_climate(g->temp, g->humid))
            land = BIOME_SWAMP;
        land_ground(c, land, c->kind == TERRAIN_PLAINS);
        break;
    }
}
