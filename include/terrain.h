#ifndef TERRAIN_H
#define TERRAIN_H

#include <stdbool.h>
#include <stdint.h>

#include "chunk.h"

/* The shape of the land (roadmap row 4-01): plains, hills, mountains,
 * rivers and oceans. Pure C with whole numbers only, so the PC tests and the
 * 3DS make exactly the same ground from the same seed.
 *
 * Every column is solid from y 0 up to its height, with nothing hanging
 * over. Seas and rivers are still water up to TERRAIN_SEA (flowing water is
 * row 4-04).
 *
 * Next-door columns (on x or on z) differ by at most one block everywhere
 * except in the mountains, so the player can walk anywhere else by stepping
 * up one block. Mountains have real cliffs: dig or build steps to climb them.
 *
 * Each column, from the top: its surface block (grass, sand or stone),
 * TERRAIN_DIRT blocks of what goes under it (dirt under grass, sand under
 * sand, stone under stone), then stone down to y 0. */

#define TERRAIN_SEA 62   /* the top water block of seas and rivers */
#define TERRAIN_FLOOR 40 /* lowest possible ground (the deepest sea floor) */
#define TERRAIN_PEAK 118 /* highest possible ground, so trees still fit under the world top */
#define TERRAIN_DIRT 3

typedef enum {
    TERRAIN_OCEAN,    /* sea floor under water, about 40 to 61: sand */
    TERRAIN_BEACH,    /* sand by the sea, from sea level to 3 blocks above */
    TERRAIN_RIVER,    /* river bed or bank at or under the water: sand */
    TERRAIN_PLAINS,   /* gentle grass land, about 63 to 69 */
    TERRAIN_HILLS,    /* rolling grass hills, up to about 80 */
    TERRAIN_MOUNTAIN, /* mountains up to TERRAIN_PEAK, with cliffs */
    TERRAIN_KIND_COUNT
} TerrainKind;

typedef struct {
    int height;       /* y of the top ground block */
    int surface;      /* BLOCK_* of that block: grass, sand or stone */
    TerrainKind kind;
} TerrainColumn;

/* World column (x, z). Coordinates up to about a million blocks from the
 * origin either way. */
TerrainColumn terrain_column(uint32_t seed, int x, int z);

/* y of the top ground block at world column (x, z). */
int terrain_height(uint32_t seed, int x, int z);

/* Every column of chunk (cx, cz) at once, out[z][x] for local x and z: the
 * same as terrain_column() for each, but much quicker. */
void terrain_chunk_columns(uint32_t seed, int cx, int cz,
                           TerrainColumn out[CHUNK_SIZE][CHUNK_SIZE]);

/* Fill chunk (cx, cz), which holds world blocks cx * CHUNK_SIZE onwards on
 * x and cz * CHUNK_SIZE onwards on z. Above the ground: water up to
 * TERRAIN_SEA, then air. */
void terrain_fill_chunk(Chunk *c, uint32_t seed, int cx, int cz);

/* A place to start: the nearest dry column to (0, 0) (above the sea, not
 * in the mountains, with no mountain column round it), searching outward
 * square by square. True and its position in *x, *z; false and (0, 0) if
 * there is none within TERRAIN_SPAWN_REACH blocks either way. */
#define TERRAIN_SPAWN_REACH 4096
bool terrain_find_spawn(uint32_t seed, int *x, int *z);

#endif
