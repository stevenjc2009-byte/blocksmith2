#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "blocktex.h"
#include "chunk.h"
#include "chunk_mesh.h"
#include "mesh.h"
#include "test.h"

/* Row 3-01: soft corner shadows (ambient occlusion), mesh.h and
 * chunk_mesh.h. The random chunk and world checks are in test_chunk_ref.c
 * and test_world.c, against test_ref_ao(). */

#define TOL 1e-5

static const int k_out[FACE_COUNT][3] = {
    { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 },
};

/* Written from the rule in chunk_mesh.h, looking from the corner itself
 * rather than from the face: the four cells of the layer in front of the
 * face that touch the corner's point, sorted by how many of the two
 * across-the-face coordinates differ from the cell in front of the face
 * (one: a side, two: across the corner). */
int test_ref_ao(int (*solid)(void *ctx, int x, int y, int z), void *ctx,
                int bx, int by, int bz, int face, int corner)
{
    const signed char *p = mesh_face_corner(face, corner);
    int v[3] = { bx + p[0], by + p[1], bz + p[2] };
    int front[3] = { bx + k_out[face][0], by + k_out[face][1], bz + k_out[face][2] };
    int a = face / 2, sides = 0, across = 0, i, j;

    for (j = 0; j < 2; j++)
        for (i = 0; i < 2; i++) {
            int cell[3], t, k = 0, differ = 0;

            for (t = 0; t < 3; t++) {
                if (t == a) {
                    cell[t] = front[t];
                    continue;
                }
                cell[t] = v[t] - (k++ == 0 ? i : j); /* the cells on either side of the point */
                differ += cell[t] != front[t];
            }
            if (differ != 0 && solid(ctx, cell[0], cell[1], cell[2])) {
                if (differ == 1)
                    sides++;
                else
                    across++;
            }
        }
    return sides == 2 ? 0 : MESH_AO_OPEN - sides - across;
}

/* --- mesh.h ------------------------------------------------------------- */

/* The corner (mesh_face_corner() order) of face f of a block at the origin
 * that vertex v sits on, or -1. */
static int corner_of(const MeshVertex *v, int f)
{
    int c;

    for (c = 0; c < 4; c++) {
        const signed char *p = mesh_face_corner(f, c);

        if (v->x == p[0] && v->y == p[1] && v->z == p[2])
            return c;
    }
    return -1;
}

static void check_face_ao(int f, const unsigned char ao[4])
{
    MeshVertex v[MESH_FACE_VERTS];
    float area = 0.0f;
    int i, k, in_both = 0, want = abs(ao[0] - ao[2]) > abs(ao[1] - ao[3]) ? 1 : 0;
    int fails = g_fails;

    mesh_face_ao(v, 0.0f, 0.0f, 0.0f, f, TILE_STONE, ao);
    for (i = 0; i < MESH_FACE_VERTS; i++) {
        int c = corner_of(&v[i], f);

        CHECK_EQ_INT(c >= 0, 1);
        if (c < 0)
            continue;
        CHECK_NEAR(v[i].r, mesh_face_shade(f) * mesh_ao_scale(ao[c]), TOL);
        CHECK_NEAR(v[i].g, v[i].r, 0.0);
        CHECK_NEAR(v[i].b, v[i].r, 0.0);
    }
    for (k = 0; k < 2; k++) { /* both triangles face out; together they fill the square */
        const MeshVertex *t = &v[k * 3];
        float e1[3] = { t[1].x - t[0].x, t[1].y - t[0].y, t[1].z - t[0].z };
        float e2[3] = { t[2].x - t[0].x, t[2].y - t[0].y, t[2].z - t[0].z };
        float n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                       e1[0] * e2[1] - e1[1] * e2[0] };
        float du1 = t[1].u - t[0].u, dv1 = t[1].v - t[0].v, du2 = t[2].u - t[0].u,
              dv2 = t[2].v - t[0].v;

        for (i = 0; i < 3; i++)
            CHECK_NEAR(n[i], k_out[f][i], TOL); /* unit square: the cross is the normal */
        area += 0.5f * sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        CHECK_EQ_INT(du1 * dv2 - dv1 * du2 > 0.0f, 1); /* tile not mirrored */
    }
    CHECK_NEAR(area, 1.0, TOL);
    /* The cut: the two corners both triangles use. */
    for (i = 0; i < 3; i++)
        for (k = 3; k < 6; k++)
            if (corner_of(&v[i], f) == corner_of(&v[k], f))
                in_both |= 1 << corner_of(&v[i], f);
    CHECK_EQ_INT(in_both, want ? 0xA : 0x5);
    if (g_fails != fails)
        printf("     (face %d, levels %d %d %d %d)\n", f, ao[0], ao[1], ao[2], ao[3]);
}

static void test_mesh_ao(void)
{
    static const unsigned char k_cases[][4] = {
        { 3, 3, 3, 3 }, { 0, 3, 3, 3 }, { 3, 0, 3, 3 }, { 3, 3, 0, 3 }, { 3, 3, 3, 0 },
        { 2, 2, 3, 3 }, { 0, 1, 3, 1 }, { 1, 0, 1, 3 }, { 3, 1, 1, 1 }, { 0, 0, 0, 0 },
        { 2, 3, 1, 3 }, { 1, 2, 3, 0 },
    };
    MeshVertex a[MESH_FACE_VERTS], b[MESH_FACE_VERTS];
    unsigned char open[4] = { MESH_AO_OPEN, MESH_AO_OPEN, MESH_AO_OPEN, MESH_AO_OPEN };
    int f, k;

    CHECK_NEAR(mesh_ao_scale(0), 0.6, TOL);
    CHECK_NEAR(mesh_ao_scale(1), 0.6 + 0.4 / 3.0, TOL);
    CHECK_NEAR(mesh_ao_scale(2), 0.6 + 0.8 / 3.0, TOL);
    CHECK_NEAR(mesh_ao_scale(MESH_AO_OPEN), 1.0, 0.0);
    CHECK_NEAR(mesh_ao_scale(-1), 1.0, 0.0);
    CHECK_NEAR(mesh_ao_scale(MESH_AO_OPEN + 1), 1.0, 0.0);
    for (f = 0; f < FACE_COUNT; f++) {
        /* mesh_face() is the open case, bit for bit. */
        mesh_face(a, 2.0f, 3.0f, -4.0f, f, TILE_GRASS_SIDE);
        mesh_face_ao(b, 2.0f, 3.0f, -4.0f, f, TILE_GRASS_SIDE, open);
        CHECK_EQ_INT(memcmp(a, b, sizeof a), 0);
        for (k = 0; k < (int)(sizeof k_cases / sizeof k_cases[0]); k++)
            check_face_ao(f, k_cases[k]);
    }
}

/* --- chunk_mesh.h ------------------------------------------------------- */

static Chunk s_c, s_nb[NB_COUNT];
static MeshVertex *s_v;
static int s_n;

static void build(const Chunk *const nb[NB_COUNT])
{
    int faces = chunk_mesh_count(&s_c, nb);

    free(s_v);
    s_v = malloc((size_t)(faces + 1) * MESH_FACE_VERTS * sizeof *s_v);
    s_n = s_v ? chunk_mesh_build(&s_c, nb, 0.0f, 0.0f, s_v, faces * MESH_FACE_VERTS) : -1;
}

/* The shadow level of corner c of face f of the block at (x, y, z) in the
 * last build, read back from its brightness; -1 if the face or corner is
 * not there. */
static int level(int x, int y, int z, int f, int c)
{
    const signed char *p = mesh_face_corner(f, c);
    float cx = x + 0.5f + 0.5f * k_out[f][0], cy = y + 0.5f + 0.5f * k_out[f][1],
          cz = z + 0.5f + 0.5f * k_out[f][2];
    int i, k, l;

    for (i = 0; i + MESH_FACE_VERTS <= s_n; i += MESH_FACE_VERTS) {
        float sx = 0, sy = 0, sz = 0;

        for (k = 0; k < MESH_FACE_VERTS; k++) {
            sx += s_v[i + k].x;
            sy += s_v[i + k].y;
            sz += s_v[i + k].z;
        }
        if (fabsf(sx / 6 - cx) > 1e-4f || fabsf(sy / 6 - cy) > 1e-4f || fabsf(sz / 6 - cz) > 1e-4f)
            continue;
        for (k = 0; k < MESH_FACE_VERTS; k++) {
            const MeshVertex *v = &s_v[i + k];

            if (v->x != x + p[0] || v->y != y + p[1] || v->z != z + p[2])
                continue;
            for (l = 0; l <= MESH_AO_OPEN; l++)
                if (fabsf(v->r - mesh_face_shade(f) * mesh_ao_scale(l)) < 1e-5f)
                    return l;
        }
    }
    return -1;
}

/* All four corners of a face, as one number: level of corner 0 in the
 * thousands down to corner 3 in the ones (-1 for a missing face). */
static int levels(int x, int y, int z, int f)
{
    int c, r = 0;

    for (c = 0; c < 4; c++) {
        int l = level(x, y, z, f, c);

        if (l < 0)
            return -1;
        r = r * 10 + l;
    }
    return r;
}

static void floor_at(Chunk *c, int y)
{
    int x, z;

    chunk_clear(c);
    for (z = 0; z < CHUNK_SIZE; z++)
        for (x = 0; x < CHUNK_SIZE; x++)
            chunk_set(c, x, y, z, BLOCK_STONE);
}

static void test_chunk_scenes(void)
{
    const Chunk *nb[NB_COUNT];
    const Chunk *none[NB_COUNT] = { NULL };
    int i;

    for (i = 0; i < NB_COUNT; i++) {
        floor_at(&s_nb[i], 10);
        nb[i] = &s_nb[i];
    }

    /* A lone block on a floor (the floor's top is y 10). Top corners
     * (mesh_face_corner order) of the +y face: 0 (x0 z1), 1 (x1 z1),
     * 2 (x1 z0), 3 (x0 z0). */
    floor_at(&s_c, 10);
    chunk_set(&s_c, 8, 11, 8, BLOCK_DIRT);
    build(nb);
    CHECK_EQ_INT(levels(8, 11, 8, FACE_POS_Y), 3333);  /* open sky over it */
    CHECK_EQ_INT(levels(9, 10, 8, FACE_POS_Y), 2332);  /* floor east of it: its west corners */
    CHECK_EQ_INT(levels(7, 10, 8, FACE_POS_Y), 3223);  /* west of it: its east corners */
    CHECK_EQ_INT(levels(8, 10, 9, FACE_POS_Y), 3322);  /* south: its north corners */
    CHECK_EQ_INT(levels(9, 10, 9, FACE_POS_Y), 3332);  /* south-east: across its north-west corner (3) */
    CHECK_EQ_INT(levels(7, 10, 7, FACE_POS_Y), 3233);  /* north-west: its south-east corner (1) */
    CHECK_EQ_INT(levels(5, 10, 5, FACE_POS_Y), 3333);  /* far off: open */
    /* The block's sides: bottom corners see the floor beside and across
     * (1), top corners open. +x corners: 0 (y0 z1), 1 (y0 z0), 2 (y1 z0),
     * 3 (y1 z1). */
    CHECK_EQ_INT(levels(8, 11, 8, FACE_POS_X), 1133);
    CHECK_EQ_INT(levels(8, 11, 8, FACE_NEG_Z), 1133);

    /* An inside corner: walls north and west of a floor cell, nothing
     * across their corner. Both sides solid is 0 whatever is across. */
    floor_at(&s_c, 10);
    chunk_set(&s_c, 4, 11, 5, BLOCK_STONE); /* west of (5, 10, 5)'s top */
    chunk_set(&s_c, 5, 11, 4, BLOCK_STONE); /* north of it */
    build(nb);
    CHECK_EQ_INT(levels(5, 10, 5, FACE_POS_Y), 2320);
    chunk_set(&s_c, 4, 11, 4, BLOCK_STONE); /* across the corner too: still 0 */
    build(nb);
    CHECK_EQ_INT(levels(5, 10, 5, FACE_POS_Y), 2320);

    /* Across a chunk's corner. One block on the chunk's north-west corner
     * cell; the chunk north-west of it has a block one up, across that
     * corner of its top. */
    chunk_clear(&s_c);
    for (i = 0; i < NB_COUNT; i++)
        chunk_clear(&s_nb[i]);
    chunk_set(&s_c, 0, 20, 0, BLOCK_STONE);
    chunk_set(&s_nb[NB_NEG_X_NEG_Z], CHUNK_SIZE - 1, 21, CHUNK_SIZE - 1, BLOCK_STONE);
    build(nb);
    CHECK_EQ_INT(levels(0, 20, 0, FACE_POS_Y), 3332);
    build(none); /* that neighbour missing: air, so open */
    CHECK_EQ_INT(levels(0, 20, 0, FACE_POS_Y), 3333);
    build(NULL);
    CHECK_EQ_INT(levels(0, 20, 0, FACE_POS_Y), 3333);
    /* The same through each other corner chunk and its matching corner. */
    chunk_clear(&s_nb[NB_NEG_X_NEG_Z]);
    chunk_set(&s_c, CHUNK_SIZE - 1, 20, CHUNK_SIZE - 1, BLOCK_STONE);
    chunk_set(&s_nb[NB_POS_X_POS_Z], 0, 21, 0, BLOCK_STONE);
    chunk_set(&s_c, CHUNK_SIZE - 1, 30, 0, BLOCK_STONE);
    chunk_set(&s_nb[NB_POS_X_NEG_Z], 0, 31, CHUNK_SIZE - 1, BLOCK_STONE);
    chunk_set(&s_c, 0, 40, CHUNK_SIZE - 1, BLOCK_STONE);
    chunk_set(&s_nb[NB_NEG_X_POS_Z], CHUNK_SIZE - 1, 41, 0, BLOCK_STONE);
    build(nb);
    CHECK_EQ_INT(levels(CHUNK_SIZE - 1, 20, CHUNK_SIZE - 1, FACE_POS_Y), 3233);
    CHECK_EQ_INT(levels(CHUNK_SIZE - 1, 30, 0, FACE_POS_Y), 3323);
    CHECK_EQ_INT(levels(0, 40, CHUNK_SIZE - 1, FACE_POS_Y), 2333);
    /* And across a side: the west chunk's block beside the top's west edge. */
    chunk_set(&s_nb[NB_NEG_X], CHUNK_SIZE - 1, 21, 0, BLOCK_STONE);
    build(nb);
    CHECK_EQ_INT(levels(0, 20, 0, FACE_POS_Y), 2332); /* corners 0 and 3: a side each */

    /* The bottom of the world counts as solid, the top as open. */
    chunk_clear(&s_c);
    chunk_set(&s_c, 5, 0, 5, BLOCK_STONE);
    chunk_set(&s_c, 5, WORLD_HEIGHT - 1, 5, BLOCK_STONE);
    build(none);
    CHECK_EQ_INT(levels(5, 0, 5, FACE_POS_X), 1133);
    CHECK_EQ_INT(levels(5, WORLD_HEIGHT - 1, 5, FACE_POS_Y), 3333);
    CHECK_EQ_INT(levels(5, WORLD_HEIGHT - 1, 5, FACE_POS_X), 3333);

    free(s_v);
    s_v = NULL;
}

void test_ao(void)
{
    test_mesh_ao();
    test_chunk_scenes();
}
