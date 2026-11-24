#include "trees.h"

#include <stdlib.h>

#include "biome.h"
#include "block.h"
#include "ruins.h"
#include "terrain_noise.h"
#include "trees_internal.h"
#include "village.h"

/* Where the trees are (see trees.h): the candidate grid, the biome rules,
 * the suppression of crowded candidates, and writing a canvas. */

TerrainColumn tree_col(const TreeCols *s, int x, int z)
{
    int lx = x - s->x0, lz = z - s->z0;

    if (s->cols != NULL && lx >= 0 && lx < CHUNK_SIZE && lz >= 0 && lz < CHUNK_SIZE)
        return s->cols[lz * CHUNK_SIZE + lx];
    return terrain_column(s->seed, x, z);
}

bool trees_blocked(uint32_t seed, int x, int z, int grow)
{
    const int vsize = VILLAGE_REGION_CHUNKS * CHUNK_SIZE, rsize = RUIN_REGION_CHUNKS * CHUNK_SIZE;
    const VillagePlan *v;
    const RuinPlan *r;
    int sx, sz;

    if (structure_spawn(seed, &sx, &sz) && abs(x - sx) <= grow && abs(z - sz) <= grow)
        return true;
    v = village_get(seed, structure_floor_div(x, vsize), structure_floor_div(z, vsize));
    if (v != NULL && x >= v->x0 - grow && x <= v->x1 + grow && z >= v->z0 - grow && z <= v->z1 + grow)
        return true;
    r = ruin_get(seed, structure_floor_div(x, rsize), structure_floor_div(z, rsize));
    return r != NULL && x >= r->x0 - grow && x < r->x0 + r->width + grow && z >= r->z0 - grow &&
           z < r->z0 + r->depth + grow;
}

/* ---- the biome rules ---- */

/* How many of 256 candidates (one per TREES_CELL x TREES_CELL columns, so 16
 * a chunk) are trees, on average, before the grove map and the suppression
 * of crowded ones. */
static int density(int biome)
{
    switch (biome) {
    case BIOME_PLAINS:       return 16;
    case BIOME_FOREST:       return 55;
    case BIOME_BIRCH_FOREST: return 55;
    case BIOME_DARK_FOREST:  return 60;
    case BIOME_TAIGA:        return 55;
    case BIOME_SNOWY_TAIGA:  return 40;
    case BIOME_SNOWY_PLAINS: return 3;
    case BIOME_SWAMP:        return 24;
    case BIOME_SAVANNA:      return 10;
    case BIOME_JUNGLE:       return 75;
    case BIOME_DESERT:       return 6;
    default:                 return 0;
    }
}

/* The tree kind of a biome for a dice roll 0 to 255. */
static int pick_kind(int biome, unsigned r)
{
    switch (biome) {
    case BIOME_FOREST:       return r < 150 ? TREE_OAK : TREE_BIRCH;
    case BIOME_BIRCH_FOREST: return TREE_BIRCH;
    case BIOME_DARK_FOREST:  return r < 190 ? TREE_DARK_OAK : TREE_OAK;
    case BIOME_TAIGA:
    case BIOME_SNOWY_TAIGA:
    case BIOME_SNOWY_PLAINS: return TREE_SPRUCE;
    case BIOME_SAVANNA:      return r < 218 ? TREE_ACACIA : TREE_OAK;
    case BIOME_JUNGLE:       return r < 50 ? TREE_JUNGLE_GIANT : r < 150 ? TREE_JUNGLE : TREE_JUNGLE_BUSH;
    case BIOME_DESERT:       return TREE_CACTUS;
    default:                 return TREE_OAK; /* plains, swamp */
    }
}

/* The slow grove map: groves and clearings about 60 blocks across. It
 * multiplies a density by 0.5 to 1.5 (128 to 384 of 256). */
static const NoiseLayer k_grove = { 41, 2, 1, 7 };

static int grove(uint32_t seed, int x, int z)
{
    return 128 + (int)((int64_t)noise_value(seed, &k_grove, x, z) * 256 / NOISE_ONE);
}

static bool tree_ground(int surface)
{
    return surface == BLOCK_GRASS || surface == BLOCK_SNOWY_GRASS;
}

/* The tree of candidate cell (gx, gz) (before crowding is looked at); kind
 * TREE_NONE if there is none. */
static void candidate(const TreeCols *s, int gx, int gz, Tree *t)
{
    uint32_t h = noise_hash(s->seed ^ 0x7231u, gx, gz);
    int x = gx * TREES_CELL + (int)(h & 3), z = gz * TREES_CELL + (int)((h >> 2) & 3);
    unsigned roll = (h >> 4) & 255, kind_roll = (h >> 12) & 255;
    TerrainColumn c;
    int d, kind, i;

    t->kind = TREE_NONE;
    c = tree_col(s, x, z);
    d = density(c.biome);
    if (d == 0 || c.height < TERRAIN_SEA)
        return;
    kind = pick_kind(c.biome, kind_roll);
    if (c.kind == TERRAIN_MOUNTAIN) { /* grass high on a mountain: a few spruce and oak */
        d /= 4;
        kind = kind_roll & 1 ? TREE_SPRUCE : TREE_OAK;
    }
    if (kind == TREE_CACTUS ? c.surface != BLOCK_SAND : !tree_ground(c.surface))
        return;
    if ((int)roll >= d * grove(s->seed, x, z) / 256)
        return;
    t->x = x;
    t->z = z;
    t->y = c.height;
    t->salt = noise_hash(s->seed ^ 0x5a5au, gx, gz);
    t->prio = noise_hash(s->seed ^ 0x9e37u, gx, gz);
    for (i = 0; i < 4; i++)
        t->gy[i] = c.height;
    if (kind == TREE_DARK_OAK || kind == TREE_JUNGLE_GIANT) { /* the other three trunk columns */
        int lo = c.height, hi = c.height;

        for (i = 1; i < 4; i++) {
            TerrainColumn o = tree_col(s, x + (i & 1), z + (i >> 1));

            if (!tree_ground(o.surface) || o.height < TERRAIN_SEA)
                return;
            t->gy[i] = o.height;
            lo = o.height < lo ? o.height : lo;
            hi = o.height > hi ? o.height : hi;
        }
        if (hi - lo > 1)
            return;
        t->y = hi;
    } else if (kind == TREE_CACTUS) { /* nothing solid beside it */
        static const signed char k_n[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

        for (i = 0; i < 4; i++)
            if (tree_col(s, x + k_n[i][0], z + k_n[i][1]).height > c.height)
                return;
    } else if (kind == TREE_ACACIA) { /* the lean's column must be free of ground up to the bend */
        int dx, dz;

        tree_acacia_lean(t, &dx, &dz);
        if (tree_col(s, x + dx, z + dz).height > c.height + tree_acacia_straight(t) - 1)
            return;
    }
    if (trees_blocked(s->seed, x, z, TREES_REACH))
        return;
    t->kind = kind;
}

/* Does candidate b take a's place (they are too close together)? */
static bool beats(const Tree *b, int bi, const Tree *a, int ai)
{
    return b->prio > a->prio || (b->prio == a->prio && bi > ai);
}

/* Every tree with a trunk column in [xlo, xhi] x [zlo, zhi], cell by cell,
 * row by row, into out (at most max; the rest are left out). */
static int collect(const TreeCols *s, int xlo, int zlo, int xhi, int zhi, Tree *out, int max)
{
    int gx0 = structure_floor_div(xlo, TREES_CELL) - 1, gz0 = structure_floor_div(zlo, TREES_CELL) - 1;
    int gx1 = structure_floor_div(xhi, TREES_CELL) + 1, gz1 = structure_floor_div(zhi, TREES_CELL) + 1;
    int nx = gx1 - gx0 + 1, nz = gz1 - gz0 + 1, gx, gz, n = 0;
    Tree *cand = malloc((size_t)nx * nz * sizeof *cand);

    if (cand == NULL)
        return 0;
    for (gz = 0; gz < nz; gz++)
        for (gx = 0; gx < nx; gx++)
            candidate(s, gx0 + gx, gz0 + gz, &cand[gz * nx + gx]);
    for (gz = 1; gz < nz - 1; gz++)
        for (gx = 1; gx < nx - 1; gx++) {
            const Tree *a = &cand[gz * nx + gx];
            bool alive = a->kind != TREE_NONE && a->x >= xlo && a->x <= xhi && a->z >= zlo && a->z <= zhi;
            int i, j;

            for (j = -1; j <= 1 && alive; j++)
                for (i = -1; i <= 1 && alive; i++) {
                    const Tree *b = &cand[(gz + j) * nx + gx + i];

                    if ((i || j) && b->kind != TREE_NONE && abs(b->x - a->x) < TREES_MIN_DIST &&
                        abs(b->z - a->z) < TREES_MIN_DIST && beats(b, (gz + j) * nx + gx + i, a, gz * nx + gx))
                        alive = false;
                }
            if (alive && n < max)
                out[n++] = *a;
        }
    free(cand);
    return n;
}

int trees_in_area(uint32_t seed, int x0, int z0, int w, int d, Tree *out, int max)
{
    TreeCols s = { seed, NULL, 0, 0 };

    return collect(&s, x0, z0, x0 + w - 1, z0 + d - 1, out, max);
}

/* ---- writing ---- */

typedef struct {
    const StructCanvas *cv;
    int pass;
} Writer;

static void write_block(void *ctx, int x, int y, int z, int block)
{
    const Writer *w = ctx;

    if (w->pass == 0 || structure_canvas_get(w->cv, x, y, z) == BLOCK_AIR)
        structure_canvas_set(w->cv, x, y, z, block);
}

static void apply(const StructCanvas *cv, const TreeCols *s)
{
    int gw = (cv->w + 2 * TREES_REACH) / TREES_CELL + 3, gd = (cv->d + 2 * TREES_REACH) / TREES_CELL + 3;
    int max = gw * gd, n, i;
    Tree *t = malloc((size_t)max * sizeof *t);
    Writer w;

    if (t == NULL)
        return;
    n = collect(s, cv->x0 - TREES_REACH, cv->z0 - TREES_REACH, cv->x0 + cv->w - 1 + TREES_REACH,
                cv->z0 + cv->d - 1 + TREES_REACH, t, max);
    w.cv = cv;
    for (w.pass = 0; w.pass < 3; w.pass++)
        for (i = 0; i < n; i++)
            tree_shape(&t[i], w.pass, write_block, &w);
    free(t);
    trees_plants(cv, s);
}

void trees_apply_canvas(const StructCanvas *cv, uint32_t seed)
{
    TreeCols s = { seed, NULL, 0, 0 };

    apply(cv, &s);
}

void trees_apply(Chunk *c, uint32_t seed, int cx, int cz, const TerrainColumn cols[CHUNK_SIZE][CHUNK_SIZE])
{
    StructCanvas cv;
    TreeCols s;

    cv.cells = &c->blocks[0][0][0];
    cv.x0 = s.x0 = cx * CHUNK_SIZE;
    cv.z0 = s.z0 = cz * CHUNK_SIZE;
    cv.w = cv.d = CHUNK_SIZE;
    s.seed = seed;
    s.cols = &cols[0][0];
    apply(&cv, &s);
}
