#ifndef BIOME_H
#define BIOME_H

#include <stdbool.h>
#include <stdint.h>

/* Biomes (roadmap row 4-02): the climate of each place and what it does to
 * the ground's blocks, colours and weather. Pure C with whole numbers only,
 * so the PC tests and the 3DS agree.
 *
 * Two slow maps decide the climate: temperature and humidity, both 0 to
 * NOISE_ONE - 1, a few hundred blocks across. Temperature falls in five
 * bands (BIOME_TEMP_*), and they only ever sit next to the band above or
 * below, so a hot biome never touches a cold one: snowy, cool, temperate,
 * warm, hot. Land biomes come from the band and the humidity (dry to wet);
 * the water's edges and the low wet swamps are added by terrain.c, which
 * knows what the ground does there. Biomes never change the ground's
 * height: only its blocks, colours and weather. */

typedef enum {
    BIOME_PLAINS,        /* grass, grass-green; the biome of a chunk nobody has filled */
    BIOME_FOREST,        /* temperate grass, a little darker; woods later (row 4-03) */
    BIOME_BIRCH_FOREST,  /* light grass, pale-trunked woods later */
    BIOME_DARK_FOREST,   /* wet temperate: dark grass, thick woods later */
    BIOME_TAIGA,         /* cool and wet: bluish grass, spruce woods later */
    BIOME_SNOWY_PLAINS,  /* snow-covered grass, no trees */
    BIOME_SNOWY_TAIGA,   /* snow-covered grass, spruce woods later */
    BIOME_SWAMP,         /* low, flat, wet land near sea level: dull dark grass, murky water */
    BIOME_SAVANNA,       /* warm and dry: yellowish grass, dry, scattered trees later */
    BIOME_JUNGLE,       /* warm and wet: bright grass, thick trees later */
    BIOME_DESERT,        /* hot and dry: sand over sandstone, no plants, no rain */
    BIOME_BADLANDS,      /* hot: red sand and banded terracotta, no rain */
    BIOME_OCEAN,         /* sea floor under water: sand */
    BIOME_WARM_OCEAN,    /* hot seas: sand, turquoise water */
    BIOME_COLD_OCEAN,    /* cool seas: gravel floor, darker water */
    BIOME_FROZEN_OCEAN,  /* snowy seas: gravel floor, ice on the surface */
    BIOME_BEACH,         /* sand by the sea */
    BIOME_SNOWY_BEACH,   /* snow over sand by a cold sea */
    BIOME_RIVER,         /* river bed and banks: sand */
    BIOME_FROZEN_RIVER,  /* river in a snowy place: ice on the surface */
    BIOME_COUNT
} Biome;

/* The temperature bands, coldest to hottest. */
enum { BIOME_TEMP_SNOWY, BIOME_TEMP_COOL, BIOME_TEMP_TEMPERATE, BIOME_TEMP_WARM, BIOME_TEMP_HOT,
       BIOME_TEMP_BANDS };

/* A band starts at these temperatures (multiples of BIOME_UNIT, so
 * weather.h can work in 0 to 255). */
#define BIOME_UNIT 256
#define BIOME_T_COOL (BIOME_UNIT * 44)
#define BIOME_T_TEMPERATE (BIOME_UNIT * 90)
#define BIOME_T_WARM (BIOME_UNIT * 148)
#define BIOME_T_HOT (BIOME_UNIT * 197)

/* The slow maps are worked out at grid points every BIOME_CELL blocks and
 * blended in between (terrain.c does the same for its own layers), so a
 * chunk costs 25 points, not 256. */
#define BIOME_CELL 4

/* Temperature and humidity at the grid point (x, z), no blending. */
void biome_climate_point(uint32_t seed, int x, int z, int *temp, int *humid);

/* The same blended in between four grid points: g00 the west-north one, g10
 * east, g01 south, g11 south-east, each {temp, humid}; fx, fz blocks (0 to
 * BIOME_CELL - 1) into the cell. */
void biome_climate_blend(const int g00[2], const int g10[2], const int g01[2], const int g11[2],
                         int fx, int fz, int out[2]);

/* Temperature and humidity at world column (x, z): what terrain_column()
 * uses (its blend of the four grid points round it). */
void biome_climate(uint32_t seed, int x, int z, int *temp, int *humid);

/* BIOME_TEMP_* of a temperature. */
int biome_temp_band(int temp);

/* The land biome of a climate (never water, beach or swamp). */
Biome biome_land(int temp, int humid);

/* Is this climate wet enough, and warm enough, for a swamp where the ground
 * is low and flat (terrain.c decides that)? */
bool biome_swamp_climate(int temp, int humid);

/* The water biomes of a temperature (ocean for a column under the sea, river
 * for one in a river's bed, beach for the shore). */
Biome biome_ocean(int temp);
Biome biome_river(int temp);
Biome biome_beach(int temp, Biome land);

/* Does it never rain in this biome (desert, savanna, badlands)? Does snow
 * fall in it (the snowy biomes and their water)? Is it water with ice on top? */
bool biome_dry(Biome b);
bool biome_snowy(Biome b);
bool biome_frozen(Biome b);

/* The mountain snow line at this temperature: snow covers the mountain from
 * this y up (low in the cold, high in the warm; above the highest peak in
 * hot places, so none). */
int biome_snow_line(int temp);

/* A short lower-case name ("snowy_taiga"); "?" for a value out of range. */
const char *biome_name(Biome b);

/* How a biome tints grass seen from above: the red, green and blue the
 * grass tile's colours are multiplied by (each 0 to 1). The same for
 * leaves, and the colour of its water, 0 to 1 each (row 4-03 and the
 * water mesh use these; a value out of range gives plains). */
void biome_grass_rgb(int biome, float rgb[3]);
void biome_foliage_rgb(int biome, float rgb[3]);
void biome_water_rgb(int biome, float rgb[3]);

#endif
