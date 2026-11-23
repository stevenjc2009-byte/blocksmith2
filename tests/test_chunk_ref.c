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
    if (o == NULL)
        return 0;
    x = chunk_get(o, x, y, z);
    return x == BLOCK_GRASS || x == BLOCK_DIRT || x == BLOCK_STONE ||
           x == BLOCK_SAND; /* not air, a torch or water */
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

/* Row 3-03's torch light at corner `corner` of face `face` of the block at
 * (bx, by, bz), from the rule in chunk_mesh.h, levels from test_ref_level()
 * (test_light.c, filled for this chunk first). In the layer in front of
 * the face, the corner's point touches the cell in front, one beside it
 * along each of the two other axes (lower axis first) and one across. */
static float ref_corner_light(const RefCtx *r, int bx, int by, int bz, int face, int corner)
{
    const signed char *p = mesh_face_corner(face, corner);
    int v[3] = { bx + p[0], by + p[1], bz + p[2] };
    int front[3] = { bx + k_step[face][0], by + k_step[face][1], bz + k_step[face][2] };
    int cell[4][3], open[4], ax[2], t, k = 0, i;
    float sum = 0.0f;
    int n = 0;

    for (t = 0; t < 3; t++)
        if (t != face / 2)
            ax[k++] = t;
    for (i = 0; i < 4; i++) {
        memcpy(cell[i], front, sizeof front);
        for (k = 0; k < 2; k++)
            if ((i == 1 && k == 0) || (i == 2 && k == 1) || i == 3) /* 1, 2 sides; 3 across */
                cell[i][ax[k]] += v[ax[k]] == front[ax[k]] ? -1 : 1;
        open[i] = !ref_solid(r->c, r->nb, cell[i][0], cell[i][1], cell[i][2]);
    }
    if (!open[1] && !open[2])
        open[3] = 0; /* both sides solid: the one across is left out */
    for (i = 0; i < 4; i++)
        if (i == 0 || open[i]) {
            sum += (float)test_ref_level(cell[i][0], cell[i][1], cell[i][2]) / 14.0f;
            n++;
        }
    return sum / (float)n;
}

static unsigned s_rng;
static int s_lit; /* vertices of the last reference mesh with any torch light */

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

    if (out) {
        test_ref_light(&s_c, nb);
        s_lit = 0;
    }
    for (y = 0; y < WORLD_HEIGHT; y++)
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++) {
                int b = chunk_get(&s_c, x, y, z);

                if (b == BLOCK_AIR || b == BLOCK_WATER) /* water: its own mesh (row 4-01) */
                    continue;
                if (b == BLOCK_TORCH) { /* the stick, whatever is round it */
                    if (out)
                        mesh_torch(out + faces * MESH_FACE_VERTS, ox + (float)x, (float)y, oz + (float)z);
                    faces += MESH_TORCH_FACES;
                    continue;
                }
                for (f = 0; f < FACE_COUNT; f++)
                    if (!ref_solid(&s_c, nb, x + k_step[f][0], y + k_step[f][1], z + k_step[f][2])) {
                        if (out) {
                            unsigned char ao[4];
                            float t[4];

                            for (k = 0; k < 4; k++) {
                                ao[k] = (unsigned char)test_ref_ao(ctx_solid, &r, x, y, z, f, k);
                                t[k] = ref_corner_light(&r, x, y, z, f, k);
                                s_lit += t[k] > 0.0f;
                            }
                            mesh_face_lit(out + faces * MESH_FACE_VERTS, ox + (float)x, (float)y,
                                          oz + (float)z, f, block_face_tile(b, f), ao, t);
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
    int lit = 0;

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
        lit += s_lit; /* a quarter of the filled cells are torches */
    }
    CHECK_EQ_INT(lit > 1000, 1);
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

/* The top solid layer of column (x, z) of chunk c. */
static int top_of(const Chunk *c, int x, int z)
{
    int y = WORLD_HEIGHT - 1;

    while (y > 0 && chunk_get(c, x, y, z) == BLOCK_AIR)
        y--;
    return y;
}

/* Torches in real ground (row 3-03): one in a pocket dug deep down (so the
 * mesh's skipped layers move down to it), one on the surface, one on a
 * neighbour's surface just past the edge, each against the reference. */
static void test_torches_in_ground(void)
{
    static const int k_at[NB_COUNT][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
                                           { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 } };
    const Chunk *nb[NB_COUNT] = ALL_NB;
    uint32_t seed = test_land_seed();
    int i, base, low = WORLD_HEIGHT, x, z;

    terrain_fill_chunk(&s_c, seed, 0, 0);
    for (i = 0; i < NB_COUNT; i++)
        terrain_fill_chunk(&s_nb[i], seed, k_at[i][0], k_at[i][1]);
    for (z = 0; z < CHUNK_SIZE; z++)
        for (x = 0; x < CHUNK_SIZE; x++)
            if (top_of(&s_c, x, z) < low)
                low = top_of(&s_c, x, z);
    base = check_reference(nb, 0.0f, 0.0f);
    CHECK_EQ_INT(s_lit, 0);

    chunk_set(&s_c, 8, low - 9, 8, BLOCK_AIR);
    chunk_set(&s_c, 8, low - 10, 8, BLOCK_TORCH);
    /* The two-cell pocket shows the ten faces round it, all four corners lit. */
    CHECK_EQ_INT(check_reference(nb, 0.0f, 0.0f), base + 10 + MESH_TORCH_FACES);
    CHECK_EQ_INT(s_lit, 10 * 4);
    chunk_set(&s_c, 4, top_of(&s_c, 4, 4) + 1, 4, BLOCK_TORCH);
    chunk_set(&s_nb[NB_NEG_X], 15, top_of(&s_nb[NB_NEG_X], 15, 9) + 1, 9, BLOCK_TORCH);
    /* The neighbour's torch is its own mesh's; its light reaches this one. */
    CHECK_EQ_INT(check_reference(nb, 16.0f, -32.0f), base + 10 + 2 * MESH_TORCH_FACES);
    CHECK_EQ_INT(s_lit > 10 * 4 + 200, 1);
    CHECK_EQ_INT(check_reference(k_none, 16.0f, -32.0f) > base, 1);
}

/* The cells one layer under the lowest meshed one light its faces' corners
 * too: a hole in the +x neighbour sets that layer (20), and a torch in a
 * pocket in the corner neighbour lights the cell one down and across. */
static void test_light_under_lowest(void)
{
    const Chunk *nb[NB_COUNT] = ALL_NB;
    int i;

    chunk_clear(&s_c);
    memset(s_c.blocks, BLOCK_STONE, 40 * sizeof s_c.blocks[0]);
    for (i = 0; i < NB_COUNT; i++)
        memcpy(s_nb[i].blocks, s_c.blocks, sizeof s_c.blocks);
    chunk_set(&s_nb[NB_POS_X], 0, 20, 15, BLOCK_AIR);
    chunk_set(&s_nb[NB_POS_X_POS_Z], 0, 20, 0, BLOCK_AIR);
    chunk_set(&s_nb[NB_POS_X_POS_Z], 0, 19, 0, BLOCK_AIR);
    chunk_set(&s_nb[NB_POS_X_POS_Z], 1, 19, 0, BLOCK_TORCH);
    CHECK_EQ_INT(check_reference(nb, 0.0f, 0.0f), CHUNK_SIZE * CHUNK_SIZE + 1);
    CHECK_EQ_INT(s_lit, 4);
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
    test_torches_in_ground();
    test_light_under_lowest();
    test_buffer_too_small();
}
