#include "structure.h"

#include <stddef.h>

#include "ruins.h"
#include "terrain.h"
#include "village.h"

/* The shared half of villages and ruins: canvases, going through the regions
 * a canvas touches, and the spawn the structures keep clear of. See
 * structure.h. */

int structure_floor_div(int a, int b)
{
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

void structure_canvas_set(const StructCanvas *cv, int x, int y, int z, int block)
{
    int lx = x - cv->x0, lz = z - cv->z0;

    if (lx < 0 || lx >= cv->w || lz < 0 || lz >= cv->d || y < 0 || y >= WORLD_HEIGHT)
        return;
    cv->cells[((size_t)y * cv->d + lz) * cv->w + lx] = (uint8_t)block;
}

int structure_canvas_get(const StructCanvas *cv, int x, int y, int z)
{
    int lx = x - cv->x0, lz = z - cv->z0;

    if (lx < 0 || lx >= cv->w || lz < 0 || lz >= cv->d || y < 0 || y >= WORLD_HEIGHT)
        return 0;
    return cv->cells[((size_t)y * cv->d + lz) * cv->w + lx];
}

/* ---- the spawn, worked out once per seed ---- */

#define SPAWN_MEMO 4

static struct {
    uint32_t seed;
    int x, z;
    bool found, used;
} s_spawn[SPAWN_MEMO];
static int s_spawn_next;

bool structure_spawn(uint32_t seed, int *x, int *z)
{
    int i;

    for (i = 0; i < SPAWN_MEMO; i++)
        if (s_spawn[i].used && s_spawn[i].seed == seed)
            break;
    if (i == SPAWN_MEMO) {
        i = s_spawn_next;
        s_spawn_next = (s_spawn_next + 1) % SPAWN_MEMO;
        s_spawn[i].used = true;
        s_spawn[i].seed = seed;
        s_spawn[i].found = terrain_find_spawn(seed, &s_spawn[i].x, &s_spawn[i].z);
    }
    *x = s_spawn[i].x;
    *z = s_spawn[i].z;
    return s_spawn[i].found;
}

/* ---- applying ---- */

void structure_cache_clear(void)
{
    village_cache_clear();
    ruin_cache_clear();
}

void structure_apply_canvas(const StructCanvas *cv, uint32_t seed)
{
    const int vsize = VILLAGE_REGION_CHUNKS * CHUNK_SIZE, rsize = RUIN_REGION_CHUNKS * CHUNK_SIZE;
    int rx, rz;

    for (rz = structure_floor_div(cv->z0, vsize); rz <= structure_floor_div(cv->z0 + cv->d - 1, vsize); rz++)
        for (rx = structure_floor_div(cv->x0, vsize); rx <= structure_floor_div(cv->x0 + cv->w - 1, vsize); rx++) {
            const VillagePlan *p = village_get(seed, rx, rz);
            if (p != NULL)
                village_apply(cv, p);
        }
    for (rz = structure_floor_div(cv->z0, rsize); rz <= structure_floor_div(cv->z0 + cv->d - 1, rsize); rz++)
        for (rx = structure_floor_div(cv->x0, rsize); rx <= structure_floor_div(cv->x0 + cv->w - 1, rsize); rx++) {
            const RuinPlan *p = ruin_get(seed, rx, rz);
            if (p != NULL)
                ruin_apply(cv, p);
        }
}

void structure_apply(Chunk *c, uint32_t seed, int cx, int cz)
{
    StructCanvas cv;

    cv.cells = &c->blocks[0][0][0];
    cv.x0 = cx * CHUNK_SIZE;
    cv.z0 = cz * CHUNK_SIZE;
    cv.w = CHUNK_SIZE;
    cv.d = CHUNK_SIZE;
    structure_apply_canvas(&cv, seed);
}
