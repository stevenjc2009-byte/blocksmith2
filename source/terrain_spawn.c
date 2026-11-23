#include "terrain.h"

#include <stdlib.h>

/* Spots tried, every SPAWN_STEP blocks on x and z. */
#define SPAWN_STEP 8

/* Dry, not in the mountains, nothing but walkable ground round it. */
static bool good_spawn(uint32_t seed, int x, int z)
{
    TerrainColumn c = terrain_column(seed, x, z);
    int dx, dz;

    if (c.kind == TERRAIN_MOUNTAIN || c.height <= TERRAIN_SEA)
        return false;
    for (dz = -1; dz <= 1; dz++)
        for (dx = -1; dx <= 1; dx++) {
            TerrainColumn n = terrain_column(seed, x + dx, z + dz);

            if (n.kind == TERRAIN_MOUNTAIN || abs(n.height - c.height) > 1)
                return false;
        }
    return true;
}

bool terrain_find_spawn(uint32_t seed, int *x, int *z)
{
    int r, i;

    /* Square ring r: 8r spots, starting at the north-west corner and going
     * round east, south, west, north. Ring 0 is (0, 0) itself. */
    for (r = 0; r * SPAWN_STEP <= TERRAIN_SPAWN_REACH; r++)
        for (i = 0; i < (r == 0 ? 1 : 8 * r); i++) {
            int side = r == 0 ? 0 : i / (2 * r), k = r == 0 ? 0 : i % (2 * r);
            int px = side == 0 ? -r + k : side == 1 ? r : side == 2 ? r - k : -r;
            int pz = side == 0 ? -r : side == 1 ? -r + k : side == 2 ? r : r - k;

            if (good_spawn(seed, px * SPAWN_STEP, pz * SPAWN_STEP)) {
                *x = px * SPAWN_STEP;
                *z = pz * SPAWN_STEP;
                return true;
            }
        }
    *x = 0;
    *z = 0;
    return false;
}
