#include "blocktex_internal.h"

/* The tiles of row 4-03 (trees and plants), drawn in code like the others:
 * five kinds of log, the leaves, the cactus and seven plants. Kept apart from
 * blocktex.c only for its length. */

#define CLEAR 0x00000000u /* a cut-out texel of a plant: alpha 0 */

/* ---- logs ------------------------------------------------------------ */

/* One log kind's colours: the bark (r, g, b), how much each column of bark
 * differs, the rings' pale and dark wood. */
typedef struct {
    int bark[3];
    int pale[3];
    int dark[3];
    int mark; /* >0: how many of 100 texels get a dark mark (birch's black dashes) */
    int mark_shade;
} LogLook;

static const LogLook k_log[5] = {
    { { 214, 210, 200 }, { 196, 176, 128 }, { 178, 158, 112 }, 12, -150 }, /* birch: pale papery bark */
    { { 80, 58, 36 }, { 122, 92, 56 }, { 104, 78, 46 }, 0, 0 },          /* spruce: dark brown */
    { { 60, 44, 26 }, { 70, 52, 30 }, { 56, 40, 24 }, 0, 0 },            /* dark oak: nearly black */
    { { 96, 88, 54 }, { 164, 128, 80 }, { 142, 108, 64 }, 6, -30 },      /* jungle: mossy brown */
    { { 108, 102, 92 }, { 176, 98, 58 }, { 156, 84, 48 }, 0, 0 },        /* acacia: grey bark, orange wood */
};

static uint32_t log_bark(const LogLook *k, int x, int y, uint32_t seed)
{
    uint32_t h = hash3((uint32_t)x, (uint32_t)y, seed);
    int column = (int)(hash3((uint32_t)x, 0, seed + 1) % 21) - 10;
    int d = wobble(h, 6) + column;

    if (k->mark > 0 && (int)((h >> 9) % 100) < k->mark)
        d += k->mark_shade;
    else if (k->mark == 0 && (h >> 13) % 23 == 0)
        d -= 14;
    return rgb(k->bark[0] + d, k->bark[1] + d, k->bark[2] + d * 8 / 10);
}

static uint32_t log_side(int kind, int x, int y)
{
    return log_bark(&k_log[kind], x, y, 40 + (uint32_t)kind * 4);
}

static uint32_t log_top(int kind, int x, int y)
{
    const LogLook *k = &k_log[kind];
    int dx = 2 * x - 15, dy = 2 * y - 15, r2 = dx * dx + dy * dy;
    int d = wobble(hash3((uint32_t)x, (uint32_t)y, 41 + (uint32_t)kind), 4);
    int ring = 0;
    const int *w;

    if (x < 2 || y < 2 || x > 13 || y > 13)
        return log_bark(k, x, y, 40 + (uint32_t)kind * 4);
    while ((ring + 1) * (ring + 1) * 12 <= r2)
        ring++;
    w = ring % 2 ? k->dark : k->pale;
    return rgb(w[0] + d, w[1] + d, w[2] + d * 8 / 10);
}

/* ---- leaves ---------------------------------------------------------- */

/* Leaves: "fast" leaves, opaque. A light green (it is multiplied by the
 * biome's foliage colour) of small clumps, with dark gaps where the sky would
 * show: about 1 texel in 4, in little blobs. The needle version has thin
 * vertical gaps. */
static uint32_t leaves(int x, int y, int conifer)
{
    uint32_t h = hash3((uint32_t)x, (uint32_t)y, conifer ? 52 : 51);
    uint32_t blob = hash3((uint32_t)(x / 2), (uint32_t)(y / 2), conifer ? 53 : 54);
    int d = wobble(h, 16);
    int gap;

    if (conifer)
        gap = (h >> 11) % 6 == 0 || (x % 4 == 2 && (blob % 3) == 0 && y % 3 != 0);
    else
        gap = (blob % 5) == 0 || (h >> 11) % 11 == 0;
    if (gap)
        return rgb(40 + d / 3, 58 + d / 3, 34 + d / 4);
    if ((h >> 15) % 7 == 0)
        d += 24; /* a lit leaf */
    if (conifer)
        return rgb(134 + d, 184 + d, 118 + d * 8 / 10);
    return rgb(152 + d, 202 + d, 108 + d * 7 / 10);
}

/* ---- cactus ---------------------------------------------------------- */

static uint32_t cactus_side(int x, int y)
{
    uint32_t h = hash3((uint32_t)x, (uint32_t)y, 60);
    int ridge = (x % 4 == 0) ? -22 : (x % 4 == 2) ? 10 : 0;
    int d = wobble(h, 5) + ridge;

    if (x % 4 == 1 && (y % 4 == 1) && (h >> 8) % 3 != 0)
        return rgb(196, 214, 150); /* a pale spine */
    return rgb(34 + d, 118 + d, 38 + d / 2);
}

static uint32_t cactus_top(int x, int y)
{
    uint32_t h = hash3((uint32_t)x, (uint32_t)y, 61);
    int d = wobble(h, 5);

    if (x < 1 || y < 1 || x > 14 || y > 14)
        return rgb(26 + d, 96 + d, 30 + d / 2);
    if ((x == 7 || x == 8) && (y == 7 || y == 8))
        return rgb(190, 208, 140); /* the middle tuft */
    return rgb(50 + d, 140 + d, 52 + d / 2);
}

/* ---- plants ---------------------------------------------------------- */

/* Short grass: blades growing from the bottom edge, each column its own
 * height (4 to 13), the tips lighter. The colour is the grass tile's, so the
 * biome's grass tint multiplies it. */
static uint32_t short_grass(int x, int y)
{
    uint32_t h = hash3((uint32_t)x, 0, 70);
    int height = 4 + (int)(h % 7) + (int)(hash3((uint32_t)(x / 3), 0, 71) % 4);
    int top = 16 - height;
    int d = wobble(hash3((uint32_t)x, (uint32_t)y, 72), 10);

    if (x % 5 == 4 && (h >> 8) % 2) /* a gap now and then */
        return CLEAR;
    if (y < top)
        return CLEAR;
    return rgb(116 + d + (y - top < 2 ? 20 : 0), 176 + d + (y - top < 2 ? 20 : 0),
               80 + d / 2 + (y > 12 ? -14 : 0));
}

/* Fern: a stem up the middle with two rows of leaflets angling up from it,
 * shorter towards the top. Grass-tinted. */
static uint32_t fern(int x, int y)
{
    int dx = x < 8 ? 7 - x : x - 8; /* 0 beside the stem .. 7 at the edge */
    int d = wobble(hash3((uint32_t)x, (uint32_t)y, 73), 8);

    if (y < 2)
        return CLEAR;
    if (x == 7 && y >= 2)
        return rgb(96 + d, 150 + d, 66);
    if (y >= 14)
        return x == 8 ? rgb(96 + d, 150 + d, 66) : CLEAR;
    /* leaflets: reach 1 + (y - 2) / 2 columns either side, only on every
     * other row, sloping up away from the stem */
    if (dx <= 1 + (y - 2) / 2 && dx >= 1 && (y + dx) % 2 == 0 && y >= 4)
        return rgb(110 + d, 168 + d, 78 + d / 2);
    if (y < 4 && dx <= 1)
        return rgb(120 + d, 178 + d, 84);
    return CLEAR;
}

/* A flower: the stem (x 7, green, from the head to the bottom, with a leaf
 * half way) and a head of head_w x head_h texels on top in two colours. */
static uint32_t flower(int x, int y, int top, int head_w, int head_h, uint32_t petal, uint32_t centre)
{
    int d = wobble(hash3((uint32_t)x, (uint32_t)y, 74), 6);
    int hx0 = 8 - head_w / 2;

    if (y >= top && y < top + head_h && x >= hx0 && x < hx0 + head_w) {
        int cx = x - hx0, cy = y - top;
        int corner = (cx == 0 || cx == head_w - 1) && (cy == 0 || cy == head_h - 1);

        if (corner)
            return CLEAR;
        if (cx >= head_w / 2 - 1 && cx <= head_w / 2 && cy >= head_h / 2 - 1 && cy <= head_h / 2)
            return centre;
        return petal;
    }
    if (x == 7 && y >= top + head_h && y < 16)
        return rgb(58 + d, 128 + d, 40);
    if (y == 11 && x == 5)
        return rgb(58 + d, 128 + d, 40); /* a leaf */
    if (y == 10 && x == 4)
        return rgb(70 + d, 142 + d, 48);
    if (y == 12 && x == 9)
        return rgb(58 + d, 128 + d, 40);
    return CLEAR;
}

/* Dead bush: a few brown twigs forking up from the bottom middle. */
static uint32_t dead_bush(int x, int y)
{
    int d = wobble(hash3((uint32_t)x, (uint32_t)y, 75), 10);
    int twig = 0;

    if (y >= 11 && (x == 7 || x == 8))
        twig = 1; /* the trunk */
    else if (y >= 7 && y < 11 && (x == 7 - (11 - y) / 2 || x == 8 + (11 - y) / 2))
        twig = 1; /* it forks */
    else if (y >= 3 && y < 7 && (x == 5 - (7 - y) / 2 || x == 10 + (7 - y) / 2 || x == 7 || x == 8) &&
             (y + x) % 5 != 0)
        twig = 1;
    else if (y == 9 && (x == 2 || x == 3 || x == 12 || x == 13))
        twig = 1; /* side twigs */
    else if (y == 8 && (x == 1 || x == 14))
        twig = 1;
    return twig ? rgb(118 + d, 88 + d, 50 + d / 2) : CLEAR;
}

uint32_t blocktex_tree_texel(int tile, int x, int y)
{
    if (tile >= TILE_BIRCH_LOG_SIDE && tile <= TILE_ACACIA_LOG_TOP) {
        int i = tile - TILE_BIRCH_LOG_SIDE;

        return i % 2 ? log_top(i / 2, x, y) : log_side(i / 2, x, y);
    }
    switch (tile) {
    case TILE_LEAVES:         return leaves(x, y, 0);
    case TILE_LEAVES_CONIFER: return leaves(x, y, 1);
    case TILE_CACTUS_SIDE:    return cactus_side(x, y);
    case TILE_CACTUS_TOP:     return cactus_top(x, y);
    case TILE_SHORT_GRASS:    return short_grass(x, y);
    case TILE_FERN:           return fern(x, y);
    case TILE_DANDELION:      return flower(x, y, 5, 4, 3, rgb(250, 214, 40), rgb(232, 160, 24));
    case TILE_POPPY:          return flower(x, y, 4, 5, 4, rgb(200, 36, 30), rgb(40, 20, 20));
    case TILE_OXEYE_DAISY:    return flower(x, y, 4, 6, 4, rgb(244, 244, 238), rgb(246, 196, 40));
    case TILE_BLUE_ORCHID:    return flower(x, y, 4, 5, 4, rgb(70, 170, 232), rgb(190, 226, 250));
    case TILE_DEAD_BUSH:      return dead_bush(x, y);
    default:                  return rgb(255, 0, 255);
    }
}
