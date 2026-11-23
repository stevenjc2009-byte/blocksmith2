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
static int is_white(uint32_t c) { return c == 0xFFFFFFFFu; }

/* Row 4-01: pale yellow sand; water mostly blue, see-through. */
static int is_sand(uint32_t c) { return red(c) >= green(c) && green(c) > blue(c) && red(c) > 180; }
static int is_water(uint32_t c)
{
    return blue(c) > green(c) && green(c) > red(c) && (c & 0xFF) == BLOCKTEX_WATER_ALPHA;
}

/* Which tile a texel of the picture is in. */
static int tile_of(int i)
{
    return (i / BLOCKTEX_SIZE / BLOCKTEX_TILE) * BLOCKTEX_PER_ROW +
           (i % BLOCKTEX_SIZE) / BLOCKTEX_TILE;
}

/* Row 4-02 tiles. */
static int is_snow(uint32_t c) { return red(c) >= 200 && green(c) >= 215 && blue(c) >= 232 && blue(c) >= red(c); }
static int is_ice(uint32_t c) { return blue(c) > red(c) && blue(c) >= green(c) && red(c) >= 100 && green(c) >= 150; }
static int is_red_sand(uint32_t c) { return red(c) > green(c) + 40 && green(c) > blue(c) && red(c) > 150; }
static int is_sand_stone(uint32_t c) { return red(c) >= green(c) && green(c) > blue(c) && red(c) > 160; }
static int is_gravel(uint32_t c)
{
    return abs(red(c) - green(c)) <= 16 && abs(green(c) - blue(c)) <= 16 && red(c) >= 70 && red(c) <= 200;
}
static int is_warm(uint32_t c) { return red(c) > green(c) && green(c) >= blue(c); }

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

/* Row 3-03: the torch tile is a two-texel stick on the slot grey, columns
 * 7 and 8: flame and ember in rows 6 to 8, wood in rows 9 to 15. */
static void check_torch(void)
{
    const float t = 1.0f / BLOCKTEX_SIZE;
    float uv[4];
    int x, y, grey = 0, flame = 0, wood = 0;

    for (y = 0; y < BLOCKTEX_TILE; y++)
        for (x = 0; x < BLOCKTEX_TILE; x++) {
            uint32_t c = texel(TILE_TORCH, x, y);
            int stick = x >= 7 && x <= 8 && y >= 6;

            if (!stick && red(c) == 60 && green(c) == 60 && blue(c) == 60)
                grey++;
            else if (stick && y <= 8 && red(c) >= 240 && green(c) > blue(c))
                flame++;
            else if (stick && y >= 9 && is_brown(c) && red(c) < 140)
                wood++;
        }
    CHECK_EQ_INT(grey, 256 - 20);
    CHECK_EQ_INT(flame, 6);
    CHECK_EQ_INT(wood, 14);
    CHECK_EQ_INT(TILE_TORCH, 6); /* column 6, row 0 of the 8 by 8 atlas: left at texel 96 */
    blocktex_sub_uv(TILE_TORCH, 7, 6, 9, 16, uv);
    CHECK_NEAR(uv[0], 0.75f + 7 * t + BLOCKTEX_INSET, TOL);
    CHECK_NEAR(uv[2], 0.75f + 9 * t - BLOCKTEX_INSET, TOL);
    CHECK_NEAR(uv[3], 1.0f - 6 * t - BLOCKTEX_INSET, TOL);
    CHECK_NEAR(uv[1], 0.875f + BLOCKTEX_INSET, TOL);
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
        if (tile_of(i) != TILE_WATER && (s_atlas[i] & 0xFF) != 0xFF)
            bad_alpha++; /* only water lets the world show through */
    CHECK_EQ_INT(bad_alpha, 0);
    CHECK_EQ_INT(BLOCKTEX_WATER_ALPHA > 0x60 && BLOCKTEX_WATER_ALPHA < 0xE0, 1);

    check_tile_all(TILE_GRASS_TOP, is_green, "green");
    check_tile_all(TILE_DIRT, is_brown, "brown");
    check_tile_all(TILE_STONE, is_grey, "grey");
    check_grass_side();
    check_torch();
    check_tile_all(TILE_SAND, is_sand, "sand"); /* row 4-01 */
    check_tile_all(TILE_WATER, is_water, "water");
    CHECK_EQ_INT(TILE_SAND, 8);
    CHECK_EQ_INT(TILE_WATER, 9);

    for (t = 0; t < TILE_COUNT; t++)
        if (t != TILE_CLOUD) /* row 3-04: the clouds are flat white */
            CHECK_EQ_INT(colours_in(t) >= 8, 1);
    check_tile_all(TILE_CLOUD, is_white, "white");
    blocktex_tile_uv(TILE_CLOUD, uv); /* tile 7: first row, right end */
    CHECK_NEAR(uv[0], 0.875 + BLOCKTEX_INSET, TOL);
    CHECK_NEAR(uv[1], 0.875 + BLOCKTEX_INSET, TOL);

    /* An unused tile is the magenta and black check. */
    CHECK_EQ_INT((int)texel(TILE_COUNT, 0, 0), (int)0xFF00FFFFu);
    CHECK_EQ_INT((int)texel(TILE_COUNT, 8, 0), (int)0x000000FFu);

    /* Tile 0 is the top-left of the picture: u 0..0.125, v 0.875..1. */
    blocktex_tile_uv(TILE_GRASS_TOP, uv);
    CHECK_NEAR(uv[0], 0.0 + BLOCKTEX_INSET, TOL);
    CHECK_NEAR(uv[1], 0.875 + BLOCKTEX_INSET, TOL);
    CHECK_NEAR(uv[2], 0.125 - BLOCKTEX_INSET, TOL);
    CHECK_NEAR(uv[3], 1.0 - BLOCKTEX_INSET, TOL);
    /* Tile 3 is the fourth column of the top row. */
    blocktex_tile_uv(TILE_STONE, uv);
    CHECK_NEAR(uv[0], 0.375 + BLOCKTEX_INSET, TOL);
    CHECK_NEAR(uv[2], 0.5 - BLOCKTEX_INSET, TOL);
    CHECK_NEAR(uv[1], 0.875 + BLOCKTEX_INSET, TOL);
    /* Tile 9 is the second row, second column. */
    blocktex_tile_uv(9, uv);
    CHECK_NEAR(uv[0], 0.125 + BLOCKTEX_INSET, TOL);
    CHECK_NEAR(uv[1], 0.75 + BLOCKTEX_INSET, TOL);
    CHECK_NEAR(uv[3], 0.875 - BLOCKTEX_INSET, TOL);

    /* Row 4-02: the picture grew to 128 pixels (8 by 8 tiles) for the new
     * blocks; each new tile is drawn (not the magenta check) and its own
     * colour, snow white-blue, ice light blue, terracotta bands warm. */
    CHECK_EQ_INT(BLOCKTEX_SIZE, 128);
    CHECK_EQ_INT(BLOCKTEX_PER_ROW, 8);
    CHECK_EQ_INT(TILE_COUNT, 22);
    for (t = TILE_SNOW; t < TILE_COUNT; t++) {
        int x, y, check = 0;

        for (y = 0; y < BLOCKTEX_TILE; y++)
            for (x = 0; x < BLOCKTEX_TILE; x++)
                check += texel(t, x, y) == 0xFF00FFFFu;
        CHECK_EQ_INT(check, 0);
    }
    check_tile_all(TILE_SNOW, is_snow, "snow");
    check_tile_all(TILE_ICE, is_ice, "ice");
    check_tile_all(TILE_RED_SAND, is_red_sand, "red sand");
    check_tile_all(TILE_GRAVEL, is_gravel, "gravel grey");
    check_tile_all(TILE_SANDSTONE_TOP, is_sand_stone, "sandstone");
    check_tile_all(TILE_SANDSTONE_SIDE, is_sand_stone, "sandstone");
    check_tile_all(TILE_TERRACOTTA, is_warm, "warm");
    check_tile_all(TILE_TERRACOTTA_WHITE, is_warm, "warm");
    check_tile_all(TILE_TERRACOTTA_YELLOW, is_warm, "warm");
    check_tile_all(TILE_TERRACOTTA_RED, is_warm, "warm");
    check_tile_all(TILE_TERRACOTTA_BROWN, is_warm, "warm");
    /* The snowy grass side is white at the top and dirt below. */
    CHECK_EQ_INT(is_snow(texel(TILE_SNOWY_GRASS_SIDE, 3, 0)), 1);
    CHECK_EQ_INT(is_brown(texel(TILE_SNOWY_GRASS_SIDE, 3, 15)), 1);
    /* The grass top is light enough for every biome tint to only darken. */
    CHECK_EQ_INT(green(texel(TILE_GRASS_TOP, 4, 4)) > 160, 1);
    /* The grass side is never tinted, so its fringe is the green of a
     * plains-tinted top (about 150), not the light untinted top. */
    {
        int x, side = 0, top = 0;

        for (x = 0; x < BLOCKTEX_TILE; x++) {
            side += green(texel(TILE_GRASS_SIDE, x, 0));
            top += green(texel(TILE_GRASS_TOP, x, 0));
        }
        CHECK_NEAR((double)side / BLOCKTEX_TILE, (double)top / BLOCKTEX_TILE * 150.0 / 178.0, 4.0);
    }
}
