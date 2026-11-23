#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "chunk_mesh.h"
#include "terrain.h"
#include "test.h"
#include "world.h"

/* Row 1-08: breaking a block, and rebuilding only the chunk meshes it
 * changes. */

static World s_w;

static void test_break_rules(void)
{
    uint32_t seed = test_land_seed();
    int h;

    test_world_small(&s_w, seed);
    h = terrain_height(seed, 0, 0);
    CHECK_EQ_INT(world_break(&s_w, 0, h, 0), 1);            /* the grass on top */
    CHECK_EQ_INT(world_get(&s_w, 0, h, 0), BLOCK_AIR);
    CHECK_EQ_INT(world_get(&s_w, 0, h - 1, 0), BLOCK_DIRT); /* nothing else */
    CHECK_EQ_INT(world_get(&s_w, 1, h, 0), BLOCK_GRASS);
    CHECK_EQ_INT(world_break(&s_w, 0, h, 0), 0);            /* air again: no */
    CHECK_EQ_INT(world_break(&s_w, 0, h + 5, 0), 0);
    CHECK_EQ_INT(world_break(&s_w, 0, 1, 0), 1);            /* deep stone: yes */
    CHECK_EQ_INT(world_get(&s_w, 0, 1, 0), BLOCK_AIR);
    CHECK_EQ_INT(world_break(&s_w, 0, 0, 0), 0);            /* the world's floor stays */
    CHECK_EQ_INT(world_get(&s_w, 0, 0, 0), BLOCK_STONE);
    CHECK_EQ_INT(world_break(&s_w, 5, 0, -7), 0);
    CHECK_EQ_INT(world_get(&s_w, 5, 0, -7), BLOCK_STONE);
    CHECK_EQ_INT(world_break(&s_w, 0, -1, 0), 0);
    CHECK_EQ_INT(world_break(&s_w, 0, WORLD_HEIGHT, 0), 0);
    CHECK_EQ_INT(world_break(&s_w, 32, 10, 0), 0);           /* outside the world */
    CHECK_EQ_INT(world_break(&s_w, -33, 10, 0), 0);
    CHECK_EQ_INT(world_break(&s_w, 0, 10, 32), 0);
    /* The edge blocks inside the world break. */
    CHECK_EQ_INT(world_break(&s_w, -32, 10, -32), 1);
    CHECK_EQ_INT(world_break(&s_w, 31, 10, 31), 1);
}

/* Expected list, in order: own chunk, then across x, then across z, then
 * across the corner. */
static void check_touched(int x, int z, int n, const int want[][2])
{
    int out[WORLD_TOUCHED_MAX][2];
    int got, k;

    memset(out, 0x7F, sizeof out);
    got = world_touched_chunks(&s_w, x, z, out);
    CHECK_EQ_INT(got, n);
    for (k = 0; k < n && k < got; k++) {
        CHECK_EQ_INT(out[k][0], want[k][0]);
        CHECK_EQ_INT(out[k][1], want[k][1]);
    }
}

static void test_touched_lists(void)
{
    static const int mid[][2] = { { 0, 0 } };
    static const int west[][2] = { { 0, 0 }, { -1, 0 } };
    static const int east[][2] = { { 0, 0 }, { 1, 0 } };
    static const int north[][2] = { { 0, 0 }, { 0, -1 } };
    static const int south[][2] = { { 0, 0 }, { 0, 1 } };
    static const int nw[][2] = { { 0, 0 }, { -1, 0 }, { 0, -1 }, { -1, -1 } };
    static const int se[][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };
    static const int neg[][2] = { { -1, -1 }, { 0, -1 }, { -1, 0 }, { 0, 0 } };
    static const int ne[][2] = { { 0, 0 }, { 1, 0 }, { 0, -1 }, { 1, -1 } };
    static const int sw[][2] = { { -1, 0 }, { -2, 0 }, { -1, 1 }, { -2, 1 } };
    static const int edge_e_nw[][2] = { { 1, 0 }, { 0, 0 }, { 1, -1 }, { 0, -1 } };
    static const int edge_w[][2] = { { -2, 0 } };
    static const int edge_ne[][2] = { { 1, -2 } };
    static const int corner_nw[][2] = { { -2, -2 } };
    static const int corner_se[][2] = { { 1, 1 } };
    static const int edge_s[][2] = { { -2, 1 }, { -1, 1 } };

    check_touched(5, 5, 1, mid);
    check_touched(0, 5, 2, west);
    check_touched(15, 5, 2, east);
    check_touched(5, 0, 2, north);
    check_touched(5, 15, 2, south);
    check_touched(0, 0, 4, nw);
    check_touched(15, 15, 4, se);
    check_touched(-1, -1, 4, neg);        /* chunk -1, local 15: east and south */
    check_touched(15, 0, 4, ne);          /* the other two corners */
    check_touched(-16, 15, 4, sw);
    check_touched(16, 0, 4, edge_e_nw);   /* chunk 1 local 0, 0: the world's last column of chunks */
    check_touched(-32, 5, 1, edge_w);     /* no chunk west of the world */
    check_touched(31, -32, 1, edge_ne);   /* local x 15, z 0: both past the edge */
    check_touched(-32, -32, 1, corner_nw);
    check_touched(31, 31, 1, corner_se);
    check_touched(-17, 31, 2, edge_s);    /* chunk -2 local x 15: east is chunk -1 */
    check_touched(32, 0, 0, mid);         /* outside: nothing */
    check_touched(-33, 0, 0, mid);
    check_touched(0, 32, 0, mid);
    check_touched(0, -33, 0, mid);
}

/* --- rebuilding only the touched chunks gives the same meshes as all ---- */

typedef struct {
    MeshVertex *v;
    int n;
} Built;

static Built s_cache[SMALL_CHUNKS][SMALL_CHUNKS];

static Built build(int i, int j)
{
    int cx = SMALL_MIN + i, cz = SMALL_MIN + j;
    const Chunk *nb[NB_COUNT];
    Built b;
    int faces;

    world_neighbours(&s_w, cx, cz, nb);
    faces = chunk_mesh_count(world_chunk(&s_w, cx, cz), nb);
    b.v = malloc((size_t)(faces + 1) * MESH_FACE_VERTS * sizeof(MeshVertex));
    b.n = b.v ? chunk_mesh_build(world_chunk(&s_w, cx, cz), nb, (float)(cx * CHUNK_SIZE),
                                 (float)(cz * CHUNK_SIZE), b.v, (faces + 1) * MESH_FACE_VERTS)
              : -1;
    return b;
}

/* Chunks whose kept mesh differs from a fresh build. */
static int stale(void)
{
    int i, j, bad = 0;

    for (j = 0; j < SMALL_CHUNKS; j++)
        for (i = 0; i < SMALL_CHUNKS; i++) {
            Built f = build(i, j);

            if (f.n < 0 || f.n != s_cache[j][i].n ||
                memcmp(f.v, s_cache[j][i].v, (size_t)f.n * sizeof(MeshVertex)) != 0)
                bad++;
            free(f.v);
        }
    return bad;
}

static int s_off_world; /* listed chunks that are not in the world */

/* Break (x, y, z) and rebuild only the chunks world_touched_chunks lists.
 * A listed chunk outside the world is counted, not used (it would index
 * past the cache). */
static void break_and_rebuild(int x, int y, int z)
{
    int out[WORLD_TOUCHED_MAX][2];
    int n, k;

    if (!world_break(&s_w, x, y, z))
        return;
    n = world_touched_chunks(&s_w, x, z, out);
    for (k = 0; k < n; k++) {
        int i = out[k][0] - SMALL_MIN, j = out[k][1] - SMALL_MIN;

        if (i < 0 || i >= SMALL_CHUNKS || j < 0 || j >= SMALL_CHUNKS) {
            s_off_world++;
            continue;
        }
        free(s_cache[j][i].v);
        s_cache[j][i] = build(i, j);
    }
}

static void test_rebuild(void)
{
    /* Spots on every kind of chunk edge and corner, inside and at the
     * world's edge, and a few middles. */
    static const int spots[][2] = {
        { 0, 0 }, { 15, 15 }, { -1, -1 }, { 0, 7 }, { 15, 7 }, { 7, 0 }, { 7, 15 },
        { -16, -16 }, { -17, 3 }, { 16, -17 }, { -32, 4 }, { 31, -9 }, { 5, -32 }, { -6, 31 },
        { -32, -32 }, { 31, 31 }, { 7, 7 }, { -9, 22 }, { 3, -20 },
    };
    uint32_t seed = test_land_seed();
    int s, i, j, d, before = 0, worst = 0;

    test_world_small(&s_w, seed);
    for (j = 0; j < SMALL_CHUNKS; j++)
        for (i = 0; i < SMALL_CHUNKS; i++)
            s_cache[j][i] = build(i, j);
    CHECK_EQ_INT(stale(), 0);

    for (s = 0; s < (int)(sizeof spots / sizeof spots[0]); s++) {
        int x = spots[s][0], z = spots[s][1];
        int h = terrain_height(seed, x, z);

        /* Dig a shaft five deep: grass, three dirt, then stone. */
        for (d = 0; d < 5; d++) {
            int bad;

            break_and_rebuild(x, h - d, z);
            bad = stale();
            if (bad > worst)
                worst = bad;
            before += (world_get(&s_w, x, h - d, z) == BLOCK_AIR);
        }
    }
    CHECK_EQ_INT(worst, 0);
    CHECK_EQ_INT(s_off_world, 0);
    /* Every break happened, so the check above had real work to judge. */
    CHECK_EQ_INT(before, (int)(sizeof spots / sizeof spots[0]) * 5);

    for (j = 0; j < SMALL_CHUNKS; j++)
        for (i = 0; i < SMALL_CHUNKS; i++)
            free(s_cache[j][i].v);
}

void test_break(void)
{
    test_break_rules();
    test_touched_lists();
    test_rebuild();
}
