#include "village.h"

#include <stdlib.h>
#include <string.h>

#include "structure_templates.h"
#include "terrain.h"
#include "terrain_noise.h"
#include "village_internal.h"
#include "village_site.h"

/* Planning a village (see village.h). Everything here is a pure function of
 * the seed and the region: the same plan comes out whenever and however
 * often it is made. Hash numbers come from noise_hash(base, a, b), where
 * base belongs to the region and (a, b) say what the number is for. */

#define VILLAGE_CHANCE 100  /* percent of regions that try to have a village */
#define SITE_TRIES 6        /* places tried in a region */
#define BAND_LO 64          /* the middle is chosen from this far in ... */
#define BAND_SPAN 256       /* ... over this many blocks */
#define MIN_HOUSES 5
#define SPAWN_KEEP 6        /* the middle is never this near the spawn */
/* How far a straight path from (x, z) going (dx, dz) can run on land with
 * no step over one block, up to want columns; the first column is the
 * cell (x, z) itself, which came after a column of height prev. */
static int run_length(const Ctx *c, int x, int z, int dx, int dz, int want, int prev, int ex, int ez)
{
    int i, n = 0;

    for (i = 0; i < want; i++) {
        int px = x + dx * i, pz = z + dz * i, h, k;

        if (!site_land(px, pz) || occ_at(c, px, pz) != OCC_FREE)
            break;
        h = site_height(px, pz);
        if (abs(h - prev) > 1)
            break;
        /* a path must not run alongside another one: no neighbour that is a
         * path except the one behind it (and the one it branches from) */
        for (k = 0; k < 4; k++) {
            static const int d4[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
            int nx = px + d4[k][0], nz = pz + d4[k][1];
            bool behind = (nx == px - dx && nz == pz - dz) || (nx == ex && nz == ez);
            if (!behind && occ_at(c, nx, nz) == OCC_PATH)
                break;
        }
        if (k < 4)
            break;
        prev = h;
        n++;
    }
    return n;
}

static bool lay_paths(Ctx *c)
{
    static const int dir[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    VillagePlan *p = c->p;
    int d, i, s, arms = 0;

    for (d = 0; d < 4; d++) {
        int dx = dir[d][0], dz = dir[d][1], n;
        int want = 12 + (int)(hv(c, 100 + d, 1) % 17);

        if (hv(c, 100 + d, 0) % 100 >= 80)
            continue;
        n = run_length(c, p->cx + dx * 3, p->cz + dz * 3, dx, dz, want,
                       site_height(p->cx + dx * 2, p->cz + dz * 2), p->cx + dx * 2, p->cz + dz * 2);
        if (n < 5)
            continue;
        c->paths[c->npaths].ox = p->cx + dx * 3;
        c->paths[c->npaths].oz = p->cz + dz * 3;
        c->paths[c->npaths].dx = dx;
        c->paths[c->npaths].dz = dz;
        c->paths[c->npaths].n = n;
        c->paths[c->npaths].arm = true;
        for (i = 0; i < n; i++)
            add_path_col(c, p->cx + dx * (3 + i), p->cz + dz * (3 + i));
        c->npaths++;
        arms++;
    }
    if (arms < 2)
        return false;
    for (s = 0; s < 2; s++) { /* up to two spurs */
        int ai = (int)(hv(c, 200 + s, 0) % (unsigned)arms), side = hv(c, 200 + s, 1) & 1 ? 1 : -1;
        const Path *a = &c->paths[ai];
        int j, sx, sz, ox, oz, n, want = 8 + (int)(hv(c, 200 + s, 2) % 7);

        if (a->n < 9)
            continue;
        j = 4 + (int)(hv(c, 200 + s, 3) % (unsigned)(a->n - 8));
        sx = -a->dz * side;
        sz = a->dx * side;
        ox = a->ox + a->dx * j + sx;
        oz = a->oz + a->dz * j + sz;
        n = run_length(c, ox, oz, sx, sz, want, site_height(ox - sx, oz - sz), ox - sx, oz - sz);
        if (n < 4)
            continue;
        c->paths[c->npaths].ox = ox;
        c->paths[c->npaths].oz = oz;
        c->paths[c->npaths].dx = sx;
        c->paths[c->npaths].dz = sz;
        c->paths[c->npaths].n = n;
        c->paths[c->npaths].arm = false;
        for (i = 0; i < n; i++)
            add_path_col(c, ox + sx * i, oz + sz * i);
        c->npaths++;
    }
    return true;
}

/* Try to put template tmpl on lane ln at its cursor. */
static bool try_place(Ctx *c, Lane *ln, int tmpl, int salt)
{
    const Template *t = template_get(tmpl);
    const Path *pa = &c->paths[ln->path];
    int sx = -pa->dz * ln->side, sz = pa->dx * ln->side; /* the way off the path */
    int a0 = ln->cursor, a1 = a0 + t->w - 1;
    int x0, z0, W, D, rot, mirror, fy, x, z, ox, oz;

    if (a1 >= pa->n)
        return false;
    /* the box of the cells a0..a1 along the path, 1..d off it */
    {
        int ax = pa->ox + pa->dx * a0 + sx, az = pa->oz + pa->dz * a0 + sz;
        int bx = pa->ox + pa->dx * a1 + sx * t->d, bz = pa->oz + pa->dz * a1 + sz * t->d;
        x0 = ax < bx ? ax : bx;
        z0 = az < bz ? az : bz;
    }
    rot = struct_rot_facing(-sx, -sz);
    mirror = (int)(hv(c, 400 + salt, 0) & 1);
    struct_footprint(t->w, t->d, rot, &W, &D);
    if (t->door_x >= 0) {
        int lx, lz;
        struct_map(t->w, t->d, rot, mirror, t->door_x, t->d - 1, &lx, &lz);
        ox = x0 + lx - sx;
        oz = z0 + lz - sz;
    } else {
        ox = pa->ox + pa->dx * (a0 + t->w / 2);
        oz = pa->oz + pa->dz * (a0 + t->w / 2);
    }
    if (occ_at(c, ox, oz) != OCC_PATH)
        return false;
    fy = site_height(ox, oz);
    for (z = z0 - 1; z <= z0 + D; z++)
        for (x = x0 - 1; x <= x0 + W; x++) {
            bool in = x >= x0 && x < x0 + W && z >= z0 && z < z0 + D;
            int o = occ_at(c, x, z);

            if (!site_land(x, z))
                return false;
            if (in ? o != OCC_FREE || abs(site_height(x, z) - fy) > VILLAGE_FIT
                   : o == OCC_BUILD || o == OCC_WELL || o == OCC_LAMP)
                return false;
        }
    if (near_spawn(c, x0, z0, W, D, 2))
        return false;
    put_building(c, tmpl, rot, mirror, x0, z0, W, D, fy, OCC_BUILD);
    c->p->nhouses++;
    ln->cursor = a0 + t->w + 1 + (int)(hv(c, 500 + salt, 0) % 3);
    return true;
}

static bool place_buildings(Ctx *c)
{
    static const uint8_t k_pick[12] = { TMPL_HOUSE_SMALL, TMPL_HOUSE_SMALL, TMPL_HOUSE_SMALL,
                                        TMPL_HOUSE_SMALL, TMPL_HOUSE_SMALL, TMPL_HUT, TMPL_HUT,
                                        TMPL_HOUSE_LARGE, TMPL_HOUSE_LARGE, TMPL_FARM, TMPL_FARM,
                                        TMPL_SHED };
    int target = 6 + (int)(hv(c, 300, 0) % 5), attempt, i;

    for (i = 0; i < c->npaths; i++) {
        int side;
        for (side = -1; side <= 1; side += 2) {
            c->lanes[c->nlanes].path = i;
            c->lanes[c->nlanes].side = side;
            c->lanes[c->nlanes].cursor = 1;
            c->nlanes++;
        }
    }
    for (attempt = 0; attempt < 80 && c->p->nhouses < target; attempt++) {
        Lane *ln = &c->lanes[attempt % c->nlanes];
        int tmpl = k_pick[hv(c, 310, attempt) % 12];

        if (!try_place(c, ln, tmpl, attempt) && !try_place(c, ln, TMPL_HUT, attempt))
            ln->cursor += 2;
    }
    return c->p->nhouses >= MIN_HOUSES;
}

/* Plan the village with its well on column (cx, cz); false if the place
 * will not do. */
static bool plan_site(Ctx *c, int cx, int cz)
{
    VillagePlan *p = c->p;
    int x, z, lo = 999, hi = -1;

    memset(p, 0, sizeof *p);
    occ_clear();
    p->cx = cx;
    p->cz = cz;
    p->style = village_style(c->seed, cx, cz);
    if (c->has_spawn && abs(c->spx - cx) <= SPAWN_KEEP && abs(c->spz - cz) <= SPAWN_KEEP)
        return false;
    site_load(c->seed, cx, cz);
    for (z = -3; z <= 3; z++)
        for (x = -3; x <= 3; x++) {
            int h;
            if (!site_land(cx + x, cz + z))
                return false;
            h = site_height(cx + x, cz + z);
            lo = h < lo ? h : lo;
            hi = h > hi ? h : hi;
        }
    if (hi - lo > 3)
        return false;
    c->npaths = 0;
    c->nlanes = 0;
    /* the plaza and the well */
    for (z = -2; z <= 2; z++)
        for (x = -2; x <= 2; x++)
            if (abs(x) == 2 || abs(z) == 2)
                add_path_col(c, cx + x, cz + z);
    put_building(c, TMPL_WELL, 0, 0, cx - 1, cz - 1, 3, 3, site_height(cx, cz), OCC_WELL);
    if (!lay_paths(c) || !place_buildings(c))
        return false;
    place_lamps(c);
    p->x0 = p->x1 = cx;
    p->z0 = p->z1 = cz;
    for (x = 0; x < p->npath; x++) {
        p->x0 = p->path[x].x < p->x0 ? p->path[x].x : p->x0;
        p->x1 = p->path[x].x > p->x1 ? p->path[x].x : p->x1;
        p->z0 = p->path[x].z < p->z0 ? p->path[x].z : p->z0;
        p->z1 = p->path[x].z > p->z1 ? p->path[x].z : p->z1;
    }
    for (x = 0; x < p->nbuild; x++) {
        const VillageBuilding *b = &p->b[x];
        p->x0 = b->x0 < p->x0 ? b->x0 : p->x0;
        p->x1 = b->x0 + b->width - 1 > p->x1 ? b->x0 + b->width - 1 : p->x1;
        p->z0 = b->z0 < p->z0 ? b->z0 : p->z0;
        p->z1 = b->z0 + b->depth - 1 > p->z1 ? b->z0 + b->depth - 1 : p->z1;
    }
    return p->x0 >= cx - VILLAGE_RADIUS && p->x1 <= cx + VILLAGE_RADIUS &&
           p->z0 >= cz - VILLAGE_RADIUS && p->z1 <= cz + VILLAGE_RADIUS;
}

bool village_plan_spawn(uint32_t seed, int rx, int rz, bool has_spawn, int sx, int sz,
                        VillagePlan *out)
{
    static const int k_probe[5][2] = { { 0, 0 }, { 14, 0 }, { -14, 0 }, { 0, 14 }, { 0, -14 } };
    Ctx c;
    int t, i, size = VILLAGE_REGION_CHUNKS * CHUNK_SIZE;

    memset(out, 0, sizeof *out);
    c.seed = seed;
    c.base = noise_hash(seed ^ 0x56494C4Cu, rx, rz);
    c.has_spawn = has_spawn;
    c.spx = sx;
    c.spz = sz;
    c.p = out;
    if (hv(&c, 1, 1) % 100 >= VILLAGE_CHANCE)
        return false;
    for (t = 0; t < SITE_TRIES; t++) {
        int cx = rx * size + BAND_LO + (int)(hv(&c, 2, t) % BAND_SPAN);
        int cz = rz * size + BAND_LO + (int)(hv(&c, 3, t) % BAND_SPAN);
        bool land = true;

        /* a quick look before the whole grid: dry land at the middle and 14
         * round it, and a village biome at the middle (row 4-02) */
        for (i = 0; i < 5 && land; i++) {
            TerrainColumn col = terrain_column(seed, cx + k_probe[i][0], cz + k_probe[i][1]);
            land = (col.kind == TERRAIN_PLAINS || col.kind == TERRAIN_HILLS) &&
                   (i > 0 || village_biome(col.biome));
        }
        if (land && plan_site(&c, cx, cz)) {
            out->valid = true;
            return true;
        }
    }
    memset(out, 0, sizeof *out);
    return false;
}
