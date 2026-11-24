#include <math.h>
#include <stdio.h>
#include <string.h>

#include "biome.h"
#include "block.h"
#include "blocktex.h"
#include "chunk.h"
#include "chunk_mesh.h"
#include "mesh.h"
#include "test.h"
#include "water.h"
#include "water_mesh.h"
#include "world.h"

/* Row 4-04: the sloped tops of flowing water (water_mesh.h): each top
 * corner's height comes from the cells round that corner, so cells side by
 * side meet with no gap, in a chunk and across chunk edges; falling water
 * and water with water above stand full height. */

#define MAXV 60000
#define FLOOR 9
#define L 10 /* the layer the water lies on */

static Chunk s_c;
static MeshVertex s_v[MAXV];
static int s_n;
static World s_w;

static void build(void)
{
    s_n = water_mesh_build(&s_c, NULL, 0.0f, 0.0f, s_v, MAXV);
}

/* A stone floor under layer L, nothing else. */
static void floor_chunk(void)
{
    int x, z;

    chunk_clear(&s_c);
    for (z = 0; z < CHUNK_SIZE; z++)
        for (x = 0; x < CHUNK_SIZE; x++)
            chunk_set(&s_c, x, FLOOR, z, BLOCK_STONE);
}

/* The height up layer y of every vertex at corner (cx, cz) in (y, y + 1]:
 * -1 if there are none or they do not all agree. */
static double corner(int cx, int cz, int y)
{
    double h = -1.0;
    int i;

    for (i = 0; i < s_n; i++) {
        const MeshVertex *v = &s_v[i];

        if (v->x != (float)cx || v->z != (float)cz || !(v->y > (float)y && v->y <= (float)(y + 1)))
            continue;
        if (h >= 0.0 && h != (double)v->y - y)
            return -1.0;
        h = (double)v->y - y;
    }
    return h;
}

/* Each top corner averages the water round it. */
static void test_corner_average(void)
{
    floor_chunk();
    chunk_set(&s_c, 5, L, 5, BLOCK_FLOW_2);
    chunk_set(&s_c, 6, L, 5, BLOCK_FLOW_4);
    chunk_set(&s_c, 5, L, 6, BLOCK_WATER);
    build();
    CHECK_EQ_INT(s_n > 0, 1);
    CHECK_NEAR(corner(6, 6, L), (6.0 / 9 + 4.0 / 9 + WATER_TOP) / 3, 1e-5);
    CHECK_NEAR(corner(5, 5, L), 6.0 / 9, 1e-5);                 /* level 2 alone */
    CHECK_NEAR(corner(7, 5, L), 4.0 / 9, 1e-5);                 /* level 4 alone */
    CHECK_NEAR(corner(6, 5, L), (6.0 / 9 + 4.0 / 9) / 2, 1e-5); /* air does not count */
    CHECK_NEAR(corner(5, 6, L), (6.0 / 9 + WATER_TOP) / 2, 1e-5);
    CHECK_NEAR(corner(5, 7, L), WATER_TOP, 1e-6); /* still water alone: as row 4-01 */
    CHECK_NEAR(corner(7, 6, L), 4.0 / 9, 1e-5);

    /* Water above a cell beside a corner lifts it to the top. */
    chunk_set(&s_c, 6, L + 1, 5, BLOCK_FALL);
    build();
    CHECK_NEAR(corner(6, 6, L), 1.0, 1e-6);
    CHECK_NEAR(corner(5, 5, L), 6.0 / 9, 1e-5);
}

/* A level-3 cell beside a level-4 one: the edge they share has the same
 * two vertices on both, and no face stands between them. */
static void test_no_gap(void)
{
    floor_chunk();
    chunk_set(&s_c, 5, L, 5, BLOCK_FLOW_3);
    chunk_set(&s_c, 6, L, 5, BLOCK_FLOW_4);
    build();
    CHECK_EQ_INT(water_mesh_count(&s_c, NULL), 8); /* two tops, three sides each */
    CHECK_NEAR(corner(6, 5, L), 0.5, 1e-6);
    CHECK_NEAR(corner(6, 6, L), 0.5, 1e-6);
    CHECK_NEAR(corner(5, 5, L), 5.0 / 9, 1e-5);
    CHECK_NEAR(corner(7, 6, L), 4.0 / 9, 1e-5);
}

/* A lone level-7 cell: a ninth of a block high, its sides showing that
 * many of the tile's bottom rows. */
static void test_thin(void)
{
    float full[4], low[4], top_v;
    int i, tops = 0, bottoms = 0, bad = 0;

    floor_chunk();
    chunk_set(&s_c, 5, L, 5, BLOCK_FLOW_7);
    build();
    CHECK_EQ_INT(s_n, 5 * MESH_FACE_VERTS);
    blocktex_tile_uv(TILE_WATER, full);
    blocktex_sub_uv(TILE_WATER, 0, BLOCKTEX_TILE - WATER_TOP_TEXELS, BLOCKTEX_TILE,
                    BLOCKTEX_TILE, low);
    top_v = full[3] - (1.0f - 1.0f / 9.0f) * ((float)BLOCKTEX_TILE / (float)BLOCKTEX_SIZE);
    for (i = 0; i < s_n; i++) {
        const MeshVertex *v = &s_v[i];

        if (v->r == 1.0f) /* the top: the whole tile */
            bad += v->v != full[1] && v->v != full[3];
        else if (v->y > (float)L) {
            tops++;
            bad += fabsf(v->y - ((float)L + 1.0f / 9.0f)) > 1e-5f;
            bad += fabsf(v->v - top_v) > 1e-6f;
            bad += v->u != low[0] && v->u != low[2];
        } else {
            bottoms++;
            bad += v->v != low[1];
        }
    }
    CHECK_EQ_INT(tops, 4 * 3);
    CHECK_EQ_INT(bottoms, 4 * 3);
    CHECK_EQ_INT(bad, 0);
    CHECK_NEAR(corner(5, 5, L), 1.0 / 9, 1e-5);
}

/* Falling water under a source: full height all the way down. */
static void test_falling(void)
{
    floor_chunk();
    chunk_set(&s_c, 5, L, 5, BLOCK_FALL);
    chunk_set(&s_c, 5, L + 1, 5, BLOCK_FALL);
    chunk_set(&s_c, 5, L + 2, 5, BLOCK_FALL);
    chunk_set(&s_c, 5, L + 3, 5, BLOCK_WATER);
    build();
    CHECK_EQ_INT(water_mesh_count(&s_c, NULL), 3 * 4 + 5);
    CHECK_NEAR(corner(5, 5, L), 1.0, 1e-6);
    CHECK_NEAR(corner(6, 6, L + 2), 1.0, 1e-6);
    CHECK_NEAR(corner(6, 6, L + 3), WATER_TOP, 1e-6);
    /* A falling cell with nothing above (before its turn): still full. */
    chunk_set(&s_c, 5, L + 3, 5, BLOCK_AIR);
    chunk_set(&s_c, 5, L + 2, 5, BLOCK_AIR);
    chunk_set(&s_c, 5, L + 1, 5, BLOCK_AIR);
    build();
    CHECK_NEAR(corner(5, 5, L), 1.0, 1e-6);
}

static unsigned s_rng;

static int rnd(int n)
{
    s_rng = s_rng * 1103515245u + 12345u;
    return (int)((s_rng >> 16) % (unsigned)n);
}

#define RLO (-6)
#define RHI 6
#define YLO 20
#define YHI 28
#define SPAN (RHI - RLO + 1)

static float s_h[YHI - YLO][SPAN][SPAN];
static bool s_has[YHI - YLO][SPAN][SPAN];

/* Note the height of top corner (x, y, z), as layer y + h: 1 if it
 * disagrees with one noted before. */
static int note(float x, float y, float z)
{
    int layer = (int)ceilf(y) - 1, i = (int)x - RLO, k = (int)z - RLO, j = layer - YLO;
    float h = y - (float)layer;

    if (i < 0 || i >= SPAN || k < 0 || k >= SPAN || j < 0 || j >= YHI - YLO || !(h > 0.0f))
        return 1;
    if (s_has[j][k][i])
        return s_h[j][k][i] != h;
    s_has[j][k][i] = true;
    s_h[j][k][i] = h;
    return 0;
}

static bool open_at(int x, int y, int z)
{
    int b = world_get(&s_w, x, y, z);

    return !block_solid(b) && !block_is_water(b);
}

/* Random flowing and falling water across four chunks' corners: every top
 * corner where cells meet has one height, chunk edges too, and every face
 * the rules ask for is there. */
static void test_random_cracks(void)
{
    static const unsigned seeds[] = { 5, 42, 2026 };
    size_t si;
    int x, y, z, cx, cz, i, cracks = 0, faces = 0, want = 0, corners = 0;

    for (si = 0; si < sizeof seeds / sizeof seeds[0]; si++) {
        s_rng = seeds[si];
        test_world_cleared(&s_w);
        for (y = YLO; y < YHI - 1; y++)
            for (z = RLO; z < RHI; z++)
                for (x = RLO; x < RHI; x++) {
                    int r = rnd(100);

                    if (r < 45)
                        world_set(&s_w, x, y, z, BLOCK_FLOW_1 + rnd(WATER_LEVEL_MAX + 1));
                    else if (r < 60)
                        world_set(&s_w, x, y, z, BLOCK_STONE);
                }
        memset(s_has, 0, sizeof s_has);
        for (cz = -1; cz <= 0; cz++)
            for (cx = -1; cx <= 0; cx++) {
                const Chunk *nb[NB_COUNT];

                world_neighbours(&s_w, cx, cz, nb);
                s_n = water_mesh_build(world_chunk(&s_w, cx, cz), nb, (float)(cx * CHUNK_SIZE),
                                       (float)(cz * CHUNK_SIZE), s_v, MAXV);
                CHECK_EQ_INT(s_n >= 0, 1);
                faces += s_n / MESH_FACE_VERTS;
                for (i = 0; i + MESH_FACE_VERTS <= s_n; i += MESH_FACE_VERTS) {
                    const MeshVertex *f = &s_v[i];

                    if (f->r == 0.5f) /* a bottom */
                        continue;
                    if (f->r == 1.0f) { /* a top: all four corners */
                        cracks += note(f[0].x, f[0].y, f[0].z) + note(f[1].x, f[1].y, f[1].z);
                        corners += 2;
                    }
                    cracks += note(f[2].x, f[2].y, f[2].z) + note(f[5].x, f[5].y, f[5].z);
                    corners += 2;
                }
            }
        for (y = YLO; y < YHI; y++)
            for (z = RLO; z < RHI; z++)
                for (x = RLO; x < RHI; x++) {
                    if (!block_is_water(world_get(&s_w, x, y, z)))
                        continue;
                    want += !block_is_water(world_get(&s_w, x, y + 1, z));
                    want += open_at(x, y - 1, z) + open_at(x + 1, y, z) + open_at(x - 1, y, z) +
                            open_at(x, y, z + 1) + open_at(x, y, z - 1);
                }
    }
    CHECK_EQ_INT(cracks, 0);
    CHECK_EQ_INT(faces, want);
    CHECK_EQ_INT(corners > 3000, 1);
}

/* The blocks' mesh (chunk_mesh.h) treats water of every kind as air, so
 * flowing water running into air, or drying back to air, changes only the
 * water mesh: meshes_water_changed() (meshes.h) rebuilds just that unless
 * a torch was washed away. Random stone, a torch and air, built; then
 * every air cell made random flowing or falling water, built again: the
 * same faces, byte for byte. */
static void test_solid_mesh_ignores_water(void)
{
    static MeshVertex before[MAXV];
    int x, y, z, n0, n1, c0;

    s_rng = 77;
    chunk_clear(&s_c);
    for (y = 20; y < 25; y++)
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++)
                if (rnd(100) < 40)
                    chunk_set(&s_c, x, y, z, BLOCK_STONE);
    chunk_set(&s_c, 3, 25, 3, BLOCK_TORCH);
    n0 = chunk_mesh_build(&s_c, NULL, 0.0f, 0.0f, before, MAXV);
    c0 = chunk_mesh_count(&s_c, NULL);
    for (y = 20; y < 27; y++)
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++)
                if (chunk_get(&s_c, x, y, z) == BLOCK_AIR)
                    chunk_set(&s_c, x, y, z, BLOCK_FLOW_1 + rnd(WATER_LEVEL_MAX + 1));
    n1 = chunk_mesh_build(&s_c, NULL, 0.0f, 0.0f, s_v, MAXV);
    CHECK_EQ_INT(n0 > 1000, 1);
    CHECK_EQ_INT(n1, n0);
    CHECK_EQ_INT(n1 == n0 && memcmp(before, s_v, (size_t)n0 * sizeof *s_v) == 0, 1);
    CHECK_EQ_INT(chunk_mesh_count(&s_c, NULL), c0); /* the count skips it too */
}

/* Flowing water anywhere in a chunk sends its water down the sloped-corner
 * path; still water must come out of that path exactly as row 4-01 drew
 * it. A still pool with a step in it (still water a layer higher over half
 * of it) is the hard case: a corner beside the higher water stays at
 * WATER_TOP, not full height. The flowing cell sits higher than the pool,
 * so its faces come after the pool's. */
static void test_still_step_unchanged(void)
{
    static MeshVertex before[MAXV];
    int x, z, n0;

    floor_chunk();
    for (z = 4; z < 8; z++)
        for (x = 4; x < 8; x++) {
            chunk_set(&s_c, x, L, z, BLOCK_WATER);
            if (x < 6)
                chunk_set(&s_c, x, L + 1, z, BLOCK_WATER);
        }
    build();
    n0 = s_n;
    memcpy(before, s_v, (size_t)n0 * sizeof *s_v);
    chunk_set(&s_c, 12, L + 5, 12, BLOCK_FLOW_3);
    build();
    CHECK_EQ_INT(n0 > 0 && s_n > n0, 1);
    CHECK_EQ_INT(memcmp(before, s_v, (size_t)n0 * sizeof *s_v) == 0, 1);
}

/* A layer of nothing but flowing water over solid stone: the block mesh
 * must still find the stone's top faces under it, as it does under air
 * (the mesh starts at the first layer with anything not solid in it). The
 * chunk is its own neighbour on every side: a missing neighbour counts as
 * air and would let the mesh start at the bottom anyway. */
static void test_water_layer_over_stone(void)
{
    static MeshVertex before[MAXV];
    const Chunk *const self[NB_COUNT] = { &s_c, &s_c, &s_c, &s_c };
    int x, y, z, n0, n1, c0;

    s_rng = 78;
    chunk_clear(&s_c);
    for (y = 0; y < 20; y++) /* solid stone up to layer 19, air above */
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++)
                chunk_set(&s_c, x, y, z, BLOCK_STONE);
    n0 = chunk_mesh_build(&s_c, self, 0.0f, 0.0f, before, MAXV);
    c0 = chunk_mesh_count(&s_c, self);
    for (z = 0; z < CHUNK_SIZE; z++)
        for (x = 0; x < CHUNK_SIZE; x++)
            chunk_set(&s_c, x, 20, z, BLOCK_FLOW_1 + rnd(WATER_LEVEL_MAX + 1));
    n1 = chunk_mesh_build(&s_c, self, 0.0f, 0.0f, s_v, MAXV);
    CHECK_EQ_INT(n0 > 0, 1);
    CHECK_EQ_INT(n1, n0);
    CHECK_EQ_INT(n1 == n0 && memcmp(before, s_v, (size_t)n0 * sizeof *s_v) == 0, 1);
    CHECK_EQ_INT(chunk_mesh_count(&s_c, self), c0);
}

/* Row 4-02's water colour: every face of water in a swamp column is its
 * shade times biome_water_rgb(BIOME_SWAMP), plains water keeps its plain
 * shade, and only the swamp column's faces change. */
static void test_biome_tint(void)
{
    float w[3];
    int i, swamp = 0, plain = 0, bad = 0;

    floor_chunk();
    chunk_set(&s_c, 3, L, 3, BLOCK_WATER);
    chunk_set(&s_c, 9, L, 9, BLOCK_FLOW_3);
    s_c.biome[3][3] = BIOME_SWAMP;
    s_c.biome[9][9] = BIOME_PLAINS;
    build();
    biome_water_rgb(BIOME_SWAMP, w);
    for (i = 0; i < s_n; i++) {
        const MeshVertex *v = &s_v[i];
        bool in_swamp = v->x >= 3.0f && v->x <= 4.0f && v->z >= 3.0f && v->z <= 4.0f;

        if (in_swamp) {
            swamp++;
            bad += fabsf(v->g * w[0] - v->r * w[1]) > 1e-5f || fabsf(v->b * w[0] - v->r * w[2]) > 1e-5f ||
                   v->r >= v->g; /* swamp green-brown: red below green */
        } else {
            plain++;
            bad += v->r != v->g || v->g != v->b;
        }
    }
    CHECK_EQ_INT(swamp > 0 && plain > 0, 1);
    CHECK_EQ_INT(bad, 0);
    s_c.biome[3][3] = BIOME_PLAINS;
}

void test_flow_mesh(void)
{
    test_corner_average();
    test_no_gap();
    test_thin();
    test_falling();
    test_random_cracks();
    test_solid_mesh_ignores_water();
    test_water_layer_over_stone();
    test_still_step_unchanged();
    test_biome_tint();
}
