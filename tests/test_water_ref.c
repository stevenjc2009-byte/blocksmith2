#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "blocktex.h"
#include "chunk.h"
#include "chunk_mesh.h"
#include "mesh.h"
#include "test.h"
#include "water.h"
#include "water_mesh.h"

/* Row 4-01: water meshes against a slow reference written from the rules
 * in water_mesh.h, not from water_mesh.c, and a few cases worked out by
 * hand. */

static Chunk s_c;
static Chunk s_nb[NB_COUNT];

static const Chunk *const k_none[NB_COUNT] = { NULL };

#define ALL_NB { &s_nb[0], &s_nb[1], &s_nb[2], &s_nb[3], &s_nb[4], &s_nb[5], &s_nb[6], &s_nb[7] }

enum { OPEN, SOLID, WATER };

static const int k_step[FACE_COUNT][3] = {
    { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 },
};

/* What the cell at local (x, y, z) is, by the rules: x and z at most one
 * past a side (never past a corner: a face only looks across a side). */
static int ref_kind(const Chunk *const nb[NB_COUNT], int x, int y, int z)
{
    const Chunk *o = &s_c;
    int b;

    if (y < 0)
        return SOLID;
    if (y >= WORLD_HEIGHT)
        return OPEN;
    if (x < 0 || x >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
        int which = x < 0 ? NB_NEG_X : x >= CHUNK_SIZE ? NB_POS_X : z < 0 ? NB_NEG_Z : NB_POS_Z;

        o = nb ? nb[which] : NULL;
        x = (x + CHUNK_SIZE) % CHUNK_SIZE;
        z = (z + CHUNK_SIZE) % CHUNK_SIZE;
    }
    if (o == NULL)
        return OPEN;
    b = chunk_get(o, x, y, z);
    if (b == BLOCK_WATER)
        return WATER;
    return b != BLOCK_AIR && b != BLOCK_TORCH ? SOLID : OPEN; /* water is returned above */
}

/* One face of the water cell at (x, y, z), height h, torch light t. */
static void ref_face(MeshVertex *out, float ox, float oz, int x, int y, int z, int f, float h,
                     float t)
{
    static const int k_tri[MESH_FACE_VERTS] = { 0, 1, 2, 0, 2, 3 };
    float uv[4];
    int i;

    if (f == FACE_POS_Y || f == FACE_NEG_Y || h == 1.0f)
        blocktex_tile_uv(TILE_WATER, uv);
    else /* the tile's bottom rows, as many as the side is high */
        blocktex_sub_uv(TILE_WATER, 0, BLOCKTEX_TILE - WATER_TOP_TEXELS, BLOCKTEX_TILE,
                        BLOCKTEX_TILE, uv);
    for (i = 0; i < MESH_FACE_VERTS; i++) {
        int c = k_tri[i];
        const signed char *p = mesh_face_corner(f, c);
        MeshVertex *v = &out[i];

        v->x = ox + (float)x + (float)p[0];
        v->y = (float)y + (p[1] ? h : 0.0f);
        v->z = oz + (float)z + (float)p[2];
        v->u = (c == 1 || c == 2) ? uv[2] : uv[0];
        v->v = c >= 2 ? uv[3] : uv[1];
        v->r = v->g = v->b = mesh_face_shade(f);
        v->t = t;
    }
}

/* The reference water mesh of s_c; writes the faces to out when it is not
 * NULL. Returns how many. */
static int ref_mesh(const Chunk *const nb[NB_COUNT], float ox, float oz, MeshVertex *out)
{
    int x, y, z, f, faces = 0;

    if (out)
        test_ref_light(&s_c, nb);
    for (y = 0; y < WORLD_HEIGHT; y++)
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++) {
                float h;

                if (chunk_get(&s_c, x, y, z) != BLOCK_WATER)
                    continue;
                h = ref_kind(nb, x, y + 1, z) == WATER ? 1.0f : WATER_TOP;
                for (f = 0; f < FACE_COUNT; f++) {
                    int k = ref_kind(nb, x + k_step[f][0], y + k_step[f][1], z + k_step[f][2]);

                    if (f == FACE_POS_Y ? k == WATER : k != OPEN)
                        continue;
                    if (out)
                        ref_face(out + faces * MESH_FACE_VERTS, ox, oz, x, y, z, f, h,
                                 (float)test_ref_level(x, y, z) / 14.0f);
                    faces++;
                }
            }
    return faces;
}

/* s_c's water count and mesh must match the reference exactly. Returns the
 * reference face count; the mesh is left in *mesh (caller frees) when it
 * is not NULL. */
static int check_reference(const Chunk *const nb[NB_COUNT], float ox, float oz, MeshVertex **mesh)
{
    int faces = ref_mesh(nb, ox, oz, NULL), n;
    MeshVertex *ref = malloc((size_t)(faces + 1) * MESH_FACE_VERTS * sizeof *ref);
    MeshVertex *v = malloc((size_t)(faces + 1) * MESH_FACE_VERTS * sizeof *v);

    ref_mesh(nb, ox, oz, ref);
    CHECK_EQ_INT(water_mesh_count(&s_c, nb), faces);
    n = water_mesh_build(&s_c, nb, ox, oz, v, faces * MESH_FACE_VERTS);
    CHECK_EQ_INT(n, faces * MESH_FACE_VERTS);
    CHECK_EQ_INT(n >= 0 && memcmp(v, ref, (size_t)n * sizeof *v) == 0, 1);
    free(ref);
    if (mesh)
        *mesh = v;
    else
        free(v);
    return faces;
}

static unsigned s_rng;

static int rnd(int n)
{
    s_rng = s_rng * 1103515245u + 12345u;
    return (int)((s_rng >> 16) % (unsigned)n);
}

/* Layers ylo to yhi - 1: water in about `water` cells in a hundred, any
 * other block (sand and torches too) in about `other`, air in the rest. */
static void random_fill(Chunk *c, int ylo, int yhi, int water, int other)
{
    int x, y, z;

    chunk_clear(c);
    for (y = ylo; y < yhi; y++)
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++) {
                int r = rnd(100);

                if (r < water) {
                    chunk_set(c, x, y, z, BLOCK_WATER);
                } else if (r < water + other) {
                    int b = 1 + rnd(BLOCK_SHORT_GRASS - 1);

                    /* Still water only: this reference is row 4-01's
                     * (flowing water: test_flow_mesh.c); no plants (row 4-03:
                     * test_trees_mesh.c). */
                    chunk_set(c, x, y, z, block_is_water(b) ? BLOCK_WATER : b);
                }
            }
}

static void test_random_against_reference(void)
{
    static const unsigned seeds[] = { 3, 11, 77, 2026, 31337, 65521 };
    const Chunk *nb[NB_COUNT] = ALL_NB;
    size_t si;
    int i, total = 0;

    for (si = 0; si < sizeof seeds / sizeof seeds[0]; si++) {
        const Chunk *const *use = si % 3 == 0 ? k_none : si % 3 == 1 ? nb : NULL;

        s_rng = seeds[si];
        random_fill(&s_c, 0, 14, 40, 35); /* the bottom of the world */
        for (i = 0; i < NB_COUNT; i++)
            random_fill(&s_nb[i], 0, 14, 30, 40);
        chunk_set(&s_c, 6, WORLD_HEIGHT - 1, 9, BLOCK_WATER); /* and the very top */
        chunk_set(&s_c, 7, WORLD_HEIGHT - 1, 9, BLOCK_WATER);
        total += check_reference(use, 32.0f, -64.0f, NULL);
    }
    CHECK_EQ_INT(total > 6000, 1);
}

/* Ocean: stone to 40, sand to 44, water from 45 to 62. */
static void ocean(Chunk *c)
{
    int y;

    chunk_clear(c);
    for (y = 0; y <= 62; y++)
        memset(c->blocks[y], y <= 40 ? BLOCK_STONE : y <= 44 ? BLOCK_SAND : BLOCK_WATER,
               sizeof c->blocks[y]);
}

/* Cases worked out by hand, each also against the reference. */
static void test_by_hand(void)
{
    const Chunk *nb[NB_COUNT] = ALL_NB;
    MeshVertex *v;
    int i, low = 0, high = 0, at_top = 0;

    /* A lone water cell in the air: six faces, its top lowered to
     * y + WATER_TOP, its sides that high, its bottom at y. */
    chunk_clear(&s_c);
    chunk_set(&s_c, 5, 10, 5, BLOCK_WATER);
    CHECK_EQ_INT(check_reference(k_none, 0.0f, 0.0f, &v), 6);
    for (i = 0; i < 6 * MESH_FACE_VERTS; i++) {
        low += v[i].y == 10.0f;
        high += v[i].y == 10.0f + WATER_TOP;
        at_top += i / MESH_FACE_VERTS == FACE_POS_Y && v[i].y == 10.875f;
    }
    CHECK_EQ_INT(low + high, 36);
    CHECK_EQ_INT(high, 4 * 3 + 6); /* half of each side's corners, all the top's */
    CHECK_EQ_INT(at_top, MESH_FACE_VERTS);
    free(v);

    /* Two side by side: no face between them. */
    chunk_set(&s_c, 6, 10, 5, BLOCK_WATER);
    CHECK_EQ_INT(check_reference(k_none, 0.0f, 0.0f, NULL), 10);
    /* Two stacked: the lower has no top and is full height, the upper is
     * lowered. */
    chunk_set(&s_c, 6, 10, 5, BLOCK_AIR);
    chunk_set(&s_c, 5, 11, 5, BLOCK_WATER);
    CHECK_EQ_INT(check_reference(k_none, 0.0f, 0.0f, &v), 10);
    CHECK_NEAR(v[0].y + v[1].y + v[2].y, 10.0f + 10.0f + 11.0f, 0.0); /* lower +x side */
    free(v);

    /* In a stone box open above: only the top. With stone above too: the
     * top still shows, lowered under the stone. */
    chunk_clear(&s_c);
    chunk_set(&s_c, 5, 10, 5, BLOCK_WATER);
    for (i = 0; i < FACE_COUNT; i++)
        if (i != FACE_POS_Y)
            chunk_set(&s_c, 5 + k_step[i][0], 10 + k_step[i][1], 5 + k_step[i][2], BLOCK_SAND);
    CHECK_EQ_INT(check_reference(k_none, 0.0f, 0.0f, NULL), 1);
    chunk_set(&s_c, 5, 11, 5, BLOCK_STONE);
    CHECK_EQ_INT(check_reference(k_none, 0.0f, 0.0f, NULL), 1);
    chunk_set(&s_c, 6, 10, 5, BLOCK_TORCH); /* a torch beside it: open */
    CHECK_EQ_INT(check_reference(k_none, 0.0f, 0.0f, NULL), 2);

    /* At the bottom of the world: no bottom face. At the top: a top face. */
    chunk_clear(&s_c);
    chunk_set(&s_c, 2, 0, 2, BLOCK_WATER);
    chunk_set(&s_c, 9, WORLD_HEIGHT - 1, 9, BLOCK_WATER);
    CHECK_EQ_INT(check_reference(k_none, 0.0f, 0.0f, NULL), 5 + 6);

    /* At the chunk's west edge: the west neighbour decides that face. */
    chunk_clear(&s_c);
    chunk_set(&s_c, 0, 20, 7, BLOCK_WATER);
    for (i = 0; i < NB_COUNT; i++)
        chunk_clear(&s_nb[i]);
    CHECK_EQ_INT(check_reference(nb, 0.0f, 0.0f, NULL), 6);
    chunk_set(&s_nb[NB_NEG_X], CHUNK_SIZE - 1, 20, 7, BLOCK_WATER);
    CHECK_EQ_INT(check_reference(nb, 0.0f, 0.0f, NULL), 5);
    chunk_set(&s_nb[NB_NEG_X], CHUNK_SIZE - 1, 20, 7, BLOCK_DIRT);
    CHECK_EQ_INT(check_reference(nb, 0.0f, 0.0f, NULL), 5);
    CHECK_EQ_INT(check_reference(k_none, 0.0f, 0.0f, NULL), 6); /* missing: open */

    /* An ocean among oceans: only its 256 tops, all at 62 + WATER_TOP. With
     * no neighbours its four sides show too, 18 cells high. */
    ocean(&s_c);
    for (i = 0; i < NB_COUNT; i++)
        ocean(&s_nb[i]);
    CHECK_EQ_INT(check_reference(nb, 0.0f, 0.0f, &v), CHUNK_SIZE * CHUNK_SIZE);
    for (i = high = 0; i < CHUNK_SIZE * CHUNK_SIZE * MESH_FACE_VERTS; i++)
        high += v[i].y == 62.0f + WATER_TOP;
    CHECK_EQ_INT(high, CHUNK_SIZE * CHUNK_SIZE * MESH_FACE_VERTS);
    free(v);
    CHECK_EQ_INT(check_reference(k_none, 0.0f, 0.0f, NULL),
                 CHUNK_SIZE * CHUNK_SIZE + 4 * CHUNK_SIZE * 18);
    /* The solid mesh leaves the water out but shows the sand under it. */
    CHECK_EQ_INT(chunk_mesh_count(&s_c, nb), CHUNK_SIZE * CHUNK_SIZE);
    chunk_clear(&s_c);
    CHECK_EQ_INT(check_reference(nb, 0.0f, 0.0f, NULL), 0);
}

/* A torch under water lights the water round it: its own cell's level. */
static void test_torch_light(void)
{
    MeshVertex *v;
    int i, lit = 0;

    ocean(&s_c);
    chunk_set(&s_c, 8, 45, 8, BLOCK_TORCH);
    CHECK_EQ_INT(check_reference(k_none, 0.0f, 0.0f, &v) > CHUNK_SIZE * CHUNK_SIZE, 1);
    for (i = 0; i < CHUNK_SIZE * CHUNK_SIZE * MESH_FACE_VERTS; i++)
        lit += v[i].t > 0.0f;
    CHECK_EQ_INT(lit > 0, 1);
    free(v);
}

static void test_buffer_too_small(void)
{
    MeshVertex buf[4 * MESH_FACE_VERTS];

    chunk_clear(&s_c);
    chunk_set(&s_c, 2, 2, 2, BLOCK_WATER); /* 6 faces, room for 3 */
    buf[3 * MESH_FACE_VERTS].x = 12345.0f;
    CHECK_EQ_INT(water_mesh_build(&s_c, k_none, 0.0f, 0.0f, buf, 3 * MESH_FACE_VERTS), -1);
    CHECK_NEAR(buf[3 * MESH_FACE_VERTS].x, 12345.0f, 0.0);
    CHECK_EQ_INT(water_mesh_build(&s_c, k_none, 0.0f, 0.0f, buf, 0), -1);
}

void test_water_ref(void)
{
    test_random_against_reference();
    test_by_hand();
    test_torch_light();
    test_buffer_too_small();
}
