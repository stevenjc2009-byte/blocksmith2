#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "alpha_test.h"
#include "biome.h"
#include "block.h"
#include "blocktex.h"
#include "chunk.h"
#include "chunk_mesh.h"
#include "mesh.h"
#include "raycast.h"
#include "test.h"
#include "world.h"

/* Row 4-03: what the new blocks are to the game (solid or not, what a ray and
 * a break do with them), how the alpha cut is set up, and the meshes of
 * plants and leaves. */

static Chunk s_c;
static World s_w;
static const Chunk *const k_none[NB_COUNT] = { NULL };

static void test_solidity(void)
{
    int b, plants = 0, solid_new = 0, bad = 0;

    for (b = 0; b < BLOCK_COUNT; b++) {
        int is_new_solid = b == BLOCK_LOG || (b >= BLOCK_BIRCH_LOG && b < BLOCK_SHORT_GRASS);
        int is_plant = b >= BLOCK_SHORT_GRASS;

        bad += block_is_plant(b) != is_plant;
        if (is_new_solid) {
            bad += !block_solid(b);
            solid_new++;
        } else if (is_plant) {
            bad += block_solid(b); /* walk through it, it hides nothing */
            plants++;
        }
    }
    CHECK_EQ_INT(bad, 0);
    CHECK_EQ_INT(plants, 7);
    CHECK_EQ_INT(solid_new, 13); /* the oak log, 5 more logs, 6 leaves, the cactus */
    CHECK_EQ_INT(block_solid(BLOCK_AIR) || block_solid(BLOCK_WATER) || block_solid(BLOCK_TORCH), 0);
    CHECK_EQ_INT(block_solid(BLOCK_COUNT), 0);
    /* The faces show the right tiles. */
    CHECK_EQ_INT(block_face_tile(BLOCK_BIRCH_LOG, FACE_POS_X), TILE_BIRCH_LOG_SIDE);
    CHECK_EQ_INT(block_face_tile(BLOCK_BIRCH_LOG, FACE_POS_Y), TILE_BIRCH_LOG_TOP);
    CHECK_EQ_INT(block_face_tile(BLOCK_ACACIA_LOG, FACE_NEG_Y), TILE_ACACIA_LOG_TOP);
    CHECK_EQ_INT(block_face_tile(BLOCK_OAK_LEAVES, FACE_POS_Z), TILE_LEAVES);
    CHECK_EQ_INT(block_face_tile(BLOCK_SPRUCE_LEAVES, FACE_POS_Y), TILE_LEAVES_CONIFER);
    CHECK_EQ_INT(block_face_tile(BLOCK_CACTUS, FACE_POS_X), TILE_CACTUS_SIDE);
    CHECK_EQ_INT(block_face_tile(BLOCK_CACTUS, FACE_POS_Y), TILE_CACTUS_TOP);
    CHECK_EQ_INT(block_face_tile(BLOCK_DANDELION, FACE_POS_X), TILE_DANDELION);
    CHECK_EQ_INT(block_face_tile(BLOCK_DEAD_BUSH, FACE_POS_Y), TILE_DEAD_BUSH);
}

/* The alpha cut: what the GPU is told, and what the tiles hold. */
static void test_alpha(void)
{
    static uint32_t atlas[BLOCKTEX_SIZE * BLOCKTEX_SIZE];
    int t, x, y, bad = 0;

    CHECK_EQ_INT((int)ALPHA_TEST_WORD, 0x8071); /* on, greater or equal, 0x80 */
    CHECK_EQ_INT(ALPHA_TEST_REF > 0 && ALPHA_TEST_REF <= BLOCKTEX_WATER_ALPHA, 1); /* the water passes */
    blocktex_build(atlas);
    for (t = 0; t < TILE_COUNT; t++) {
        int clear = 0, solid = 0;

        for (y = 0; y < BLOCKTEX_TILE; y++)
            for (x = 0; x < BLOCKTEX_TILE; x++) {
                unsigned a = atlas[((t / BLOCKTEX_PER_ROW) * BLOCKTEX_TILE + y) * BLOCKTEX_SIZE +
                                   (t % BLOCKTEX_PER_ROW) * BLOCKTEX_TILE + x] & 0xFF;

                clear += a < ALPHA_TEST_REF;
                solid += a >= ALPHA_TEST_REF;
                if (t >= TILE_SHORT_GRASS)
                    bad += a != 0 && a != 0xFF; /* a plant texel is in or out, never half */
            }
        if (t >= TILE_SHORT_GRASS) { /* a plant: some picture, mostly see-through */
            bad += solid < 8 || clear < 100;
        } else if (t != TILE_WATER) { /* the leaves and logs among them: every texel drawn */
            bad += clear != 0;
        }
    }
    CHECK_EQ_INT(bad, 0);
}

/* The mesh of a lone block on a floor, and the checks on it. */
static void clear_with_floor(int biome)
{
    int x, z;

    chunk_clear(&s_c);
    for (z = 0; z < CHUNK_SIZE; z++)
        for (x = 0; x < CHUNK_SIZE; x++) {
            chunk_set(&s_c, x, 0, z, BLOCK_STONE);
            s_c.biome[z][x] = (uint8_t)biome;
        }
}

#define MAXF 400 /* faces of a floor and a block or two */

static int build(MeshVertex *out, int cap)
{
    return chunk_mesh_build(&s_c, k_none, 0.0f, 0.0f, out, cap);
}

static void sub3(const MeshVertex *a, const MeshVertex *b, float d[3])
{
    d[0] = a->x - b->x;
    d[1] = a->y - b->y;
    d[2] = a->z - b->z;
}

static void test_plant_mesh(void)
{
    static MeshVertex v[MAXF * MESH_FACE_VERTS], ref[MESH_PLANT_VERTS];
    int plant, q, k, n, bad = 0, floor_faces;

    for (plant = BLOCK_SHORT_GRASS; plant <= BLOCK_DEAD_BUSH; plant++) {
        int biome = plant == BLOCK_FERN ? BIOME_SWAMP : BIOME_JUNGLE;

        clear_with_floor(biome);
        floor_faces = chunk_mesh_count(&s_c, k_none);
        chunk_set(&s_c, 5, 1, 7, plant);
        /* A plant is exactly its crossed quads, and hides no face of what is under it. */
        CHECK_EQ_INT(chunk_mesh_count(&s_c, k_none), floor_faces + MESH_PLANT_FACES);
        n = build(v, MAXF * MESH_FACE_VERTS);
        CHECK_EQ_INT(n, (floor_faces + MESH_PLANT_FACES) * MESH_FACE_VERTS);
        /* They are the last vertices (the layer above the floor). */
        mesh_plant(ref, 5.0f, 1.0f, 7.0f, block_face_tile(plant, 0), 0.0f);
        test_ref_tree_tint(ref, MESH_PLANT_VERTS, plant, biome);
        bad += memcmp(v + n - MESH_PLANT_VERTS, ref, sizeof ref) != 0;
        /* The shape: four vertical quads in the cell, two on each diagonal, the
         * two of a diagonal facing opposite ways (the GPU draws one side). */
        for (q = 0; q < MESH_PLANT_FACES; q++) {
            const MeshVertex *f = ref + q * MESH_FACE_VERTS;
            float a[3], b[3], nrm[3];

            sub3(&f[1], &f[0], a);
            sub3(&f[2], &f[0], b);
            nrm[0] = a[1] * b[2] - a[2] * b[1];
            nrm[1] = a[2] * b[0] - a[0] * b[2];
            nrm[2] = a[0] * b[1] - a[1] * b[0];
            bad += fabsf(nrm[1]) > 1e-4f || (fabsf(nrm[0]) < 1e-4f && fabsf(nrm[2]) < 1e-4f);
            for (k = 0; k < MESH_FACE_VERTS; k++)
                bad += f[k].x < 5.0f || f[k].x > 6.0f || f[k].z < 7.0f || f[k].z > 8.0f || f[k].y < 1.0f ||
                       f[k].y > 2.0f;
            if (q > 0) { /* same plane (parallel normals), the pair opposite */
                const MeshVertex *g = ref + (q ^ 1) * MESH_FACE_VERTS;
                float c[3], d[3], n2[3];

                sub3(&g[1], &g[0], c);
                sub3(&g[2], &g[0], d);
                n2[0] = c[1] * d[2] - c[2] * d[1];
                n2[2] = c[0] * d[1] - c[1] * d[0];
                if (q == 1 || q == 3)
                    bad += nrm[0] * n2[0] + nrm[2] * n2[2] >= 0.0f;
            }
        }
    }
    CHECK_EQ_INT(bad, 0);
    /* A plant's neighbours show their faces: a cube beside it keeps all six. */
    clear_with_floor(BIOME_PLAINS);
    floor_faces = chunk_mesh_count(&s_c, k_none);
    chunk_set(&s_c, 5, 1, 7, BLOCK_DANDELION);
    chunk_set(&s_c, 6, 1, 7, BLOCK_STONE);
    CHECK_EQ_INT(chunk_mesh_count(&s_c, k_none), floor_faces + 5 + 4 - 1); /* stone's 5, the plant's 4, the floor's lost top */
}

/* Leaf faces: leaf against leaf is never drawn, a leaf in the open draws its
 * six, each carrying the biome's foliage colour (birch and spruce fixed). */
static void test_leaf_mesh(void)
{
    static MeshVertex v[MAXF * MESH_FACE_VERTS], ref[MESH_FACE_VERTS];
    static const int k_biomes[] = { BIOME_PLAINS, BIOME_JUNGLE, BIOME_SWAMP, BIOME_DESERT, BIOME_TAIGA };
    int bi, leaf, f, bad = 0, same = 0, n, floor_faces;
    float first[3] = { 0, 0, 0 };

    clear_with_floor(BIOME_PLAINS);
    chunk_set(&s_c, 4, 5, 4, BLOCK_OAK_LEAVES);
    chunk_set(&s_c, 5, 5, 4, BLOCK_OAK_LEAVES);
    chunk_set(&s_c, 4, 6, 4, BLOCK_BIRCH_LEAVES);
    /* Three leaves in a row and a stack: 3 * 6 - 2 * 2 shared faces. */
    floor_faces = 0;
    CHECK_EQ_INT(chunk_mesh_count(&s_c, k_none) - 320, 3 * 6 - 2 * 2); /* the bare floor: 320 faces */
    for (bi = 0; bi < 5; bi++)
        for (leaf = BLOCK_OAK_LEAVES; leaf <= BLOCK_ACACIA_LEAVES; leaf++) {
            unsigned char ao[4] = { MESH_AO_OPEN, MESH_AO_OPEN, MESH_AO_OPEN, MESH_AO_OPEN };
            static const float dark[4] = { 0, 0, 0, 0 };

            clear_with_floor(k_biomes[bi]);
            chunk_set(&s_c, 8, 40, 8, leaf);
            /* (the floor is at y 0, far under: the leaf's corners are all open) */
            n = build(v, MAXF * MESH_FACE_VERTS);
            floor_faces += n != 326 * MESH_FACE_VERTS; /* the floor's 320 faces and the leaf's 6 */
            /* the floor's top faces come first (y 0): the leaf's six are the last */
            for (f = 0; f < FACE_COUNT; f++) {
                mesh_face_lit(ref, 8.0f, 40.0f, 8.0f, f, block_face_tile(leaf, f), ao, dark);
                test_ref_tree_tint(ref, MESH_FACE_VERTS, leaf, k_biomes[bi]);
                bad += memcmp(v + n - (FACE_COUNT - f) * MESH_FACE_VERTS, ref, sizeof ref) != 0;
            }
            if (leaf == BLOCK_OAK_LEAVES) { /* the colour really is a tint: it moves with the biome */
                float col[3] = { v[n - 1].r, v[n - 1].g, v[n - 1].b };

                if (bi == 0)
                    memcpy(first, col, sizeof first);
                else
                    same += col[0] == first[0] && col[1] == first[1] && col[2] == first[2];
            }
        }
    CHECK_EQ_INT(bad, 0);
    CHECK_EQ_INT(floor_faces, 0);
    CHECK_EQ_INT(same <= 1, 1); /* only two of the biomes may share a foliage colour */
    /* Birch and spruce keep one colour in every biome. */
    clear_with_floor(BIOME_PLAINS);
    chunk_set(&s_c, 8, 40, 8, BLOCK_BIRCH_LEAVES);
    n = build(v, MAXF * MESH_FACE_VERTS);
    first[0] = v[n - 1].r;
    clear_with_floor(BIOME_SWAMP);
    chunk_set(&s_c, 8, 40, 8, BLOCK_BIRCH_LEAVES);
    n = build(v, MAXF * MESH_FACE_VERTS);
    CHECK_EQ_INT(v[n - 1].r == first[0], 1);
}

/* A ray stops at a plant, and a plant goes with the block it stood on. */
static void test_world_rules(void)
{
    RayHit h;
    int x, y, z;

    test_world_cleared(&s_w);
    for (x = 0; x < 6; x++)
        for (z = 0; z < 6; z++)
            for (y = 0; y <= 60; y++)
                world_set(&s_w, x, y, z, BLOCK_STONE);
    world_set(&s_w, 2, 61, 2, BLOCK_POPPY);
    world_set(&s_w, 4, 61, 4, BLOCK_SHORT_GRASS);
    CHECK_EQ_INT(raycast(&s_w, 2.5f, 64.5f, 2.5f, 0.0f, -1.0f, 0.0f, RAY_REACH, &h), 1);
    CHECK_EQ_INT(h.y, 61); /* the poppy, not the stone under it */
    CHECK_EQ_INT(h.face, FACE_POS_Y);
    CHECK_EQ_INT(raycast(&s_w, 0.5f, 64.5f, 0.5f, 0.0f, -1.0f, 0.0f, RAY_REACH, &h) && h.y == 60, 1); /* bare ground */
    /* The plant can be broken, and goes when its ground does. */
    CHECK_EQ_INT(world_break(&s_w, 2, 61, 2), 1);
    CHECK_EQ_INT(world_get(&s_w, 2, 61, 2), BLOCK_AIR);
    CHECK_EQ_INT(world_break(&s_w, 4, 60, 4), 1);
    CHECK_EQ_INT(world_get(&s_w, 4, 60, 4), BLOCK_AIR);
    CHECK_EQ_INT(world_get(&s_w, 4, 61, 4), BLOCK_AIR);
    /* A log or leaf above a broken block stays (only a plant or a torch goes). */
    world_set(&s_w, 1, 61, 1, BLOCK_LOG);
    CHECK_EQ_INT(world_break(&s_w, 1, 60, 1), 1);
    CHECK_EQ_INT(world_get(&s_w, 1, 61, 1), BLOCK_LOG);
}

/* A plant in a pocket deep in full ground (merged with row 4-04): the mesh
 * must not skip its layer as buried (chunk_mesh.c first_air_layer()). Each
 * plant, in each byte of the four first_air_layer() reads at once. */
static void test_buried_plant(void)
{
    static Chunk full;
    const Chunk *nb[NB_COUNT];
    int i, x, y, z, b, base, bad = 0;

    chunk_clear(&full);
    for (y = 0; y <= 40; y++)
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++)
                chunk_set(&full, x, y, z, BLOCK_STONE);
    for (i = 0; i < NB_COUNT; i++)
        nb[i] = &full;
    s_c = full;
    base = chunk_mesh_count(&s_c, nb);
    CHECK_EQ_INT(base, CHUNK_SIZE * CHUNK_SIZE); /* only the tops */
    for (b = BLOCK_SHORT_GRASS; b < BLOCK_COUNT; b++) {
        x = 4 + b % 4;
        chunk_set(&s_c, x, 20, 5, b);
        bad += chunk_mesh_count(&s_c, nb) != base + 6 + MESH_PLANT_FACES;
        chunk_set(&s_c, x, 20, 5, BLOCK_STONE);
    }
    CHECK_EQ_INT(bad, 0);
}

void test_trees_mesh(void)
{
    test_solidity();
    test_alpha();
    test_plant_mesh();
    test_leaf_mesh();
    test_world_rules();
    test_buried_plant();
}
