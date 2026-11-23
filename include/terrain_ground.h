#ifndef TERRAIN_GROUND_H
#define TERRAIN_GROUND_H

#include <stdbool.h>

#include "terrain.h"

/* What terrain.c knows about a column when the biome decides its blocks. */
typedef struct {
    int temp, humid; /* the climate (biome.h) */
    bool inland;     /* not under the sea's coast slope: a river, not an ocean, if wet */
    bool cliff;      /* a mountain's cliff face */
    bool flat;       /* plains that are low and level: a swamp where it is wet */
} GroundIn;

#define TERRAIN_STONE_TOP 100 /* bare stone on mountains from here up */

/* Choose c->biome, surface, under and depth. c->height and c->kind are
 * already set. */
void terrain_ground(TerrainColumn *c, const GroundIn *g);

#endif
