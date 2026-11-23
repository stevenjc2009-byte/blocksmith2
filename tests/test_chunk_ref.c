#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "chunk.h"
#include "chunk_mesh.h"
#include "mesh.h"
#include "terrain.h"
#include "test.h"

/* Chunk meshes against a slow reference mesher written apart from
 * chunk_mesh.c (the first tests are in test_chunk.c). */

static Chunk s_c;
static Chunk s_nb[NB_COUNT];

static const Chunk *const k_none[NB_COUNT] = { NULL };

#define ALL_NB { &s_nb[0], &s_nb[1], &s_nb[2], &s_nb[3], &s_nb[4], &s_nb[5], &s_nb[6], &s_nb[7] }

/* Build the chunk's mesh into a fresh buffer; returns the vertex count or
 * -1, and hands the buffer back (caller frees). */
static int build(const Chunk *const nb[NB_COUNT], float ox, float oz, MeshVertex **out)
{
    int faces = chunk_mesh_count(&s_c, nb);
    int n;

    *out = malloc((size_t)(faces > 0 ? faces : 1) * MESH_FACE_VERTS * sizeof **out);
    n = chunk_mesh_build(&s_c, nb, ox, oz, *out, faces * MESH_FACE_VERTS);
    return n;
}

/* Which neighbour holds the cell one chunk over by (dx, dz), each -1, 0
 * or 1 and not both 0, from the list order in chunk_mesh.h. */
static int ref_nb(int dx, int dz)
{
    if (dz == 0)
        return dx > 0 ? NB_POS_X : NB_NEG_X;
    if (dx == 0)
        return dz > 0 ? NB_POS_Z : NB_NEG_Z;
    if (dz > 0)
        return dx > 0 ? NB_POS_X_POS_Z : NB_NEG_X_POS_Z;
    return dx > 0 ? NB_POS_X_NEG_Z : NB_NEG_X_NEG_Z;
}

/* An independent answer to "is this cell solid?" (so "is this face
 * seen?"), written from the rules in chunk_mesh.h, not from chunk_mesh.c.
 * x and z may be one chunk past either side, or past a corner. */
static int ref_solid(const Chunk *c, const Chunk *const nb[NB_COUNT], int x, int y, int z)
{
    const Chunk *o = c;
    int dx = x >= CHUNK_SIZE ? 1 : x < 0 ? -1 : 0, dz = z >= CHUNK_SIZE ? 1 : z < 0 ? -1 : 0;

    if (y < 0)
        return 1;
    if (y >= WORLD_HEIGHT)
        return 0;
    if (dx || dz) {
        o = nb ? nb[ref_nb(dx, dz)] : NULL;
        x -= dx * CHUNK_SIZE;
        z -= dz * CHUNK_SIZE;
    }
    return o && chunk_get(o, x, y, z) != BLOCK_AIR;
}

typedef struct {
    const Chunk *c;
    const Chunk *const *nb;
} RefCtx;

static int ctx_solid(void *ctx, int x, int y, int z)
{
    const RefCtx *r = ctx;

    return ref_solid(r->c, r->nb, x, y, z);
}

static const int k_step[FACE_COUNT][3] = {
    { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 },
};

static unsigned s_rng;

static int rnd(int n)
{
    s_rng = s_rng * 1103515245u + 12345u;
    return (int)((s_rng >> 16) % (unsigned)n);
}

static void random_fill(Chunk *c, int ylo, int yhi, int pct)
{
    int x, y, z;

    chunk_clear(c);
    for (y = ylo; y < yhi; y++)
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++)
                if (rnd(100) < pct)
                    chunk_set(c, x, y, z, 1 + rnd(BLOCK_COUNT - 1));
}

/* The reference mesh: every block of s_c, every face, through ref_solid(),
 * corner shadows from test_ref_ao() (test_ao.c). Writes the faces to out
 * when it is not NULL; returns how many. */
static int ref_mesh(const Chunk *const nb[NB_COUNT], float ox, float oz, MeshVertex *out)
{
    RefCtx r = { &s_c, nb };
    int x, y, z, f, k, faces = 0;

    for (y = 0; y < WORLD_HEIGHT; y++)
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++) {
                int b = chunk_get(&s_c, x, y, z);

                if (b == BLOCK_AIR)
                    continue;
                for (f = 0; f < FACE_COUNT; f++)
                    if (!ref_solid(&s_c, nb, x + k_step[f][0], y + k_step[f][1], z + k_step[f][2])) {
                        if (out) {
                            unsigned char ao[4];

                            for (k = 0; k < 4; k++)
                                ao[k] = (unsigned char)test_ref_ao(ctx_solid, &r, x, y, z, f, k);
                            mesh_face_ao(out + faces * MESH_FACE_VERTS, ox + (float)x, (float)y,
                                         oz + (float)z, f, block_face_tile(b, f), ao);
                        }
                        faces++;
                    }
            }
    return faces;
}

/* s_c's count and mesh must match the reference exactly. Returns the
 * reference face count. */
static int check_reference(const Chunk *const nb[NB_COUNT], float ox, float oz)
{
    int faces = ref_mesh(nb, ox, oz, NULL), n;
    MeshVertex *ref = malloc((size_t)(faces + 1) * MESH_FACE_VERTS * sizeof *ref), *v;

    ref_mesh(nb, ox, oz, ref);
    CHECK_EQ_INT(chunk_mesh_count(&s_c, nb), faces);
    n = build(nb, ox, oz, &v);
    CHECK_EQ_INT(n, faces * MESH_FACE_VERTS);
    CHECK_EQ_INT(n >= 0 && memcmp(v, ref, (size_t)n * sizeof *v) == 0, 1);
    free(v);
    free(ref);
    return faces;
}

static void test_random_against_reference(void)
{
    static const unsigned seeds[] = { 1, 7, 42, 1234, 99991 };
    const Chunk *nb[NB_COUNT] = ALL_NB;
    size_t si;

    for (si = 0; si < sizeof seeds / sizeof seeds[0]; si++) {
        int x, y, z, i;
        const Chunk *const *use = (si % 2) ? nb : k_none;

        s_rng = seeds[si];
        random_fill(&s_c, 0, 12, 35);          /* bottom of the world */
        for (y = 120; y < WORLD_HEIGHT; y++)   /* and the top */
            for (z = 0; z < CHUNK_SIZE; z++)
                for (x = 0; x < CHUNK_SIZE; x++)
                    if (rnd(100) < 35)
                        chunk_set(&s_c, x, y, z, BLOCK_STONE);
        for (i = 0; i < NB_COUNT; i++)
            random_fill(&s_nb[i], 0, 12, 50);
        CHECK_EQ_INT(check_reference(use, -48.0f, 80.0f) > 0, 1);
    }
}

/* Dig one cell of chunk c to air, check s_c's mesh against the reference
 * and against want faces, then put the cell back. */
static void dig_and_check(Chunk *c, const Chunk *const nb[NB_COUNT], int x, int y, int z, int want)
{
    int was = chunk_get(c, x, y, z);

    CHECK_EQ_INT(was != BLOCK_AIR, 1);
    chunk_set(c, x, y, z, BLOCK_AIR);
    CHECK_EQ_INT(check_reference(nb, 0.0f, 0.0f), want);
    chunk_set(c, x, y, z, was);
}

/* The mesh skips the buried layers at the bottom and the empty ones at the
 * top (chunk_mesh.c face_layers()). Real ground with holes dug right at the
 * edges of that skipping, each against the reference and a face count
 * worked out by hand. */
static void test_buried_layers(void)
{
    static const int k_at[NB_COUNT][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
                                           { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 } };
    const Chunk *nb[NB_COUNT] = ALL_NB;
    const Chunk *some[NB_COUNT];
    int i, k, x, z, low = WORLD_HEIGHT, base;

    terrain_fill_chunk(&s_c, 2026u, 0, 0);
    for (i = 0; i < NB_COUNT; i++)
        terrain_fill_chunk(&s_nb[i], 2026u, k_at[i][0], k_at[i][1]);
    for (z = 0; z < CHUNK_SIZE; z++) /* the chunk's lowest ground */
        for (x = 0; x < CHUNK_SIZE; x++)
            if (terrain_height(2026u, x, z) < low)
                low = terrain_height(2026u, x, z);
    CHECK_EQ_INT(low > 20 && low < 100, 1);

    base = check_reference(nb, 0.0f, 0.0f);
    CHECK_EQ_INT(base >= CHUNK_SIZE * CHUNK_SIZE, 1);
    for (k = 0; k < NB_COUNT; k++) {
        for (i = 0; i < NB_COUNT; i++)
            some[i] = i == k ? NULL : nb[i];
        if (k < NB_SIDES) /* a missing side neighbour shows that side */
            CHECK_EQ_INT(check_reference(some, 0.0f, 0.0f) >= base + CHUNK_SIZE * low, 1);
        else /* a missing corner one only lightens corner shadows (air) */
            CHECK_EQ_INT(check_reference(some, 0.0f, 0.0f), base);
    }
    CHECK_EQ_INT(check_reference(NULL, 0.0f, 0.0f) >= base + 4 * CHUNK_SIZE * low, 1);

    /* A hole deep inside: its six neighbours each show one face. One at the
     * chunk's west edge: five (the sixth is the west neighbour's). One at
     * the very bottom: five (nothing below). Each one just under the lowest
     * full layer's neighbours, so every layer next to it must be meshed. */
    dig_and_check(&s_c, nb, 5, low - 3, 5, base + 6);
    dig_and_check(&s_c, nb, 0, low - 3, 7, base + 5);
    dig_and_check(&s_c, nb, 8, 0, 8, base + 5);
    dig_and_check(&s_c, nb, 9, low - 2, 9, base + 6);
    /* A hole deep in each neighbour, next to this chunk: this chunk's block
     * beside it shows the one face toward it. */
    dig_and_check(&s_nb[NB_NEG_X], nb, CHUNK_SIZE - 1, 10, 7, base + 1);
    dig_and_check(&s_nb[NB_POS_X], nb, 0, 11, 3, base + 1);
    dig_and_check(&s_nb[NB_NEG_Z], nb, 4, 0, CHUNK_SIZE - 1, base + 1);
    dig_and_check(&s_nb[NB_POS_Z], nb, 12, low - 2, 0, base + 1);

    /* A lone block at the top of the world: all six faces. */
    chunk_set(&s_c, 3, WORLD_HEIGHT - 1, 3, BLOCK_STONE);
    CHECK_EQ_INT(check_reference(nb, 0.0f, 0.0f), base + 6);
    chunk_set(&s_c, 3, WORLD_HEIGHT - 1, 3, BLOCK_AIR);

    /* Everything solid to the top: only the 256 top faces. */
    memset(s_c.blocks, BLOCK_STONE, sizeof s_c.blocks);
    for (i = 0; i < NB_COUNT; i++)
        memset(s_nb[i].blocks, BLOCK_STONE, sizeof s_nb[i].blocks);
    CHECK_EQ_INT(check_reference(nb, 0.0f, 0.0f), CHUNK_SIZE * CHUNK_SIZE);
    CHECK_EQ_INT(check_reference(k_none, 0.0f, 0.0f), CHUNK_SIZE * CHUNK_SIZE * (1 + 4 * WORLD_HEIGHT / CHUNK_SIZE));
    chunk_clear(&s_c);
    CHECK_EQ_INT(check_reference(nb, 0.0f, 0.0f), 0);
}

static void test_buffer_too_small(void)
{
    /* Room for 4 faces, but the call is only allowed 3: a 4th face written
     * by mistake lands on the sentinel instead of past the array. */
    MeshVertex buf[4 * MESH_FACE_VERTS];
    float sentinel = 12345.0f;

    chunk_clear(&s_c);
    chunk_set(&s_c, 2, 2, 2, BLOCK_STONE); /* 6 faces, room for 3 */
    buf[3 * MESH_FACE_VERTS].x = sentinel;
    CHECK_EQ_INT(chunk_mesh_build(&s_c, k_none, 0.0f, 0.0f, buf, 3 * MESH_FACE_VERTS), -1);
    CHECK_NEAR(buf[3 * MESH_FACE_VERTS].x, sentinel, 0.0);
    CHECK_EQ_INT(chunk_mesh_build(&s_c, k_none, 0.0f, 0.0f, buf, 0), -1);
}

void test_chunk_ref(void)
{
    test_random_against_reference();
    test_buried_layers();
    test_buffer_too_small();
}
