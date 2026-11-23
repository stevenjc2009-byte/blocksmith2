#include "block.h"
#include "chunk.h"
#include "light.h"
#include "test.h"

/* Row 3-05: light_world_level(), the torch light of one world cell (the
 * held block's and the break bits'), against test_light.c's reference. */

static World s_w;
static unsigned s_rng;

static int rnd(int n)
{
    s_rng = s_rng * 1103515245u + 12345u;
    return (int)((s_rng >> 16) % (unsigned)n);
}

/* 400 random cells of chunk (cx, cz) against the reference; returns how
 * many of them are lit. */
static int compare_chunk(int cx, int cz)
{
    const Chunk *nb[NB_COUNT];
    int i, bad = 0, lit = 0;

    world_neighbours(&s_w, cx, cz, nb);
    test_ref_light(world_chunk(&s_w, cx, cz), nb);
    for (i = 0; i < 400; i++) {
        int lx = rnd(CHUNK_SIZE), y = 40 + rnd(40), lz = rnd(CHUNK_SIZE);
        int want = test_ref_level(lx, y, lz);
        int got = light_world_level(&s_w, cx * CHUNK_SIZE + lx, y, cz * CHUNK_SIZE + lz);

        lit += want > 0;
        if (got != want && bad++ == 0)
            printf("world light (%d, %d, %d): %d, reference %d\n", cx * CHUNK_SIZE + lx, y,
                   cz * CHUNK_SIZE + lz, got, want);
    }
    CHECK_EQ_INT(bad, 0);
    return lit;
}

void test_light_world(void)
{
    int i, lit;

    test_world_cleared(&s_w);
    s_rng = 305;
    for (i = 0; i < 6000; i++) /* stone and torches through blocks -32 to 31 */
        world_set(&s_w, rnd(64) - 32, 50 + rnd(20), rnd(64) - 32,
                  rnd(100) < 8 ? BLOCK_TORCH : BLOCK_STONE);
    /* (0, -1): a chunk whose x and z differ, so x and z mixed up shows. */
    lit = compare_chunk(0, 0) + compare_chunk(-1, -1) + compare_chunk(0, -1);
    CHECK_EQ_INT(lit > 150, 1);
    printf("     (world light: %d of 1200 cells lit)\n", lit);

    test_world_cleared(&s_w);
    CHECK_EQ_INT(light_world_level(&s_w, 0, 60, 0), 0); /* no torch */
    world_set(&s_w, 31, 60, 31, BLOCK_TORCH);           /* the loaded world's corner */
    CHECK_EQ_INT(light_world_level(&s_w, 31, 60, 31), LIGHT_TORCH);
    CHECK_EQ_INT(light_world_level(&s_w, 30, 60, 31), LIGHT_TORCH - 1);
    CHECK_EQ_INT(light_world_level(&s_w, 31, 57, 28), LIGHT_TORCH - 6);
    CHECK_EQ_INT(light_world_level(&s_w, 32, 60, 31), 0); /* chunk 2: not loaded */
    world_set(&s_w, 0, 0, 0, BLOCK_TORCH);
    CHECK_EQ_INT(light_world_level(&s_w, 0, 0, 0), LIGHT_TORCH);
    CHECK_EQ_INT(light_world_level(&s_w, 0, -1, 0), 0); /* under the world */
    world_set(&s_w, 0, WORLD_HEIGHT - 1, 0, BLOCK_TORCH);
    CHECK_EQ_INT(light_world_level(&s_w, 0, WORLD_HEIGHT - 1, 0), LIGHT_TORCH);
    CHECK_EQ_INT(light_world_level(&s_w, 0, WORLD_HEIGHT, 0), 0); /* over it */
    world_set(&s_w, 1, 60, 0, BLOCK_STONE); /* a solid cell beside a torch */
    world_set(&s_w, 0, 60, 0, BLOCK_TORCH);
    CHECK_EQ_INT(light_world_level(&s_w, 1, 60, 0), 0);
    for (i = 0; i < 600; i++) /* many searches: the seen marks wrap past 255 */
        CHECK_EQ_INT(light_world_level(&s_w, 0, 60, 2 + i % 3), LIGHT_TORCH - 2 - i % 3);

    /* It leaves the last light_build()'s levels alone. */
    {
        const Chunk *nb[NB_COUNT];

        world_neighbours(&s_w, 0, 0, nb);
        CHECK_EQ_INT(light_build(world_chunk(&s_w, 0, 0), nb, 60, 60), 1);
        CHECK_EQ_INT(light_level(0, 60, 3), LIGHT_TORCH - 3);
        CHECK_EQ_INT(light_world_level(&s_w, 30, 60, 31), LIGHT_TORCH - 1);
        CHECK_EQ_INT(light_level(0, 60, 3), LIGHT_TORCH - 3);
    }
}
