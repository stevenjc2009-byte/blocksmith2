#include "ruins.h"

#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "structure_templates.h"
#include "terrain.h"
#include "terrain_noise.h"
#include "village.h"

/* Planning and building ruins (see ruins.h). Like villages, a ruin is a pure
 * function of the seed and its region. */

#define RUIN_CHANCE 55       /* percent of regions that try to have a ruin */
#define RUIN_TRIES 3
#define BAND_LO RUIN_MARGIN  /* the middle is chosen from this far in ... */
#define BAND_SPAN 128        /* ... over this many blocks */
#define VILLAGE_AWAY 72      /* no ruin this near a village's middle */
#define SPREAD 4             /* most the ground may rise or fall under one */
#define MIN_VISIBLE 5        /* columns with stone showing above the ground */
#define MOSS_LAND 40         /* percent of stones that are mossy */
#define MOSS_SEA 55

int ruin_column_cap(const RuinPlan *p, int lx, int lz)
{
    const RuinShape *s = ruin_shape(p->shape);
    int ax, az, hgt, r, cap;

    struct_unmap(s->w, s->d, p->rot, p->mirror, lx, lz, &ax, &az);
    hgt = ruin_shape_height(s, ax, az);
    if (hgt == 0)
        return 0;
    r = (int)(noise_hash(p->salt, lx, lz) >> 4);
    if (r % 5 == 0)
        return 0; /* this bit fell down */
    cap = hgt - (r >> 8) % (hgt / 2 + 2);
    return cap > 0 ? cap : 0;
}

/* Try one spot (cx, cz); true if a ruin fits and p is filled in. */
static bool try_spot(uint32_t seed, uint32_t base, int t, int cx, int cz, bool has_spawn,
                     int spx, int spz, RuinPlan *p)
{
    TerrainKind centre = terrain_column(seed, cx, cz).kind;
    const RuinShape *s;
    int x, z, W, D, lo = 999, hi = -1, visible = 0;
    const VillagePlan *vp;

    if (centre != TERRAIN_PLAINS && centre != TERRAIN_HILLS && centre != TERRAIN_OCEAN)
        return false;
    memset(p, 0, sizeof *p);
    p->sea = centre == TERRAIN_OCEAN;
    p->shape = (uint8_t)(noise_hash(base, 10, t) % RUIN_SHAPE_COUNT);
    p->rot = (uint8_t)(noise_hash(base, 11, t) % 4);
    p->mirror = (uint8_t)(noise_hash(base, 12, t) & 1);
    p->salt = noise_hash(base, 13, t);
    p->sink = noise_hash(base, 14, t) % 4 == 0 ? 2 : 1;
    s = ruin_shape(p->shape);
    struct_footprint(s->w, s->d, p->rot, &W, &D);
    p->width = W;
    p->depth = D;
    p->x0 = cx - W / 2;
    p->z0 = cz - D / 2;
    if (has_spawn && spx >= p->x0 - 3 && spx < p->x0 + W + 3 && spz >= p->z0 - 3 && spz < p->z0 + D + 3)
        return false;
    for (z = 0; z < D; z++)
        for (x = 0; x < W; x++) {
            TerrainColumn col = terrain_column(seed, p->x0 + x, p->z0 + z);
            bool ok = p->sea ? col.kind == TERRAIN_OCEAN && col.height <= RUIN_SEA_DEEPEST
                             : col.kind == TERRAIN_PLAINS || col.kind == TERRAIN_HILLS;

            if (!ok)
                return false;
            p->gh[z * W + x] = (uint8_t)col.height;
            lo = col.height < lo ? col.height : lo;
            hi = col.height > hi ? col.height : hi;
        }
    if (hi - lo > SPREAD)
        return false;
    for (z = 0; z < D; z++)
        for (x = 0; x < W; x++)
            visible += ruin_column_cap(p, x, z) > p->sink;
    if (visible < MIN_VISIBLE)
        return false;
    vp = village_get(seed, structure_floor_div(p->x0, VILLAGE_REGION_CHUNKS * CHUNK_SIZE),
                     structure_floor_div(p->z0, VILLAGE_REGION_CHUNKS * CHUNK_SIZE));
    if (vp != NULL && abs(vp->cx - cx) < VILLAGE_AWAY && abs(vp->cz - cz) < VILLAGE_AWAY)
        return false;
    p->valid = true;
    return true;
}

bool ruin_plan_spawn(uint32_t seed, int rx, int rz, bool has_spawn, int sx, int sz, RuinPlan *out)
{
    uint32_t base = noise_hash(seed ^ 0x52554E53u, rx, rz);
    int size = RUIN_REGION_CHUNKS * CHUNK_SIZE, t;

    memset(out, 0, sizeof *out);
    if (noise_hash(base, 1, 1) % 100 >= RUIN_CHANCE)
        return false;
    for (t = 0; t < RUIN_TRIES; t++) {
        int cx = rx * size + BAND_LO + (int)(noise_hash(base, 2, t) % BAND_SPAN);
        int cz = rz * size + BAND_LO + (int)(noise_hash(base, 3, t) % BAND_SPAN);

        if (try_spot(seed, base, t, cx, cz, has_spawn, sx, sz, out))
            return true;
    }
    memset(out, 0, sizeof *out);
    return false;
}

bool ruin_plan(uint32_t seed, int rx, int rz, RuinPlan *out)
{
    int sx = 0, sz = 0;
    bool has = structure_spawn(seed, &sx, &sz);

    return ruin_plan_spawn(seed, rx, rz, has, sx, sz, out);
}

/* ---- the cache ---- */

#define CACHE_SIZE 16

static struct {
    bool used;
    uint32_t seed;
    int rx, rz;
    RuinPlan plan;
} s_cache[CACHE_SIZE];
static int s_next;

void ruin_cache_clear(void)
{
    memset(s_cache, 0, sizeof s_cache);
    s_next = 0;
}

const RuinPlan *ruin_get(uint32_t seed, int rx, int rz)
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
    ruin_plan(seed, rx, rz, &s_cache[i].plan);
    return s_cache[i].plan.valid ? &s_cache[i].plan : NULL;
}

void ruin_apply(const StructCanvas *cv, const RuinPlan *p)
{
    int lx, lz, ly;
    int moss = p->sea ? MOSS_SEA : MOSS_LAND;

    if (!p->valid || p->x0 >= cv->x0 + cv->w || p->x0 + p->width <= cv->x0 ||
        p->z0 >= cv->z0 + cv->d || p->z0 + p->depth <= cv->z0)
        return;
    for (lz = 0; lz < p->depth; lz++)
        for (lx = 0; lx < p->width; lx++) {
            int cap, gh = p->gh[lz * p->width + lx];

            if (p->x0 + lx < cv->x0 || p->x0 + lx >= cv->x0 + cv->w ||
                p->z0 + lz < cv->z0 || p->z0 + lz >= cv->z0 + cv->d)
                continue;
            cap = ruin_column_cap(p, lx, lz);
            for (ly = 0; ly < cap; ly++) {
                bool mossy = (int)(noise_hash(p->salt ^ 0x6D6F7373u, lx + 64 * ly, lz) % 100) < moss;
                structure_canvas_set(cv, p->x0 + lx, gh + 1 - p->sink + ly, p->z0 + lz,
                                     mossy ? BLOCK_MOSSY_COBBLESTONE : BLOCK_COBBLESTONE);
            }
        }
}
