#include "village_site.h"

#include "structure.h"
#include "terrain.h"

static uint8_t s_height[SITE_SIZE][SITE_SIZE]; /* [z][x] from the grid's north-west corner */
static uint8_t s_land[SITE_SIZE][SITE_SIZE];
static int s_x0, s_z0;

void site_load(uint32_t seed, int cx, int cz)
{
    TerrainColumn cols[CHUNK_SIZE][CHUNK_SIZE];
    int x0 = cx - SITE_HALF, z0 = cz - SITE_HALF;
    int ccx, ccz, x, z;

    s_x0 = x0;
    s_z0 = z0;
    for (ccz = structure_floor_div(z0, CHUNK_SIZE); ccz <= structure_floor_div(z0 + SITE_SIZE - 1, CHUNK_SIZE); ccz++)
        for (ccx = structure_floor_div(x0, CHUNK_SIZE); ccx <= structure_floor_div(x0 + SITE_SIZE - 1, CHUNK_SIZE); ccx++) {
            terrain_chunk_columns(seed, ccx, ccz, cols);
            for (z = 0; z < CHUNK_SIZE; z++)
                for (x = 0; x < CHUNK_SIZE; x++) {
                    int gx = ccx * CHUNK_SIZE + x - x0, gz = ccz * CHUNK_SIZE + z - z0;
                    if (gx < 0 || gx >= SITE_SIZE || gz < 0 || gz >= SITE_SIZE)
                        continue;
                    s_height[gz][gx] = (uint8_t)cols[z][x].height;
                    s_land[gz][gx] = cols[z][x].kind == TERRAIN_PLAINS || cols[z][x].kind == TERRAIN_HILLS;
                }
        }
}

bool site_land(int x, int z)
{
    int gx = x - s_x0, gz = z - s_z0;

    return gx >= 0 && gx < SITE_SIZE && gz >= 0 && gz < SITE_SIZE && s_land[gz][gx];
}

int site_height(int x, int z)
{
    int gx = x - s_x0, gz = z - s_z0;

    return gx >= 0 && gx < SITE_SIZE && gz >= 0 && gz < SITE_SIZE ? s_height[gz][gx] : -1;
}
