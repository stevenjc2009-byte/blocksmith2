#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "chunk.h"
#include "chunk_mesh.h"
#include "mesh.h"
#include "terrain.h"
#include "test.h"
#include "world.h"

#define LO (SMALL_MIN * CHUNK_SIZE)                 /* first block, -32 */
#define HI ((SMALL_MIN + SMALL_CHUNKS) * CHUNK_SIZE) /* one past the last, 32 */

static World s_w;
static const uint32_t k_seeds[] = { 2026u, 0xFFFFFFFFu };

#define C(cx, cz) test_slot_chunk(&s_w, (cx), (cz))

void test_world_small(World *w, uint32_t seed)
{
    int cx, cz;

    memset(w, 0xA5, sizeof *w); /* whatever was there before must not matter */
    world_init(w, seed);
    for (cz = SMALL_MIN; cz < SMALL_MIN + SMALL_CHUNKS; cz++)
        for (cx = SMALL_MIN; cx < SMALL_MIN + SMALL_CHUNKS; cx++)
            world_load(w, cx, cz);
}

void test_world_cleared(World *w)
{
    int cx, cz;

    test_world_small(w, 0u);
    for (cz = SMALL_MIN; cz < SMALL_MIN + SMALL_CHUNKS; cz++)
        for (cx = SMALL_MIN; cx < SMALL_MIN + SMALL_CHUNKS; cx++)
            chunk_clear(test_slot_chunk(w, cx, cz));
}

Chunk *test_slot_chunk(World *w, int cx, int cz)
{
    return &w->slots[world_slot(cz)][world_slot(cx)].chunk;
}

static void test_slots(void)
{
    CHECK_EQ_INT(world_slot(0), 0);
    CHECK_EQ_INT(world_slot(8), 8);
    CHECK_EQ_INT(world_slot(9), 0);
    CHECK_EQ_INT(world_slot(10), 1);
    CHECK_EQ_INT(world_slot(-1), 8);
    CHECK_EQ_INT(world_slot(-9), 0);
    CHECK_EQ_INT(world_slot(-10), 8);
    CHECK_EQ_INT(world_slot(62501), 62501 % 9);
    CHECK_EQ_INT(world_slot(-62501), 9 - 62501 % 9);
    CHECK_EQ_INT(world_slot(-2147483647 - 1), 7);
    CHECK_EQ_INT(world_slot(2147483647), 1);
}

/* Loading, putting away, a chunk taking another's slot, and what reads and
 * writes do where nothing is loaded. */
static void test_load(void)
{
    static Chunk c;
    const Chunk *nb[NB_COUNT];
    int i, j, loaded = 0;

    memset(&s_w, 0xA5, sizeof s_w);
    world_init(&s_w, 77u);
    for (j = 0; j < WORLD_SLOTS; j++)
        for (i = 0; i < WORLD_SLOTS; i++)
            loaded += s_w.slots[j][i].loaded;
    CHECK_EQ_INT(loaded, 0);
    CHECK_EQ_INT(world_chunk(&s_w, 0, 0) == NULL, 1);
    CHECK_EQ_INT(world_get(&s_w, 3, 0, 3), BLOCK_AIR);
    world_set(&s_w, 3, 0, 3, BLOCK_DIRT); /* ignored: nothing loaded */
    CHECK_EQ_INT(world_break(&s_w, 3, 5, 3), 0);

    world_load(&s_w, 5, -3);
    terrain_fill_chunk(&c, 77u, 5, -3);
    CHECK_EQ_INT(world_chunk(&s_w, 5, -3) == C(5, -3), 1);
    CHECK_EQ_INT(memcmp(world_chunk(&s_w, 5, -3), &c, sizeof c), 0);
    CHECK_EQ_INT(world_get(&s_w, 5 * CHUNK_SIZE, 0, -3 * CHUNK_SIZE), BLOCK_STONE);
    /* Chunk (14, -3) shares the slot: loading it puts (5, -3) away. */
    world_load(&s_w, 14, -3);
    terrain_fill_chunk(&c, 77u, 14, -3);
    CHECK_EQ_INT(world_chunk(&s_w, 5, -3) == NULL, 1);
    CHECK_EQ_INT(world_chunk(&s_w, 14, -3) == C(5, -3), 1);
    CHECK_EQ_INT(memcmp(world_chunk(&s_w, 14, -3), &c, sizeof c), 0);
    CHECK_EQ_INT(world_get(&s_w, 5 * CHUNK_SIZE, 0, -3 * CHUNK_SIZE), BLOCK_AIR);
    /* Putting away a chunk that is not there leaves the slot alone. */
    world_unload(&s_w, 5, -3);
    world_unload(&s_w, 14, 6);
    CHECK_EQ_INT(world_chunk(&s_w, 14, -3) != NULL, 1);
    world_unload(&s_w, 14, -3);
    CHECK_EQ_INT(world_chunk(&s_w, 14, -3) == NULL, 1);
    CHECK_EQ_INT(world_get(&s_w, 14 * CHUNK_SIZE, 0, -3 * CHUNK_SIZE), BLOCK_AIR);
    world_set(&s_w, 14 * CHUNK_SIZE, 100, -3 * CHUNK_SIZE, BLOCK_STONE);
    world_load(&s_w, 14, -3); /* the write above went nowhere */
    CHECK_EQ_INT(memcmp(world_chunk(&s_w, 14, -3), &c, sizeof c), 0);

    /* A neighbour's slot holding some other chunk is not a neighbour. */
    world_load(&s_w, 0, 0);
    world_load(&s_w, 10, 0); /* slot of chunk (1, 0) */
    world_load(&s_w, -1, 0);
    world_load(&s_w, 0, 10); /* slot of chunk (0, 1) */
    CHECK_EQ_INT(world_chunk(&s_w, 0, 0) == C(0, 0), 1);
    world_neighbours(&s_w, 0, 0, nb);
    CHECK_EQ_INT(nb[NB_POS_X] == NULL, 1);
    CHECK_EQ_INT(nb[NB_NEG_X] == C(-1, 0), 1);
    CHECK_EQ_INT(nb[NB_POS_Z] == NULL, 1);
    CHECK_EQ_INT(nb[NB_NEG_Z] == NULL, 1);
    CHECK_EQ_INT(world_get(&s_w, CHUNK_SIZE, 0, 0), BLOCK_AIR);
    CHECK_EQ_INT(world_get(&s_w, 10 * CHUNK_SIZE, 0, 0), BLOCK_STONE);
    CHECK_EQ_INT(world_get(&s_w, 0, 0, CHUNK_SIZE), BLOCK_AIR);
    CHECK_EQ_INT(world_get(&s_w, 0, 0, 10 * CHUNK_SIZE), BLOCK_STONE);
}

static void test_generate(uint32_t seed)
{
    static Chunk c;
    int x, z, cx, cz, wrong = 0, chunks_wrong = 0;

    test_world_small(&s_w, seed);
    CHECK_EQ_INT((int)(s_w.seed == seed), 1);
    for (z = LO; z < HI; z++)
        for (x = LO; x < HI; x++) {
            int h = terrain_height(seed, x, z);

            wrong += world_get(&s_w, x, h, z) != BLOCK_GRASS;
            wrong += world_get(&s_w, x, h + 1, z) != BLOCK_AIR;
            wrong += world_get(&s_w, x, h - 1, z) != BLOCK_DIRT;
            wrong += world_get(&s_w, x, 0, z) != BLOCK_STONE;
        }
    CHECK_EQ_INT(wrong, 0);
    for (cz = SMALL_MIN; cz < SMALL_MIN + SMALL_CHUNKS; cz++)
        for (cx = SMALL_MIN; cx < SMALL_MIN + SMALL_CHUNKS; cx++) {
            const Chunk *got = world_chunk(&s_w, cx, cz);

            terrain_fill_chunk(&c, seed, cx, cz);
            chunks_wrong += got == NULL || memcmp(got, &c, sizeof c) != 0;
        }
    CHECK_EQ_INT(chunks_wrong, 0);
}

static void test_edges(void)
{
    /* Corners inside, one step past every side outside. */
    CHECK_EQ_INT(world_get(&s_w, LO, 0, LO), BLOCK_STONE);
    CHECK_EQ_INT(world_get(&s_w, HI - 1, 0, HI - 1), BLOCK_STONE);
    CHECK_EQ_INT(world_get(&s_w, HI, 0, 0), BLOCK_AIR);
    CHECK_EQ_INT(world_get(&s_w, LO - 1, 0, 0), BLOCK_AIR);
    CHECK_EQ_INT(world_get(&s_w, 0, 0, HI), BLOCK_AIR);
    CHECK_EQ_INT(world_get(&s_w, 0, 0, LO - 1), BLOCK_AIR);
    CHECK_EQ_INT(world_get(&s_w, 0, -1, 0), BLOCK_AIR);
    CHECK_EQ_INT(world_get(&s_w, 0, WORLD_HEIGHT, 0), BLOCK_AIR);

    CHECK_EQ_INT(world_chunk(&s_w, SMALL_MIN, SMALL_MIN) == C(SMALL_MIN, SMALL_MIN), 1);
    CHECK_EQ_INT(world_chunk(&s_w, SMALL_MIN + 3, SMALL_MIN) == C(SMALL_MIN + 3, SMALL_MIN), 1);
    CHECK_EQ_INT(world_chunk(&s_w, SMALL_MIN, SMALL_MIN + 3) == C(SMALL_MIN, SMALL_MIN + 3), 1);
    CHECK_EQ_INT(world_chunk(&s_w, SMALL_MIN - 1, 0) == NULL, 1);
    CHECK_EQ_INT(world_chunk(&s_w, SMALL_MIN + SMALL_CHUNKS, 0) == NULL, 1);
    CHECK_EQ_INT(world_chunk(&s_w, 0, SMALL_MIN - 1) == NULL, 1);
    CHECK_EQ_INT(world_chunk(&s_w, 0, SMALL_MIN + SMALL_CHUNKS) == NULL, 1);
}

/* world_set changes only the one block it names, in the right chunk, and
 * ignores spots outside the world. Run last: it changes the world. */
static void test_set(void)
{
    int before = world_get(&s_w, 6, 50, 6);

    world_set(&s_w, LO, 100, LO, BLOCK_STONE);
    world_set(&s_w, HI - 1, 101, -17, BLOCK_DIRT);
    CHECK_EQ_INT(world_get(&s_w, LO, 100, LO), BLOCK_STONE);
    CHECK_EQ_INT(world_get(&s_w, HI - 1, 101, -17), BLOCK_DIRT);
    CHECK_EQ_INT(chunk_get(C(SMALL_MIN, SMALL_MIN), 0, 100, 0), BLOCK_STONE);
    CHECK_EQ_INT(chunk_get(C(SMALL_MIN + 3, SMALL_MIN), CHUNK_SIZE - 1, 101, CHUNK_SIZE - 1), BLOCK_DIRT);
    CHECK_EQ_INT(world_get(&s_w, LO + 1, 100, LO), BLOCK_AIR);
    CHECK_EQ_INT(world_get(&s_w, HI - 1, 101, -16), BLOCK_AIR);
    world_set(&s_w, LO, 100, LO, BLOCK_AIR);
    CHECK_EQ_INT(world_get(&s_w, LO, 100, LO), BLOCK_AIR);
    world_set(&s_w, HI, 50, 6, BLOCK_STONE);
    world_set(&s_w, 6, 50, LO - 1, BLOCK_STONE);
    CHECK_EQ_INT(world_get(&s_w, HI, 50, 6), BLOCK_AIR);
    CHECK_EQ_INT(world_get(&s_w, 6, 50, 6), before);
}

static void test_neighbours(void)
{
    const Chunk *nb[NB_COUNT];

    /* Chunk (-1, -1): all four neighbours are there. */
    world_neighbours(&s_w, -1, -1, nb);
    CHECK_EQ_INT(nb[NB_POS_X] == C(0, -1), 1);
    CHECK_EQ_INT(nb[NB_NEG_X] == C(-2, -1), 1);
    CHECK_EQ_INT(nb[NB_POS_Z] == C(-1, 0), 1);
    CHECK_EQ_INT(nb[NB_NEG_Z] == C(-1, -2), 1);
    /* North-west corner (-2, -2): nothing west or north. */
    world_neighbours(&s_w, -2, -2, nb);
    CHECK_EQ_INT(nb[NB_POS_X] == C(-1, -2), 1);
    CHECK_EQ_INT(nb[NB_NEG_X] == NULL, 1);
    CHECK_EQ_INT(nb[NB_POS_Z] == C(-2, -1), 1);
    CHECK_EQ_INT(nb[NB_NEG_Z] == NULL, 1);
    /* South-east corner (1, 1): nothing east or south. */
    world_neighbours(&s_w, 1, 1, nb);
    CHECK_EQ_INT(nb[NB_POS_X] == NULL, 1);
    CHECK_EQ_INT(nb[NB_NEG_X] == C(0, 1), 1);
    CHECK_EQ_INT(nb[NB_POS_Z] == NULL, 1);
    CHECK_EQ_INT(nb[NB_NEG_Z] == C(1, 0), 1);
}

/* Separate reference: is world cell (x, y, z) solid, for face hiding? */
static int ref_solid(int x, int y, int z)
{
    if (y < 0)
        return 1;
    if (y >= WORLD_HEIGHT)
        return 0;
    return world_get(&s_w, x, y, z) != BLOCK_AIR;
}

static const int k_step[FACE_COUNT][3] = {
    { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 },
};

/* The faces of chunk (cx, cz) worked out from whole-world blocks. */
static int ref_build(int cx, int cz, MeshVertex *out)
{
    int x, y, z, f, n = 0;

    for (y = 0; y < WORLD_HEIGHT; y++)
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++) {
                int wx = cx * CHUNK_SIZE + x, wz = cz * CHUNK_SIZE + z;
                int b = world_get(&s_w, wx, y, wz);

                if (b == BLOCK_AIR)
                    continue;
                for (f = 0; f < FACE_COUNT; f++)
                    if (!ref_solid(wx + k_step[f][0], y + k_step[f][1], wz + k_step[f][2])) {
                        mesh_face(out + n, (float)wx, (float)y, (float)wz, f, block_face_tile(b, f));
                        n += MESH_FACE_VERTS;
                    }
            }
    return n;
}

static int min_int(int a, int b)
{
    return a < b ? a : b;
}

static void test_meshes(uint32_t seed)
{
    static const Chunk *const k_none[NB_COUNT] = { NULL, NULL, NULL, NULL };
    int cx, cz, x, z, wrong = 0, with_nb = 0, alone = 0, seam = 0;

    for (cz = SMALL_MIN; cz < SMALL_MIN + SMALL_CHUNKS; cz++)
        for (cx = SMALL_MIN; cx < SMALL_MIN + SMALL_CHUNKS; cx++) {
            const Chunk *c = world_chunk(&s_w, cx, cz);
            const Chunk *nb[NB_COUNT];
            int faces, n, rn;
            MeshVertex *got, *ref;

            if (c == NULL) { /* a chunk of the world went missing */
                wrong++;
                continue;
            }
            world_neighbours(&s_w, cx, cz, nb);
            faces = chunk_mesh_count(c, nb);
            got = malloc((size_t)faces * MESH_FACE_VERTS * sizeof *got);
            ref = malloc((size_t)CHUNK_SIZE * CHUNK_SIZE * WORLD_HEIGHT * MESH_BLOCK_VERTS * sizeof *ref);
            n = chunk_mesh_build(c, nb, (float)(cx * CHUNK_SIZE), (float)(cz * CHUNK_SIZE), got,
                                 faces * MESH_FACE_VERTS);
            rn = ref_build(cx, cz, ref);
            wrong += n != rn || n != faces * MESH_FACE_VERTS ||
                     memcmp(got, ref, (size_t)(n > 0 ? n : 0) * sizeof *got) != 0;
            with_nb += faces;
            alone += chunk_mesh_count(c, k_none);
            free(got);
            free(ref);
        }
    CHECK_EQ_INT(wrong, 0);
    /* Without neighbours each chunk would also draw the faces at every
     * inside seam: on both sides of a seam, one per block that has a block
     * across from it, so 2 * (lower column + 1) per column pair. */
    for (x = LO + CHUNK_SIZE; x < HI; x += CHUNK_SIZE)
        for (z = LO; z < HI; z++)
            seam += 2 * (min_int(terrain_height(seed, x - 1, z), terrain_height(seed, x, z)) + 1);
    for (z = LO + CHUNK_SIZE; z < HI; z += CHUNK_SIZE)
        for (x = LO; x < HI; x++)
            seam += 2 * (min_int(terrain_height(seed, x, z - 1), terrain_height(seed, x, z)) + 1);
    CHECK_EQ_INT(alone - with_nb, seam);
}

void test_world(void)
{
    int i;

    test_slots();
    test_load();
    for (i = 0; i < (int)(sizeof k_seeds / sizeof k_seeds[0]); i++) {
        test_generate(k_seeds[i]);
        test_edges();
        test_neighbours();
        test_meshes(k_seeds[i]);
        test_set();
    }
}
