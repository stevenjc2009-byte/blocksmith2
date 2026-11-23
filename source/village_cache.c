#include "village.h"

#include <string.h>

#include "structure.h"
#include "structure_templates.h"

/* The village plan wrapper, the plan cache and the door helper (see
 * village.h), split from village.c to keep each file short. */

bool village_plan(uint32_t seed, int rx, int rz, VillagePlan *out)
{
    int sx = 0, sz = 0;
    bool has = structure_spawn(seed, &sx, &sz);

    return village_plan_spawn(seed, rx, rz, has, sx, sz, out);
}

/* ---- the cache ---- */

#define CACHE_SIZE 6

static struct {
    bool used;
    uint32_t seed;
    int rx, rz;
    VillagePlan plan;
} s_cache[CACHE_SIZE];
static int s_next;

void village_cache_clear(void)
{
    memset(s_cache, 0, sizeof s_cache);
    s_next = 0;
}

const VillagePlan *village_get(uint32_t seed, int rx, int rz)
{
    int i;

    for (i = 0; i < CACHE_SIZE; i++)
        if (s_cache[i].used && s_cache[i].seed == seed && s_cache[i].rx == rx && s_cache[i].rz == rz)
            return s_cache[i].plan.valid ? &s_cache[i].plan : NULL;
    i = s_next;
    s_next = (s_next + 1) % CACHE_SIZE;
    s_cache[i].used = true;
    s_cache[i].seed = seed;
    s_cache[i].rx = rx;
    s_cache[i].rz = rz;
    village_plan(seed, rx, rz, &s_cache[i].plan);
    return s_cache[i].plan.valid ? &s_cache[i].plan : NULL;
}

bool village_door_outside(const VillageBuilding *b, int *x, int *z)
{
    const Template *t = template_get(b->tmpl);
    int lx, lz, fx = 0, fz = 0;

    if (t == NULL || t->door_x < 0)
        return false;
    struct_map(t->w, t->d, b->rot, b->mirror, t->door_x, t->d - 1, &lx, &lz);
    switch (b->rot & 3) {
    case 0: fz = 1; break;
    case 1: fx = -1; break;
    case 2: fz = -1; break;
    default: fx = 1; break;
    }
    *x = b->x0 + lx + fx;
    *z = b->z0 + lz + fz;
    return true;
}
