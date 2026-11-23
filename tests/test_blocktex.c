#include <stdlib.h>
#include <string.h>

#include "blocktex.h"
#include "test.h"

#define TOL 1e-6

static uint32_t s_atlas[BLOCKTEX_SIZE * BLOCKTEX_SIZE];

/* Texel (x, y) of a tile, y from the top of the tile. */
static uint32_t texel(int tile, int x, int y)
{
    int tx = (tile % BLOCKTEX_PER_ROW) * BLOCKTEX_TILE;
    int ty = (tile / BLOCKTEX_PER_ROW) * BLOCKTEX_TILE;
    return s_atlas[(ty + y) * BLOCKTEX_SIZE + tx + x];
}

static int red(uint32_t c) { return (int)(c >> 24); }
static int green(uint32_t c) { return (int)((c >> 16) & 0xFF); }
static int blue(uint32_t c) { return (int)((c >> 8) & 0xFF); }

static int is_green(uint32_t c) { return green(c) > red(c) && green(c) > blue(c); }
static int is_brown(uint32_t c) { return red(c) > green(c) && green(c) > blue(c); }

static int is_grey(uint32_t c)
{
    return abs(red(c) - green(c)) <= 8 && abs(green(c) - blue(c)) <= 8 &&
           red(c) >= 70 && red(c) <= 180;
}

/* How many different colours a tile uses: a flat tile would be 1. */
static int colours_in(int tile)
{
    uint32_t seen[BLOCKTEX_TILE * BLOCKTEX_TILE];
    int n = 0, x, y, i;

    for (y = 0; y < BLOCKTEX_TILE; y++)
        for (x = 0; x < BLOCKTEX_TILE; x++) {
            uint32_t c = texel(tile, x, y);
            for (i = 0; i < n && seen[i] != c; i++)
                ;
            if (i == n)
                seen[n++] = c;
        }
    return n;
}

static void check_tile_all(int tile, int (*test)(uint32_t), const char *what)
{
    int x, y, bad = 0;

    for (y = 0; y < BLOCKTEX_TILE; y++)
        for (x = 0; x < BLOCKTEX_TILE; x++)
            if (!test(texel(tile, x, y)))
                bad++;
    if (bad)
        printf("     (tile %d: %d texels not %s)\n", tile, bad, what);
    CHECK_EQ_INT(bad, 0);
}

static void check_grass_side(void)
{
    int x, y, bad_run = 0, bad_below = 0, min_run = 99, max_run = 0;

    for (x = 0; x < BLOCKTEX_TILE; x++) {
        int run = 0;
        /* Green from the top edge down, then only brown. */
        while (run < BLOCKTEX_TILE && is_green(texel(TILE_GRASS_SIDE, x, run)))
            run++;
        if (run < 1 || run > 6)
            bad_run++;
        if (run < min_run)
            min_run = run;
        if (run > max_run)
            max_run = run;
        for (y = run; y < BLOCKTEX_TILE; y++)
            if (!is_brown(texel(TILE_GRASS_SIDE, x, y)))
                bad_below++;
    }
    CHECK_EQ_INT(bad_run, 0);
    CHECK_EQ_INT(bad_below, 0);
    /* The grass edge is ragged, not a straight line. */
    CHECK_EQ_INT(max_run > min_run, 1);
}

void test_blocktex(void)
{
    static uint32_t again[BLOCKTEX_SIZE * BLOCKTEX_SIZE];
    float uv[4];
    int i, bad_alpha = 0, t;

    blocktex_build(s_atlas);
    blocktex_build(again);
    CHECK_EQ_INT(memcmp(s_atlas, again, sizeof again), 0); /* same every time */

    for (i = 0; i < BLOCKTEX_SIZE * BLOCKTEX_SIZE; i++)
        if ((s_atlas[i] & 0xFF) != 0xFF)
            bad_alpha++;
    CHECK_EQ_INT(bad_alpha, 0);

    check_tile_all(TILE_GRASS_TOP, is_green, "green");
    check_tile_all(TILE_DIRT, is_brown, "brown");
    check_tile_all(TILE_STONE, is_grey, "grey");
    check_grass_side();

    for (t = 0; t < TILE_COUNT; t++)
        CHECK_EQ_INT(colours_in(t) >= 8, 1);

    /* An unused tile is the magenta and black check. */
    CHECK_EQ_INT((int)texel(TILE_COUNT, 0, 0), (int)0xFF00FFFFu);
    CHECK_EQ_INT((int)texel(TILE_COUNT, 8, 0), (int)0x000000FFu);

    /* Tile 0 is the top-left of the picture: u 0..0.25, v 0.75..1. */
    blocktex_tile_uv(TILE_GRASS_TOP, uv);
    CHECK_NEAR(uv[0], 0.0 + BLOCKTEX_INSET, TOL);
    CHECK_NEAR(uv[1], 0.75 + BLOCKTEX_INSET, TOL);
    CHECK_NEAR(uv[2], 0.25 - BLOCKTEX_INSET, TOL);
    CHECK_NEAR(uv[3], 1.0 - BLOCKTEX_INSET, TOL);
    /* Tile 3 is the right end of the top row. */
    blocktex_tile_uv(TILE_STONE, uv);
    CHECK_NEAR(uv[0], 0.75 + BLOCKTEX_INSET, TOL);
    CHECK_NEAR(uv[2], 1.0 - BLOCKTEX_INSET, TOL);
    CHECK_NEAR(uv[1], 0.75 + BLOCKTEX_INSET, TOL);
    /* Tile 5 is the second row, second column. */
    blocktex_tile_uv(5, uv);
    CHECK_NEAR(uv[0], 0.25 + BLOCKTEX_INSET, TOL);
    CHECK_NEAR(uv[1], 0.5 + BLOCKTEX_INSET, TOL);
    CHECK_NEAR(uv[3], 0.75 - BLOCKTEX_INSET, TOL);
}
