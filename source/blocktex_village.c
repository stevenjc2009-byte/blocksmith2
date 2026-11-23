#include "blocktex_internal.h"

/* The tiles of the row 4-05 blocks (villages and ruins; trees use the
 * log too): planks, log side and top, cobblestone and mossy cobblestone.
 * Kept apart from blocktex.c only for its length. */

/* Planks (row 4-05): four oak boards one above the other, each 4 texels
 * tall with a dark seam at its bottom edge, and a dark joint where two
 * boards meet end to end (a different place in each board). */
uint32_t blocktex_planks_texel(int x, int y)
{
    int board = y / 4, row = y % 4;
    uint32_t h = hash3((uint32_t)x, (uint32_t)y, 11);
    int d = wobble(h, 7) + ((int)hash3((uint32_t)board, 0, 12) % 9) - 4; /* each board its own shade */

    if (row == 3)
        return rgb(120 + d / 2, 88 + d / 2, 50 + d / 3); /* the seam */
    if (x == (int)(hash3((uint32_t)board, 0, 13) % 16))
        return rgb(132 + d / 2, 98 + d / 2, 56 + d / 3); /* the end joint */
    if ((h >> 14) % 9 == 0)
        d -= 14; /* a bit of grain */
    return rgb(186 + d, 150 + d, 94 + d * 6 / 10);
}

/* Log side (row 4-05): dark brown bark with vertical furrows: each column
 * has its own darkness, and the odd knot. */
uint32_t blocktex_log_side_texel(int x, int y)
{
    uint32_t h = hash3((uint32_t)x, (uint32_t)y, 14);
    int column = (int)(hash3((uint32_t)x, 0, 15) % 25) - 12;
    int d = wobble(h, 6) + column;

    if ((h >> 13) % 23 == 0)
        d -= 22; /* a knot */
    return rgb(98 + d, 76 + d * 8 / 10, 46 + d * 6 / 10);
}

/* Log top (row 4-05): rings of pale wood, darker every other ring, in a
 * bark-brown rim 2 texels wide. The distance to the middle (7.5, 7.5) is
 * worked out in half texels, squared. */
uint32_t blocktex_log_top_texel(int x, int y)
{
    int dx = 2 * x - 15, dy = 2 * y - 15, r2 = dx * dx + dy * dy; /* 0 .. 450 */
    int d = wobble(hash3((uint32_t)x, (uint32_t)y, 16), 5);
    int ring;

    if (x < 2 || y < 2 || x > 13 || y > 13)
        return rgb(98 + d, 76 + d * 8 / 10, 46 + d * 6 / 10); /* the bark rim */
    ring = 0;
    while ((ring + 1) * (ring + 1) * 12 <= r2)
        ring++;
    return ring % 2 ? rgb(168 + d, 130 + d, 78 + d * 6 / 10) : rgb(190 + d, 152 + d, 96 + d * 6 / 10);
}

/* Cobblestone (row 4-05): rounded grey stones laid in rows 4 texels tall,
 * each row shifted, each stone its own shade, with dark gaps between. */
static int cobble_shade(int x, int y)
{
    int row = y / 4, shift = row % 2 ? 3 : 0;
    int stone = (x + shift) / 5;
    int sx = (x + shift) % 5, sy = y % 4;
    int v = 126 + ((int)hash3((uint32_t)stone, (uint32_t)row, 17) % 40) - 20 +
            wobble(hash3((uint32_t)x, (uint32_t)y, 18), 6);

    if (sx == 4 || sy == 3)
        return 70 + wobble(hash3((uint32_t)x, (uint32_t)y, 19), 5); /* the gap */
    if ((sx == 0 && sy == 0) || (sx == 3 && sy == 0))
        v -= 18; /* the stone's rounded corners */
    return v;
}

uint32_t blocktex_cobble_texel(int x, int y)
{
    int v = cobble_shade(x, y);

    return rgb(v, v, v + 2);
}

/* Mossy cobblestone (row 4-05): the same stones, with green moss over
 * patches 3 texels across (about 40 of 100 of them), and over the gaps
 * round them. */
uint32_t blocktex_mossy_cobble_texel(int x, int y)
{
    int v = cobble_shade(x, y);
    uint32_t patch = hash3((uint32_t)(x / 3), (uint32_t)(y / 3), 20);
    int moss = patch % 100 < 40 || (v < 90 && patch % 100 < 70);

    if (moss) {
        int d = wobble(hash3((uint32_t)x, (uint32_t)y, 21), 9);

        return rgb(v * 6 / 10 + d / 2, v * 9 / 10 + 22 + d, v * 4 / 10 + d / 2);
    }
    return rgb(v, v, v + 2);
}
