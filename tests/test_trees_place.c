#include <stdlib.h>
#include <string.h>

#include "biome.h"
#include "block.h"
#include "ruins.h"
#include "structure.h"
#include "terrain.h"
#include "test.h"
#include "trees.h"
#include "village.h"
#include "worldgen.h"

/* Row 4-03: how trees and plants are written: the same in any chunk order
 * and equal to a slow reference, only ever into air (or the ground under a
 * trunk), standing on the ground, and never in a village, a ruin or on the
 * spawn. Which kinds grow where is in test_trees.c. */

static const uint32_t k_seeds[] = { 2026u, 1u, 77u, 12345u, 999u, 31337u };
#define N_SEEDS ((int)(sizeof k_seeds / sizeof k_seeds[0]))

typedef struct {
    uint8_t *cells;
    StructCanvas cv;
} Area;

static int fdiv(int a, int b) { return structure_floor_div(a, b); }

static int is_tree_block(int b) { return b == BLOCK_LOG || (b >= BLOCK_BIRCH_LOG && b < BLOCK_COUNT); }
static int is_log(int b) { return b == BLOCK_LOG || (b >= BLOCK_BIRCH_LOG && b <= BLOCK_ACACIA_LOG) || b == BLOCK_CACTUS; }

/* A box of nx x nz chunks from chunk (cx0, cz0): bare terrain, or what
 * world_gen_chunk() makes (trees, plants, structures). Free with area_free(). */
static void area_make(Area *a, uint32_t seed, int cx0, int cz0, int nx, int nz, bool generated)
{
    int w = nx * CHUNK_SIZE, d = nz * CHUNK_SIZE, cx, cz, y, z;
    static Chunk c;

    a->cells = malloc((size_t)w * d * WORLD_HEIGHT);
    a->cv.cells = a->cells;
    a->cv.x0 = cx0 * CHUNK_SIZE;
    a->cv.z0 = cz0 * CHUNK_SIZE;
    a->cv.w = w;
    a->cv.d = d;
    for (cz = 0; cz < nz; cz++)
        for (cx = 0; cx < nx; cx++) {
            if (generated)
                world_gen_chunk(&c, seed, cx0 + cx, cz0 + cz);
            else
                terrain_fill_chunk(&c, seed, cx0 + cx, cz0 + cz);
            for (y = 0; y < WORLD_HEIGHT; y++)
                for (z = 0; z < CHUNK_SIZE; z++)
                    memcpy(&a->cells[((size_t)y * d + cz * CHUNK_SIZE + z) * w + cx * CHUNK_SIZE], c.blocks[y][z],
                           CHUNK_SIZE);
        }
}

static void area_free(Area *a) { free(a->cells); }
static int at(const Area *a, int x, int y, int z) { return structure_canvas_get(&a->cv, x, y, z); }

/* The first 5 x 5 chunk box (from the origin out) whose middle column is
 * `biome` and that holds at least min_trees trees. */
static bool find_window(uint32_t seed, int biome, int min_trees, int *cx0, int *cz0)
{
    static Tree t[1024];
    int cx, cz;

    for (cz = -60; cz <= 60; cz += 5)
        for (cx = -60; cx <= 60; cx += 5) {
            if (terrain_column(seed, cx * CHUNK_SIZE + 8, cz * CHUNK_SIZE + 8).biome != biome)
                continue;
            if (trees_in_area(seed, (cx - 2) * CHUNK_SIZE, (cz - 2) * CHUNK_SIZE, 80, 80, t, 1024) < min_trees)
                continue;
            *cx0 = cx - 2;
            *cz0 = cz - 2;
            return true;
        }
    return false;
}

static const struct { int biome, min_trees; } k_windows[] = {
    { BIOME_JUNGLE, 12 },      { BIOME_DARK_FOREST, 8 }, { BIOME_FOREST, 8 }, { BIOME_BIRCH_FOREST, 8 },
    { BIOME_TAIGA, 8 },        { BIOME_SAVANNA, 1 },     { BIOME_DESERT, 0 }, { BIOME_PLAINS, 1 },
    { BIOME_SWAMP, 0 },        { BIOME_BADLANDS, 0 },    { BIOME_SNOWY_TAIGA, 4 },
};
#define N_WINDOWS ((int)(sizeof k_windows / sizeof k_windows[0]))

/* A slow reference: every tree of the box and its reach, tree_shape() pass by
 * pass, logs and leaves into air only; plants left out. */
typedef struct {
    const StructCanvas *cv;
    int pass;
} RefWriter;

static void ref_put(void *ctx, int x, int y, int z, int block)
{
    const RefWriter *w = ctx;

    if (w->pass == 0 || structure_canvas_get(w->cv, x, y, z) == BLOCK_AIR)
        structure_canvas_set(w->cv, x, y, z, block);
}

static int s_windows, s_trees, s_crossing, s_plants_seen[BLOCK_COUNT];

static void check_window(uint32_t seed, int cx0, int cz0)
{
    static Tree t[2048];
    Area bare, got, ref;
    RefWriter w;
    int n, i, x, y, z, order, cx, cz, bad_plant = 0, bad_write = 0, bad_trunk = 0, changed = 0, plants_here = 0, diff = 0, nx = 5, nz = 5;
    int x0 = cx0 * CHUNK_SIZE, z0 = cz0 * CHUNK_SIZE;
    static Chunk c;

    area_make(&bare, seed, cx0, cz0, nx, nz, false);
    area_make(&got, seed, cx0, cz0, nx, nz, false);
    area_make(&ref, seed, cx0, cz0, nx, nz, false);
    trees_apply_canvas(&got.cv, seed);
    n = trees_in_area(seed, x0 - TREES_REACH, z0 - TREES_REACH, 80 + 2 * TREES_REACH, 80 + 2 * TREES_REACH, t, 2048);
    w.cv = &ref.cv;
    for (w.pass = 0; w.pass < 3; w.pass++)
        for (i = 0; i < n; i++)
            tree_shape(&t[i], w.pass, ref_put, &w);
    s_trees += n;
    for (i = 0; i < n; i++) { /* a trunk near a chunk's edge: its canopy crosses it */
        int lx = ((t[i].x % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE, lz = ((t[i].z % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;

        s_crossing += (lx < 3 || lx > 12 || lz < 3 || lz > 12) && t[i].x >= x0 && t[i].x < x0 + 80 &&
                      t[i].z >= z0 && t[i].z < z0 + 80;
    }

    for (y = 0; y < WORLD_HEIGHT; y++)
        for (z = z0; z < z0 + 80; z++)
            for (x = x0; x < x0 + 80; x++) {
                int g = at(&got, x, y, z), r = at(&ref, x, y, z), b = at(&bare, x, y, z);

                if (block_is_plant(g)) { /* a plant stands in air, on the ground, with air over it */
                    int below = at(&got, x, y - 1, z), dead = g == BLOCK_DEAD_BUSH;

                    s_plants_seen[g]++;
                    plants_here++;
                    bad_plant += r != BLOCK_AIR || at(&got, x, y + 1, z) != BLOCK_AIR;
                    bad_plant += dead ? !(below == BLOCK_SAND || below == BLOCK_RED_SAND ||
                                    (below >= BLOCK_TERRACOTTA && below <= BLOCK_TERRACOTTA_BROWN))
                                : below != BLOCK_GRASS;
                    continue;
                }
                diff += g != r; /* the slow reference agrees block for block */
                /* Only air takes a log or leaf (or the dirt filling a 2 x 2
                 * trunk's lower columns), and only grass becomes dirt. */
                bad_write += !(g == b || (b == BLOCK_AIR && (is_tree_block(g) || g == BLOCK_DIRT)) ||
                         ((b == BLOCK_GRASS || b == BLOCK_SNOWY_GRASS) && g == BLOCK_DIRT));
                changed += g != b;
                if (is_log(g) && x > x0 && x < x0 + 79 && z > z0 && z < z0 + 79) { /* a trunk is held up: ground or log under it,
                                                                                      * or the acacia's bend beside it (not at the box's edge) */
                    int u = at(&got, x, y - 1, z);
                    int side = is_log(at(&got, x + 1, y, z)) || is_log(at(&got, x - 1, y, z)) ||
                               is_log(at(&got, x, y, z + 1)) || is_log(at(&got, x, y, z - 1));

                    bad_trunk += !(is_log(u) || u == BLOCK_DIRT || u == BLOCK_SAND || side);
                    if (g == BLOCK_CACTUS)
                        bad_trunk += !(is_log(u) || u == BLOCK_SAND);
                }
            }
    CHECK_EQ_INT(diff, 0);
    CHECK_EQ_INT(bad_plant, 0);
    CHECK_EQ_INT(bad_write, 0);
    CHECK_EQ_INT(bad_trunk, 0);
    s_windows += changed > 0 || plants_here > 0;

    /* Chunk by chunk, in two orders, is the whole box (with the structures). */
    structure_cache_clear();
    structure_apply_canvas(&got.cv, seed);
    for (order = 0; order < 2; order++) {
        structure_cache_clear();
        for (i = 0; i < nx * nz; i++) {
            int k = order ? nx * nz - 1 - i : i;

            cx = k % nx;
            cz = k / nx;
            if (i == nx * nz / 2)
                structure_cache_clear();
            world_gen_chunk(&c, seed, cx0 + cx, cz0 + cz);
            for (y = 0; y < WORLD_HEIGHT; y++)
                for (z = 0; z < CHUNK_SIZE; z++)
                    for (x = 0; x < CHUNK_SIZE; x++)
                        diff += c.blocks[y][z][x] != at(&got, x0 + cx * CHUNK_SIZE + x, y, z0 + cz * CHUNK_SIZE + z);
        }
    }
    CHECK_EQ_INT(diff, 0);
    area_free(&bare);
    area_free(&got);
    area_free(&ref);
}

static void test_windows(void)
{
    int s, k, found = 0, p;

    for (s = 0; s < N_SEEDS; s++)
        for (k = 0; k < N_WINDOWS; k++) {
            int cx0, cz0;

            if (find_window(k_seeds[s], k_windows[k].biome, k_windows[k].min_trees, &cx0, &cz0)) {
                check_window(k_seeds[s], cx0, cz0);
                found++;
            }
        }
    CHECK_EQ_INT(found >= 30, 1);
    CHECK_EQ_INT(s_windows, found);       /* every box got trees or plants */
    CHECK_EQ_INT(s_trees > 200, 1);
    CHECK_EQ_INT(s_crossing > 40, 1);     /* canopies did cross chunk borders */
    for (p = BLOCK_SHORT_GRASS; p <= BLOCK_DEAD_BUSH; p++) {
        if (s_plants_seen[p] == 0)
            printf("     (no plant of block %d in the boxes)\n", p);
        CHECK_EQ_INT(s_plants_seen[p] > 0, 1);
    }
    printf("     (%d boxes of 5 x 5 chunks, %d trees, %d near a chunk edge; short grass %d, fern %d, dandelion %d, "
           "poppy %d, daisy %d, orchid %d, dead bush %d)\n",
           found, s_trees, s_crossing, s_plants_seen[BLOCK_SHORT_GRASS], s_plants_seen[BLOCK_FERN],
           s_plants_seen[BLOCK_DANDELION], s_plants_seen[BLOCK_POPPY], s_plants_seen[BLOCK_OXEYE_DAISY],
           s_plants_seen[BLOCK_BLUE_ORCHID], s_plants_seen[BLOCK_DEAD_BUSH]);
}

/* No tree block or leaf in a village's box or a ruin's footprint, no plant
 * within a column of it (so none on a path), nothing on the spawn column. */
static int box_trees(const Area *a, int x0, int z0, int x1, int z1, int grow, int plants_only)
{
    int x, y, z, n = 0;

    for (y = 0; y < WORLD_HEIGHT; y++)
        for (z = z0 - grow; z <= z1 + grow; z++)
            for (x = x0 - grow; x <= x1 + grow; x++) {
                int b = at(a, x, y, z);

                n += plants_only ? block_is_plant(b) : (b >= BLOCK_BIRCH_LOG && b < BLOCK_COUNT);
            }
    return n;
}

static int s_villages, s_ruins, s_trees_beside;

static void check_box(uint32_t seed, int x0, int z0, int x1, int z1)
{
    int cx0 = fdiv(x0 - 8, CHUNK_SIZE), cz0 = fdiv(z0 - 8, CHUNK_SIZE);
    int cx1 = fdiv(x1 + 8, CHUNK_SIZE), cz1 = fdiv(z1 + 8, CHUNK_SIZE);
    Area a;

    area_make(&a, seed, cx0, cz0, cx1 - cx0 + 1, cz1 - cz0 + 1, true);
    CHECK_EQ_INT(box_trees(&a, x0, z0, x1, z1, 0, 0), 0);
    CHECK_EQ_INT(box_trees(&a, x0, z0, x1, z1, 1, 1), 0);
    /* The ring round it is not empty of trees and plants: the rule is not
     * "no trees anywhere near" by accident of a bare map. */
    s_trees_beside += box_trees(&a, x0, z0, x1, z1, 8, 0) > 0;
    area_free(&a);
}

static void test_structures(void)
{
    int s, rx, rz, sx, sz, bad = 0;

    for (s = 0; s < N_SEEDS; s++) {
        uint32_t seed = k_seeds[s];

        for (rz = -2; rz < 2; rz++)
            for (rx = -2; rx < 2; rx++) {
                const VillagePlan *v = village_get(seed, rx, rz);

                if (v != NULL && v->valid) {
                    check_box(seed, v->x0, v->z0, v->x1, v->z1);
                    s_villages++;
                }
            }
        for (rz = -4; rz < 4; rz++)
            for (rx = -4; rx < 4; rx++) {
                const RuinPlan *r = ruin_get(seed, rx, rz);

                if (r != NULL) {
                    check_box(seed, r->x0, r->z0, r->x0 + r->width - 1, r->z0 + r->depth - 1);
                    s_ruins++;
                }
            }
        if (structure_spawn(seed, &sx, &sz)) { /* nothing grows on or beside the spawn column */
            Area a;

            area_make(&a, seed, fdiv(sx - 2, CHUNK_SIZE), fdiv(sz - 2, CHUNK_SIZE), 2, 2, true);
            bad += box_trees(&a, sx, sz, sx, sz, 1, 0) + box_trees(&a, sx, sz, sx, sz, 1, 1);
            area_free(&a);
        }
    }
    CHECK_EQ_INT(bad, 0);
    CHECK_EQ_INT(s_villages >= 8, 1);
    CHECK_EQ_INT(s_ruins >= 8, 1);
    CHECK_EQ_INT(s_trees_beside >= (s_villages + s_ruins) / 3, 1);
    printf("     (%d villages and %d ruins checked; %d of them have trees within 8 blocks)\n", s_villages, s_ruins,
           s_trees_beside);
}

void test_trees_place(void)
{
    test_windows();
    test_structures();
}
