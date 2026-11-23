#include "village_internal.h"

#include <string.h>

#include "structure_templates.h"
#include "terrain.h"
#include "terrain_noise.h"

/* The site grid of what is built where, and the small helpers the village
 * planner uses on it (see village_internal.h). */

static uint8_t s_occ[SITE_SIZE][SITE_SIZE]; /* what is on each column of the site grid */

int occ_at(const Ctx *c, int x, int z)
{
    int gx = x - (c->p->cx - SITE_HALF), gz = z - (c->p->cz - SITE_HALF);

    if (gx < 0 || gx >= SITE_SIZE || gz < 0 || gz >= SITE_SIZE)
        return OCC_OUT;
    return s_occ[gz][gx];
}

void occ_set(const Ctx *c, int x, int z, int v)
{
    int gx = x - (c->p->cx - SITE_HALF), gz = z - (c->p->cz - SITE_HALF);

    if (gx >= 0 && gx < SITE_SIZE && gz >= 0 && gz < SITE_SIZE)
        s_occ[gz][gx] = (uint8_t)v;
}

uint32_t hv(const Ctx *c, int a, int b)
{
    return noise_hash(c->base, a, b);
}

void add_path_col(Ctx *c, int x, int z)
{
    VillagePlan *p = c->p;

    if (p->npath < VILLAGE_MAX_PATH) {
        p->path[p->npath].x = x;
        p->path[p->npath].z = z;
        p->path[p->npath].y = (uint8_t)site_height(x, z);
        p->npath++;
    }
    occ_set(c, x, z, OCC_PATH);
}

/* Record a building (footprint x0, z0, W x D) and mark its columns. */
void put_building(Ctx *c, int tmpl, int rot, int mirror, int x0, int z0, int W, int D,
                         int fy, int mark)
{
    VillagePlan *p = c->p;
    VillageBuilding *b = &p->b[p->nbuild++];
    int x, z, top = fy + template_get(tmpl)->h - 1;

    b->tmpl = (uint8_t)tmpl;
    b->rot = (uint8_t)rot;
    b->mirror = (uint8_t)mirror;
    b->x0 = x0;
    b->z0 = z0;
    b->width = W;
    b->depth = D;
    b->fy = fy;
    for (z = 0; z < D; z++)
        for (x = 0; x < W; x++) {
            int gh = site_height(x0 + x, z0 + z);
            b->gh[z * W + x] = (uint8_t)gh;
            if (gh > top)
                top = gh;
            occ_set(c, x0 + x, z0 + z, mark);
        }
    b->clear_top = top < WORLD_HEIGHT - 1 ? top : WORLD_HEIGHT - 1;
}

bool near_spawn(const Ctx *c, int x0, int z0, int W, int D, int margin)
{
    return c->has_spawn && c->spx >= x0 - margin && c->spx < x0 + W + margin &&
           c->spz >= z0 - margin && c->spz < z0 + D + margin;
}

/* Lamp posts: one by each arm and one at the end of each spur, on a free
 * column by the path with nothing built next to it. */
void place_lamps(Ctx *c)
{
    int i, k, s;

    for (i = 0; i < c->npaths; i++) {
        const Path *pa = &c->paths[i];
        int start = pa->arm ? (int)(hv(c, 600 + i, 0) % (unsigned)pa->n) : pa->n - 1;
        bool done = false;

        for (k = 0; k < pa->n && !done; k++) {
            int idx = pa->arm ? (start + k) % pa->n : pa->n - 1 - k;
            for (s = 0; s < 2 && !done; s++) {
                int side = (hv(c, 610 + i, 0) & 1) ? 1 - 2 * s : 2 * s - 1;
                int x = pa->ox + pa->dx * idx - pa->dz * side;
                int z = pa->oz + pa->dz * idx + pa->dx * side;
                int dx, dz, clear = 1;

                if (!site_land(x, z) || occ_at(c, x, z) != OCC_FREE || near_spawn(c, x, z, 1, 1, 2))
                    continue;
                for (dz = -1; dz <= 1; dz++)
                    for (dx = -1; dx <= 1; dx++) {
                        int o = occ_at(c, x + dx, z + dz);
                        if (o == OCC_BUILD || o == OCC_WELL || o == OCC_LAMP)
                            clear = 0;
                    }
                if (clear && c->p->nbuild < VILLAGE_MAX_BUILD) {
                    put_building(c, TMPL_LAMP, 0, 0, x, z, 1, 1, site_height(x, z), OCC_LAMP);
                    done = true;
                }
            }
        }
    }
}

void occ_clear(void)
{
    memset(s_occ, 0, sizeof s_occ);
}
