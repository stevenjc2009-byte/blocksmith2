#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "chunk.h"
#include "light.h"
#include "test.h"

/* Row 3-03 torch light (light.h) against a slow reference written from the
 * rule in light.h, not from light.c: every cell of a chunk and its eight
 * neighbours (all WORLD_HEIGHT layers) relaxed until nothing changes, each
 * open cell taking one less than its brightest neighbour. light.c instead
 * spreads breadth-first inside a smaller box, so the two only agree if that
 * box really holds everything that matters. */

#define AREA (3 * CHUNK_SIZE) /* local x and z from -CHUNK_SIZE to 2 * CHUNK_SIZE - 1 */

static uint8_t s_ref[WORLD_HEIGHT][AREA][AREA];
static uint8_t s_open[WORLD_HEIGHT][AREA][AREA];

/* The chunk one over by (dx, dz), each -1 to 1, from the order in
 * chunk_mesh.h; the middle is c. */
static const Chunk *ref_chunk(const Chunk *c, const Chunk *const nb[NB_COUNT], int dx, int dz)
{
    static const int k_order[NB_COUNT][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
                                              { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 } };
    int i;

    if (dx == 0 && dz == 0)
        return c;
    for (i = 0; i < NB_COUNT; i++)
        if (k_order[i][0] == dx && k_order[i][1] == dz)
            return nb ? nb[i] : NULL;
    return NULL;
}

void test_ref_light(const Chunk *c, const Chunk *const nb[NB_COUNT])
{
    int x, y, z, f, any = 0, changed = 1;
    static const int k_step[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 },
                                      { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };

    memset(s_ref, 0, sizeof s_ref);
    for (y = 0; y < WORLD_HEIGHT; y++)
        for (z = 0; z < AREA; z++)
            for (x = 0; x < AREA; x++) {
                const Chunk *o = ref_chunk(c, nb, x / CHUNK_SIZE - 1, z / CHUNK_SIZE - 1);
                int b = o ? chunk_get(o, x % CHUNK_SIZE, y, z % CHUNK_SIZE) : BLOCK_AIR;

                /* Light goes through air, torches and water (row 4-01), nothing else. */
                s_open[y][z][x] = b == BLOCK_AIR || b == BLOCK_TORCH || b == BLOCK_WATER;
                if (b == BLOCK_TORCH) {
                    s_ref[y][z][x] = 14;
                    any = 1;
                }
            }
    while (any && changed) {
        changed = 0;
        for (y = 0; y < WORLD_HEIGHT; y++)
            for (z = 0; z < AREA; z++)
                for (x = 0; x < AREA; x++) {
                    int best = s_ref[y][z][x];

                    if (!s_open[y][z][x])
                        continue;
                    for (f = 0; f < 6; f++) {
                        int nx = x + k_step[f][0], ny = y + k_step[f][1], nz = z + k_step[f][2];

                        if (nx >= 0 && nx < AREA && nz >= 0 && nz < AREA && ny >= 0 &&
                            ny < WORLD_HEIGHT && s_ref[ny][nz][nx] - 1 > best)
                            best = s_ref[ny][nz][nx] - 1;
                    }
                    if (best != s_ref[y][z][x]) {
                        s_ref[y][z][x] = (uint8_t)best;
                        changed = 1;
                    }
                }
    }
}

int test_ref_level(int x, int y, int z)
{
    if (y < 0 || y >= WORLD_HEIGHT || x < -CHUNK_SIZE || x >= 2 * CHUNK_SIZE ||
        z < -CHUNK_SIZE || z >= 2 * CHUNK_SIZE)
        return 0;
    return s_ref[y][z + CHUNK_SIZE][x + CHUNK_SIZE];
}

/* --- random areas --------------------------------------------------------- */

static Chunk s_area[3][3]; /* [dz + 1][dx + 1] */
static const Chunk *s_nb[NB_COUNT];

static unsigned s_rng;

static int rnd(int n)
{
    s_rng = s_rng * 1103515245u + 12345u;
    return (int)((s_rng >> 16) % (unsigned)n);
}

static void link_area(void)
{
    s_nb[NB_POS_X] = &s_area[1][2];
    s_nb[NB_NEG_X] = &s_area[1][0];
    s_nb[NB_POS_Z] = &s_area[2][1];
    s_nb[NB_NEG_Z] = &s_area[0][1];
    s_nb[NB_POS_X_POS_Z] = &s_area[2][2];
    s_nb[NB_NEG_X_POS_Z] = &s_area[2][0];
    s_nb[NB_POS_X_NEG_Z] = &s_area[0][2];
    s_nb[NB_NEG_X_NEG_Z] = &s_area[0][0];
}

/* Rows ylo to yhi - 1 of all nine chunks: solid in pct percent of cells,
 * a torch in torch_pm per thousand, air in the rest. */
static void fill_area(int ylo, int yhi, int pct, int torch_pm)
{
    int i, j, x, y, z;

    for (j = 0; j < 3; j++)
        for (i = 0; i < 3; i++) {
            chunk_clear(&s_area[j][i]);
            for (y = ylo; y < yhi; y++)
                for (z = 0; z < CHUNK_SIZE; z++)
                    for (x = 0; x < CHUNK_SIZE; x++) {
                        int r = rnd(1000);

                        if (r < torch_pm)
                            chunk_set(&s_area[j][i], x, y, z, BLOCK_TORCH);
                        else if (r < torch_pm + pct * 10)
                            chunk_set(&s_area[j][i], x, y, z, BLOCK_GRASS + rnd(3));
                    }
        }
}

/* light_build() for rows ylo to yhi against the reference over every cell a
 * mesh samples (and none outside the asked rows). Returns how many sampled
 * cells are lit. */
static int compare(const Chunk *const nb[NB_COUNT], int ylo, int yhi)
{
    int x, y, z, bad = 0, lit = 0, built;

    test_ref_light(&s_area[1][1], nb);
    built = light_build(&s_area[1][1], nb, ylo, yhi);
    for (y = ylo < 0 ? 0 : ylo; y <= yhi && y < WORLD_HEIGHT; y++)
        for (z = -1; z <= CHUNK_SIZE; z++)
            for (x = -1; x <= CHUNK_SIZE; x++) {
                int want = test_ref_level(x, y, z);

                lit += want > 0;
                if (light_level(x, y, z) != want && bad++ == 0)
                    printf("light (%d, %d, %d): %d, reference %d (rows %d to %d)\n", x, y, z,
                           light_level(x, y, z), want, ylo, yhi);
            }
    CHECK_EQ_INT(bad, 0);
    CHECK_EQ_INT(built || lit == 0, 1); /* false only when nothing is lit */
    if (ylo > 0)
        CHECK_EQ_INT(light_level(5, ylo - 1, 5) + light_level(-1, ylo - 1, 16), 0);
    if (yhi < WORLD_HEIGHT - 1)
        CHECK_EQ_INT(light_level(5, yhi + 1, 5) + light_level(16, yhi + 1, -1), 0);
    return lit;
}

static void test_random_against_reference(void)
{
    static const unsigned seeds[] = { 3, 77, 2026 };
    const Chunk *some[NB_COUNT];
    size_t si;
    int i, lit = 0;

    link_area();
    for (si = 0; si < sizeof seeds / sizeof seeds[0]; si++) {
        s_rng = seeds[si];
        fill_area(30, 90, 55, 6); /* caves, a few torches in them */
        lit += compare(s_nb, 0, WORLD_HEIGHT - 1);
        lit += compare(s_nb, 55, 62);
        lit += compare(s_nb, 30, 30);
        lit += compare(s_nb, 89, 110);
        CHECK_EQ_INT(compare(s_nb, 103, 127), 0); /* over LIGHT_REACH above every torch */
        for (i = 0; i < NB_COUNT; i++)
            some[i] = (i + (int)si) % 3 == 0 ? NULL : s_nb[i]; /* missing chunks: air */
        lit += compare(some, 40, 70);
        lit += compare(NULL, 40, 70);
        fill_area(0, WORLD_HEIGHT, 12, 1); /* open, torches high and low */
        lit += compare(s_nb, 0, WORLD_HEIGHT - 1);
        lit += compare(s_nb, 0, 0);
        lit += compare(s_nb, 127, 127);
    }
    CHECK_EQ_INT(lit > 20000, 1); /* the comparisons saw plenty of light */
}

/* So many torches that the queue fills: settle() must still get it right. */
static void test_full_queue(void)
{
    link_area();
    s_rng = 9;
    fill_area(0, WORLD_HEIGHT, 20, 400);
    CHECK_EQ_INT(compare(s_nb, 0, WORLD_HEIGHT - 1) > 30000, 1);
    fill_area(40, 100, 0, 1000); /* nothing but torches */
    CHECK_EQ_INT(compare(s_nb, 30, 110) > 0, 1);
}

/* --- worked out by hand ----------------------------------------------------- */

static void test_one_torch(void)
{
    int y;

    link_area();
    for (y = 0; y < 9; y++)
        chunk_clear(&s_area[y / 3][y % 3]);
    chunk_set(&s_area[1][1], 8, 60, 8, BLOCK_TORCH);
    CHECK_EQ_INT(light_build(&s_area[1][1], s_nb, 0, WORLD_HEIGHT - 1), 1);
    CHECK_EQ_INT(light_level(8, 60, 8), 14);
    CHECK_EQ_INT(light_level(9, 60, 8), 13);
    CHECK_EQ_INT(light_level(8 + 5, 60 + 3, 8 - 2), 4);
    CHECK_EQ_INT(light_level(-1, 60, 8), 5);  /* the ring past the west edge */
    CHECK_EQ_INT(light_level(16, 60, 16), 0); /* 16 steps: too far */
    CHECK_EQ_INT(light_level(8, 73, 8), 1);
    CHECK_EQ_INT(light_level(8, 74, 8), 0);
    CHECK_EQ_INT(light_level(8, 60, 17), 0);  /* past the ring: not asked */

    /* A wall all across the chunk two steps east: the cell behind it is 19
     * steps round (out past the chunk's south edge and back), so dark. */
    for (y = 0; y < WORLD_HEIGHT; y++) {
        int z;

        for (z = 0; z < CHUNK_SIZE; z++)
            chunk_set(&s_area[1][1], 10, y, z, BLOCK_STONE);
    }
    CHECK_EQ_INT(light_build(&s_area[1][1], s_nb, 0, WORLD_HEIGHT - 1), 1);
    CHECK_EQ_INT(light_level(9, 60, 8), 13);
    CHECK_EQ_INT(light_level(10, 60, 8), 0); /* the wall itself */
    CHECK_EQ_INT(light_level(11, 60, 8), 0);
    CHECK_EQ_INT(light_level(11, 60, 15), 2); /* 8 south, 3 east, 1 back: 12 */
    CHECK_EQ_INT(light_level(9, 60, 16), 5);

    /* A torch block turned to air: nothing lit, build says so. */
    chunk_set(&s_area[1][1], 8, 60, 8, BLOCK_AIR);
    CHECK_EQ_INT(light_build(&s_area[1][1], s_nb, 0, WORLD_HEIGHT - 1), 0);
    CHECK_EQ_INT(light_level(9, 60, 8), 0);
    CHECK_EQ_INT(light_build(NULL, s_nb, 0, WORLD_HEIGHT - 1), 0);
    CHECK_EQ_INT(light_build(&s_area[1][1], s_nb, 200, 300), 0);
}

/* Torches in the neighbours, right at the edge of reach. */
static void test_reach(void)
{
    int i;

    link_area();
    for (i = 0; i < 9; i++)
        chunk_clear(&s_area[i / 3][i % 3]);
    chunk_set(&s_area[1][2], 13, 40, 4, BLOCK_TORCH); /* x 29: 13 past the ring */
    CHECK_EQ_INT(light_build(&s_area[1][1], s_nb, 0, WORLD_HEIGHT - 1), 1);
    CHECK_EQ_INT(light_level(16, 40, 4), 1);
    CHECK_EQ_INT(light_level(15, 40, 4), 0);
    chunk_set(&s_area[1][2], 13, 40, 4, BLOCK_AIR);
    chunk_set(&s_area[1][2], 14, 40, 4, BLOCK_TORCH); /* x 30: one too far */
    CHECK_EQ_INT(light_build(&s_area[1][1], s_nb, 0, WORLD_HEIGHT - 1), 0);
    chunk_set(&s_area[1][2], 14, 40, 4, BLOCK_AIR);
    chunk_set(&s_area[2][2], 5, 40, 5, BLOCK_TORCH); /* south-east (21, 21) */
    CHECK_EQ_INT(light_build(&s_area[1][1], s_nb, 0, WORLD_HEIGHT - 1), 1);
    CHECK_EQ_INT(light_level(16, 40, 16), 4);
    CHECK_EQ_INT(light_level(15, 40, 15), 2);
    /* Rows: 13 above the asked band still lights its top row, 14 does not. */
    chunk_set(&s_area[2][2], 5, 40, 5, BLOCK_AIR);
    chunk_set(&s_area[1][1], 8, 65, 8, BLOCK_TORCH);
    CHECK_EQ_INT(light_build(&s_area[1][1], s_nb, 50, 52), 1);
    CHECK_EQ_INT(light_level(8, 52, 8), 1);
    CHECK_EQ_INT(light_level(8, 51, 8), 0);
    chunk_set(&s_area[1][1], 8, 65, 8, BLOCK_AIR);
    chunk_set(&s_area[1][1], 8, 66, 8, BLOCK_TORCH);
    CHECK_EQ_INT(light_build(&s_area[1][1], s_nb, 50, 52), 0);
}

static void test_brightness(void)
{
    CHECK_NEAR(light_brightness(0), 0.0, 0.0);
    CHECK_NEAR(light_brightness(-3), 0.0, 0.0);
    CHECK_NEAR(light_brightness(1), 1.0 / 14.0, 1e-6);
    CHECK_NEAR(light_brightness(7), 0.5, 1e-6);
    CHECK_NEAR(light_brightness(14), 1.0, 0.0);
    CHECK_NEAR(light_brightness(20), 1.0, 0.0);
    CHECK_EQ_INT(LIGHT_TORCH, 14);
    CHECK_EQ_INT(LIGHT_REACH, 13);
}

/* --- light_touched_box() and the world's torch count ------------------------ */

static World s_w;

static void check_box(int x, int y, int z, int torch, int want, int x0, int z0, int x1, int z1)
{
    int box[4] = { 99, 99, 99, 99 };

    CHECK_EQ_INT(light_touched_box(&s_w, x, y, z, torch != 0, box), want);
    if (want) {
        CHECK_EQ_INT(box[0], x0);
        CHECK_EQ_INT(box[1], z0);
        CHECK_EQ_INT(box[2], x1);
        CHECK_EQ_INT(box[3], z1);
    }
}

static void test_touched_box(void)
{
    unsigned n;

    test_world_cleared(&s_w);
    check_box(0, 60, 0, 0, 0, 0, 0, 0, 0);     /* no torch anywhere */
    check_box(0, 60, 0, 1, 1, -1, -1, 0, 0);   /* a torch broken there: -14 to 14 */
    check_box(2, 60, 13, 1, 1, -1, -1, 1, 1);  /* 2 + 14 and 13 - 14 just cross an edge: */
    check_box(13, 60, 2, 1, 1, -1, -1, 1, 1);  /* the ring a mesh samples counts */
    n = s_w.torch_edits;
    world_set(&s_w, 0, 60, 0, BLOCK_TORCH);
    CHECK_EQ_INT(s_w.torch_edits, n + 1);
    world_set(&s_w, 0, 60, 0, BLOCK_TORCH);    /* no change: not counted */
    CHECK_EQ_INT(s_w.torch_edits, n + 1);
    check_box(5, 60, 0, 0, 1, -1, -1, 0, 0);   /* a block beside it */
    check_box(0, 60, 13, 0, 1, -1, -1, 0, 0);  /* 13 away: still in reach */
    check_box(0, 60, 14, 0, 0, 0, 0, 0, 0);    /* 14 away: not */
    check_box(6, 66, 1, 0, 1, -1, -1, 0, 0);   /* 13 away diagonally */
    check_box(6, 67, 1, 0, 0, 0, 0, 0, 0);
    check_box(10, 60, 3, 1, 1, -1, -1, 1, 1);  /* a new torch at (10, 3) too: -14 to 24, -14 to 17 */
    world_set(&s_w, -10, 60, 0, BLOCK_TORCH);
    check_box(-5, 60, 0, 0, 1, -2, -1, 0, 0);  /* between two torches: -24 to 14 */
    world_set(&s_w, -10, 60, 0, BLOCK_STONE);  /* a torch replaced: counted */
    CHECK_EQ_INT(s_w.torch_edits, n + 3);
    world_set(&s_w, -10, 60, 0, BLOCK_DIRT);   /* no torch either side: not */
    CHECK_EQ_INT(s_w.torch_edits, n + 3);

    /* Breaking the block under a torch takes the torch too, and counts. */
    world_set(&s_w, 0, 59, 0, BLOCK_STONE);
    CHECK_EQ_INT(world_break(&s_w, 0, 59, 0), 1);
    CHECK_EQ_INT(world_get(&s_w, 0, 60, 0), BLOCK_AIR);
    CHECK_EQ_INT(s_w.torch_edits, n + 4);
    /* Breaking a torch itself leaves the block under it. */
    world_set(&s_w, 3, 59, 3, BLOCK_STONE);
    world_set(&s_w, 3, 60, 3, BLOCK_TORCH);
    CHECK_EQ_INT(world_break(&s_w, 3, 60, 3), 1);
    CHECK_EQ_INT(world_get(&s_w, 3, 59, 3), BLOCK_STONE);
    CHECK_EQ_INT(world_get(&s_w, 3, 60, 3), BLOCK_AIR);
    CHECK_EQ_INT(s_w.torch_edits, n + 6);
}

void test_light(void)
{
    test_brightness();
    test_one_torch();
    test_reach();
    test_random_against_reference();
    test_full_queue();
    test_touched_box();
}
