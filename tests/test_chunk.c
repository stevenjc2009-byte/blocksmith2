#include <stdlib.h>
#include <string.h>

#include "biome.h"
#include "block.h"
#include "chunk.h"
#include "chunk_mesh.h"
#include "mesh.h"
#include "terrain.h"
#include "test.h"

static Chunk s_c;
static Chunk s_nb[NB_COUNT];

static const Chunk *const k_none[NB_COUNT] = { NULL, NULL, NULL, NULL };

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

static int count_none(void)
{
    return chunk_mesh_count(&s_c, k_none);
}

static void test_storage(void)
{
    int x, y, z, sum = 0;

    chunk_clear(&s_c);
    CHECK_EQ_INT(chunk_get(&s_c, 3, 4, 5), BLOCK_AIR);
    chunk_set(&s_c, 3, 4, 5, BLOCK_STONE);
    CHECK_EQ_INT(chunk_get(&s_c, 3, 4, 5), BLOCK_STONE);
    CHECK_EQ_INT(chunk_get(&s_c, 4, 4, 5), BLOCK_AIR); /* x, y, z kept apart */
    CHECK_EQ_INT(chunk_get(&s_c, 3, 5, 5), BLOCK_AIR);
    CHECK_EQ_INT(chunk_get(&s_c, 3, 4, 4), BLOCK_AIR);
    chunk_set(&s_c, 15, 127, 15, BLOCK_GRASS); /* far corner */
    CHECK_EQ_INT(chunk_get(&s_c, 15, 127, 15), BLOCK_GRASS);
    chunk_set(&s_c, 0, 0, 0, BLOCK_DIRT); /* near corner */
    CHECK_EQ_INT(chunk_get(&s_c, 0, 0, 0), BLOCK_DIRT);

    /* Outside the chunk reads as air and writes change nothing. */
    CHECK_EQ_INT(chunk_get(&s_c, -1, 4, 5), BLOCK_AIR);
    CHECK_EQ_INT(chunk_get(&s_c, 16, 4, 5), BLOCK_AIR);
    CHECK_EQ_INT(chunk_get(&s_c, 3, -1, 5), BLOCK_AIR);
    CHECK_EQ_INT(chunk_get(&s_c, 3, 128, 5), BLOCK_AIR);
    CHECK_EQ_INT(chunk_get(&s_c, 3, 4, -1), BLOCK_AIR);
    CHECK_EQ_INT(chunk_get(&s_c, 3, 4, 16), BLOCK_AIR);
    chunk_set(&s_c, -1, 0, 0, BLOCK_STONE);
    chunk_set(&s_c, 16, 0, 0, BLOCK_STONE);
    chunk_set(&s_c, 0, 128, 0, BLOCK_STONE);
    chunk_set(&s_c, 0, 0, -1, BLOCK_STONE);
    chunk_set(&s_c, 1, 1, 1, BLOCK_COUNT); /* not a block */
    chunk_set(&s_c, 1, 1, 1, -1);
    for (y = 0; y < WORLD_HEIGHT; y++)
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++)
                sum += chunk_get(&s_c, x, y, z) != BLOCK_AIR;
    CHECK_EQ_INT(sum, 3);
    CHECK_EQ_INT(chunk_get(&s_c, 1, 1, 1), BLOCK_AIR);
}

static void test_single_block(void)
{
    MeshVertex *v, ref[MESH_BLOCK_VERTS];
    int n;

    chunk_clear(&s_c);
    CHECK_EQ_INT(count_none(), 0);
    n = build(k_none, 0.0f, 0.0f, &v);
    CHECK_EQ_INT(n, 0);
    free(v);

    chunk_set(&s_c, 5, 10, 5, BLOCK_GRASS);
    CHECK_EQ_INT(count_none(), 6);
    /* The chunk's mesh of one block is exactly mesh_block() at the chunk's
     * place in the world: positions, UVs (grass top, sides, dirt bottom)
     * and shades, faces in FACE_* order. */
    n = build(k_none, 32.0f, -16.0f, &v);
    CHECK_EQ_INT(n, MESH_BLOCK_VERTS);
    CHECK_EQ_INT(mesh_block(ref, 37.0f, 10.0f, -11.0f, BLOCK_GRASS), MESH_BLOCK_VERTS);
    /* row 4-02: a cleared chunk is plains, and mesh_block() gives a grass
     * top the plains tint itself */
    CHECK_EQ_INT(memcmp(v, ref, sizeof ref) == 0, 1);
    free(v);
}

static void test_neighbours_inside(void)
{
    chunk_clear(&s_c);
    chunk_set(&s_c, 5, 10, 5, BLOCK_STONE);
    chunk_set(&s_c, 6, 10, 5, BLOCK_STONE);
    CHECK_EQ_INT(count_none(), 10); /* the two touching faces are gone */
    CHECK_EQ_INT(chunk_face_visible(&s_c, k_none, 5, 10, 5, FACE_POS_X), 0);
    CHECK_EQ_INT(chunk_face_visible(&s_c, k_none, 6, 10, 5, FACE_NEG_X), 0);
    CHECK_EQ_INT(chunk_face_visible(&s_c, k_none, 5, 10, 5, FACE_NEG_X), 1);
    CHECK_EQ_INT(chunk_face_visible(&s_c, k_none, 6, 10, 5, FACE_POS_X), 1);

    chunk_set(&s_c, 5, 11, 5, BLOCK_DIRT); /* on top of the first */
    CHECK_EQ_INT(count_none(), 14);
    CHECK_EQ_INT(chunk_face_visible(&s_c, k_none, 5, 10, 5, FACE_POS_Y), 0);
    CHECK_EQ_INT(chunk_face_visible(&s_c, k_none, 5, 11, 5, FACE_NEG_Y), 0);

    chunk_set(&s_c, 5, 10, 6, BLOCK_DIRT); /* south of the first (+z) */
    CHECK_EQ_INT(count_none(), 18);
    CHECK_EQ_INT(chunk_face_visible(&s_c, k_none, 5, 10, 5, FACE_POS_Z), 0);
    CHECK_EQ_INT(chunk_face_visible(&s_c, k_none, 5, 10, 6, FACE_NEG_Z), 0);
    CHECK_EQ_INT(chunk_face_visible(&s_c, k_none, 5, 10, 5, FACE_NEG_Z), 1);
}

static void test_world_top_and_bottom(void)
{
    chunk_clear(&s_c);
    chunk_set(&s_c, 3, 0, 3, BLOCK_STONE); /* the bottom of the world: nothing sees under it */
    CHECK_EQ_INT(count_none(), 5);
    CHECK_EQ_INT(chunk_face_visible(&s_c, k_none, 3, 0, 3, FACE_NEG_Y), 0);
    chunk_clear(&s_c);
    chunk_set(&s_c, 3, 127, 3, BLOCK_STONE); /* the top: open sky above */
    CHECK_EQ_INT(count_none(), 6);
    CHECK_EQ_INT(chunk_face_visible(&s_c, k_none, 3, 127, 3, FACE_POS_Y), 1);
}

static void test_chunk_edges(void)
{
    const Chunk *nb[NB_COUNT] = { &s_nb[0], &s_nb[1], &s_nb[2], &s_nb[3] };
    int i;

    for (i = 0; i < NB_COUNT; i++)
        chunk_clear(&s_nb[i]);
    chunk_clear(&s_c);
    chunk_set(&s_c, 15, 20, 0, BLOCK_STONE); /* east edge, north edge */
    chunk_set(&s_c, 0, 30, 15, BLOCK_STONE); /* west edge, south edge */
    CHECK_EQ_INT(count_none(), 12);          /* no neighbour chunk: open */
    CHECK_EQ_INT(chunk_mesh_count(&s_c, nb), 12);

    /* The matching cell of each neighbour, at the same height. */
    chunk_set(&s_nb[NB_POS_X], 0, 20, 0, BLOCK_DIRT);
    CHECK_EQ_INT(chunk_mesh_count(&s_c, nb), 11);
    CHECK_EQ_INT(chunk_face_visible(&s_c, nb, 15, 20, 0, FACE_POS_X), 0);
    chunk_set(&s_nb[NB_NEG_Z], 15, 20, 15, BLOCK_DIRT);
    CHECK_EQ_INT(chunk_mesh_count(&s_c, nb), 10);
    CHECK_EQ_INT(chunk_face_visible(&s_c, nb, 15, 20, 0, FACE_NEG_Z), 0);
    chunk_set(&s_nb[NB_NEG_X], 15, 30, 15, BLOCK_DIRT);
    CHECK_EQ_INT(chunk_mesh_count(&s_c, nb), 9);
    CHECK_EQ_INT(chunk_face_visible(&s_c, nb, 0, 30, 15, FACE_NEG_X), 0);
    chunk_set(&s_nb[NB_POS_Z], 0, 30, 0, BLOCK_DIRT);
    CHECK_EQ_INT(chunk_mesh_count(&s_c, nb), 8);
    CHECK_EQ_INT(chunk_face_visible(&s_c, nb, 0, 30, 15, FACE_POS_Z), 0);

    /* Neighbour blocks one off in height or along the edge hide nothing. */
    for (i = 0; i < NB_COUNT; i++)
        chunk_clear(&s_nb[i]);
    chunk_set(&s_nb[NB_POS_X], 0, 21, 0, BLOCK_DIRT);
    chunk_set(&s_nb[NB_POS_X], 0, 20, 1, BLOCK_DIRT);
    chunk_set(&s_nb[NB_POS_X], 1, 20, 0, BLOCK_DIRT);
    chunk_set(&s_nb[NB_NEG_Z], 15, 20, 14, BLOCK_DIRT);
    chunk_set(&s_nb[NB_NEG_Z], 14, 20, 15, BLOCK_DIRT);
    chunk_set(&s_nb[NB_NEG_X], 15, 30, 14, BLOCK_DIRT);
    chunk_set(&s_nb[NB_NEG_X], 14, 30, 15, BLOCK_DIRT);
    chunk_set(&s_nb[NB_POS_Z], 0, 30, 1, BLOCK_DIRT);
    chunk_set(&s_nb[NB_POS_Z], 1, 30, 0, BLOCK_DIRT);
    CHECK_EQ_INT(chunk_mesh_count(&s_c, nb), 12);

    /* Some neighbours missing: those sides stay open. */
    {
        const Chunk *part[NB_COUNT] = { &s_nb[0], NULL, NULL, &s_nb[3] };

        chunk_set(&s_nb[NB_POS_X], 0, 20, 0, BLOCK_DIRT);
        chunk_set(&s_nb[NB_NEG_Z], 15, 20, 15, BLOCK_DIRT);
        CHECK_EQ_INT(chunk_mesh_count(&s_c, part), 10);
        CHECK_EQ_INT(chunk_mesh_count(&s_c, NULL), 12); /* no neighbour list at all */
    }
}

/* Stone 0..59, dirt 60..62, grass 63: the row 1-05 scene's layers. */
static void fill_layers(Chunk *c)
{
    int x, y, z;

    chunk_clear(c);
    for (y = 0; y < 64; y++)
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++)
                chunk_set(c, x, y, z, y < 60 ? BLOCK_STONE : y < 63 ? BLOCK_DIRT : BLOCK_GRASS);
}

static void test_full_chunk(void)
{
    const Chunk *nb[NB_COUNT] = { &s_nb[0], &s_nb[1], &s_nb[2], &s_nb[3] };
    int i, x, y, z;

    fill_layers(&s_c);
    /* Top 16 x 16, four walls 16 wide x 64 high, no bottom. Drawing every
     * face of every block would be 16 x 16 x 64 x 6 = 98304 faces. */
    CHECK_EQ_INT(count_none(), 256 + 4 * 16 * 64);
    for (i = 0; i < NB_COUNT; i++)
        fill_layers(&s_nb[i]);
    CHECK_EQ_INT(chunk_mesh_count(&s_c, nb), 256); /* walled in: only the top */

    /* A 4 x 4 pit 3 deep in the middle (x, z 6..9, y 61..63). */
    for (y = 61; y < 64; y++)
        for (z = 6; z < 10; z++)
            for (x = 6; x < 10; x++)
                chunk_set(&s_c, x, y, z, BLOCK_AIR);
    CHECK_EQ_INT(chunk_mesh_count(&s_c, nb), 256 + 4 * 4 * 3); /* floor replaces the lost tops */
    CHECK_EQ_INT(chunk_face_visible(&s_c, nb, 7, 60, 7, FACE_POS_Y), 1);   /* pit floor */
    CHECK_EQ_INT(chunk_face_visible(&s_c, nb, 5, 62, 7, FACE_POS_X), 1);   /* west pit wall */
    CHECK_EQ_INT(chunk_face_visible(&s_c, nb, 5, 60, 7, FACE_POS_X), 0);   /* below the pit */
}

/* A build must not lean on a count just before it: after counting another
 * chunk (full of stone up to the top layer, so every layer it judged was
 * solid), a build alone must give the same mesh as count-then-build. */
static void test_build_without_count(void)
{
    static Chunk full;
    MeshVertex *want, *got;
    int n, m;

    chunk_clear(&s_c);
    chunk_set(&s_c, 3, 10, 4, BLOCK_STONE);
    chunk_set(&s_c, 9, 10, 9, BLOCK_GRASS);
    n = build(k_none, 0.0f, 0.0f, &want);
    CHECK_EQ_INT(n, 12 * MESH_FACE_VERTS);
    memset(full.blocks, BLOCK_STONE, sizeof full.blocks);
    chunk_set(&full, 0, WORLD_HEIGHT - 1, 0, BLOCK_AIR);
    CHECK_EQ_INT(chunk_mesh_count(&full, k_none) > 0, 1);
    got = malloc((size_t)(n > 0 ? n : 1) * sizeof *got);
    m = chunk_mesh_build(&s_c, k_none, 0.0f, 0.0f, got, n);
    CHECK_EQ_INT(m, n);
    CHECK_EQ_INT(m == n && n > 0 && memcmp(want, got, (size_t)n * sizeof *got) == 0, 1);
    free(want);
    free(got);
}

void test_chunk(void)
{
    test_storage();
    test_single_block();
    test_neighbours_inside();
    test_world_top_and_bottom();
    test_chunk_edges();
    test_full_chunk();
    test_build_without_count();
}
