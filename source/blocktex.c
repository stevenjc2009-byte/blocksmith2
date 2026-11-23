#include "blocktex.h"

/* Every texel's wobble comes from a hash of where it is, so the picture is
 * the same on every run and every machine. */
static uint32_t hash3(uint32_t x, uint32_t y, uint32_t seed)
{
    uint32_t h = x * 0x8DA6B343u ^ y * 0xD8163841u ^ seed * 0xCB1AB31Fu;

    h ^= h >> 13;
    h *= 0x5BD1E995u;
    h ^= h >> 15;
    return h;
}

/* A whole number from -amount to +amount. */
static int wobble(uint32_t h, int amount)
{
    return (int)(h % (uint32_t)(2 * amount + 1)) - amount;
}

static int clamp255(int v)
{
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

static uint32_t rgb(int r, int g, int b)
{
    return ((uint32_t)clamp255(r) << 24) | ((uint32_t)clamp255(g) << 16) |
           ((uint32_t)clamp255(b) << 8) | 0xFF;
}

/* Grass seen from above: green with lighter and darker blades. */
static uint32_t grass_texel(int x, int y)
{
    uint32_t h = hash3((uint32_t)x, (uint32_t)y, 1);
    int d = wobble(h, 16);

    if ((h >> 12) % 7 == 0)
        d -= 22; /* a darker blade */
    return rgb(88 + d * 6 / 10, 150 + d, 50 + d * 4 / 10);
}

/* Dirt: brown with darker crumbs and the odd pale pebble. */
static uint32_t dirt_texel(int x, int y)
{
    uint32_t h = hash3((uint32_t)x, (uint32_t)y, 2);
    int d = wobble(h, 12);

    if ((h >> 12) % 9 == 0)
        d -= 26; /* crumb */
    else if ((h >> 12) % 13 == 1)
        return rgb(152 + d / 2, 122 + d / 2, 96 + d / 2); /* pebble */
    return rgb(132 + d, 94 + d * 8 / 10, 64 + d * 6 / 10);
}

/* Stone: grey, with darker patches a few texels across. */
static uint32_t stone_texel(int x, int y)
{
    uint32_t h = hash3((uint32_t)x, (uint32_t)y, 3);
    uint32_t patch = hash3((uint32_t)(x / 4), (uint32_t)(y / 3), 4);
    int v = 124 + wobble(h, 12);

    if (patch % 4 == 0)
        v -= 22;
    return rgb(v, v, v + 2);
}

/* Grass side: dirt with a ragged strip of grass hanging from the top edge. */
static uint32_t grass_side_texel(int x, int y)
{
    uint32_t h = hash3((uint32_t)x, 0, 5);
    int depth = 2 + (int)(h % 3);   /* 2..4 texels of grass */

    if ((h >> 8) % 5 == 0)
        depth += 2;                 /* a longer tuft now and then */
    return y < depth ? grass_texel(x, y) : dirt_texel(x, y + 7);
}

/* How far a texel is from the tile's nearest edge: 0 .. 7. */
static int edge_distance(int x, int y)
{
    int a = x < BLOCKTEX_TILE - 1 - x ? x : BLOCKTEX_TILE - 1 - x;
    int b = y < BLOCKTEX_TILE - 1 - y ? y : BLOCKTEX_TILE - 1 - y;

    return a < b ? a : b;
}

/* The sun: a pale yellow-white middle in a warmer ring and a gold rim. */
static uint32_t sun_texel(int x, int y)
{
    int e = edge_distance(x, y);
    int d = wobble(hash3((uint32_t)x, (uint32_t)y, 6), 5);

    if (e < 2)
        return rgb(255, 214 + d, 90 + d);
    if (e < 5)
        return rgb(255, 240 + d, 170 + d);
    return rgb(255, 250 + d, 222 + d);
}

/* The moon: pale blue-grey with four darker craters (middle x, y and the
 * square of the radius), each with a lighter rim. */
static uint32_t moon_texel(int x, int y)
{
    static const int k_craters[4][3] = { { 5, 5, 5 }, { 11, 9, 6 }, { 5, 12, 2 }, { 12, 3, 1 } };
    int d = wobble(hash3((uint32_t)x, (uint32_t)y, 7), 4);
    int i, rim = 0;

    for (i = 0; i < 4; i++) {
        int dx = x - k_craters[i][0], dy = y - k_craters[i][1];

        if (dx * dx + dy * dy <= k_craters[i][2])
            return rgb(160 + d, 168 + d, 184 + d);
        if (dx * dx + dy * dy <= k_craters[i][2] + 3)
            rim = 1;
    }
    return rim ? rgb(194 + d, 200 + d, 212 + d) : rgb(214 + d, 220 + d, 232 + d);
}

/* The torch (row 3-03): a stick two texels wide in columns 7 and 8, rows 6
 * to 15 (BLOCKTEX_TORCH_*): a yellow-white flame (rows 6 and 7), an orange
 * ember (row 8) and brown wood with a little grain (rows 9 to 15). The rest
 * is the hotbar's empty-slot grey, so the torch's icon is a torch standing
 * in a slot; the torch in the world only shows the stick. */
static uint32_t torch_texel(int x, int y)
{
    int d = wobble(hash3((uint32_t)x, (uint32_t)y, 8), 6);

    if (x < BLOCKTEX_TORCH_X0 || x >= BLOCKTEX_TORCH_X1 || y < BLOCKTEX_TORCH_Y0)
        return rgb(60, 60, 60);
    if (y == BLOCKTEX_TORCH_Y0)
        return rgb(255, 244, 170 + d);
    if (y < BLOCKTEX_TORCH_FLAME_Y1)
        return rgb(255, 204 + d, 64);
    if (y == BLOCKTEX_TORCH_FLAME_Y1)
        return rgb(240, 128 + d, 36);
    return rgb(124 + d, 86 + d, 48 + d / 2);
}

/* A loud check for tiles nothing uses yet. */
static uint32_t missing_texel(int x, int y)
{
    return ((x / 8) + (y / 8)) % 2 ? 0x000000FFu : 0xFF00FFFFu;
}

static uint32_t tile_texel(int tile, int x, int y)
{
    switch (tile) {
    case TILE_GRASS_TOP:  return grass_texel(x, y);
    case TILE_GRASS_SIDE: return grass_side_texel(x, y);
    case TILE_DIRT:       return dirt_texel(x, y);
    case TILE_STONE:      return stone_texel(x, y);
    case TILE_SUN:        return sun_texel(x, y);
    case TILE_MOON:       return moon_texel(x, y);
    case TILE_TORCH:      return torch_texel(x, y);
    default:              return missing_texel(x, y);
    }
}

void blocktex_build(uint32_t *atlas)
{
    int x, y;

    for (y = 0; y < BLOCKTEX_SIZE; y++)
        for (x = 0; x < BLOCKTEX_SIZE; x++) {
            int tile = (y / BLOCKTEX_TILE) * BLOCKTEX_PER_ROW + x / BLOCKTEX_TILE;
            atlas[y * BLOCKTEX_SIZE + x] =
                tile_texel(tile, x % BLOCKTEX_TILE, y % BLOCKTEX_TILE);
        }
}

void blocktex_sub_uv(int tile, int x0, int y0, int x1, int y1, float uv[4])
{
    const float texel = 1.0f / BLOCKTEX_SIZE;
    int left = (tile % BLOCKTEX_PER_ROW) * BLOCKTEX_TILE;
    int top = (tile / BLOCKTEX_PER_ROW) * BLOCKTEX_TILE; /* texels from the top */

    uv[0] = (left + x0) * texel + BLOCKTEX_INSET;
    uv[2] = (left + x1) * texel - BLOCKTEX_INSET;
    uv[3] = 1.0f - (top + y0) * texel - BLOCKTEX_INSET;
    uv[1] = 1.0f - (top + y1) * texel + BLOCKTEX_INSET;
}

void blocktex_tile_uv(int tile, float uv[4])
{
    const float step = 1.0f / BLOCKTEX_PER_ROW;
    int col = tile % BLOCKTEX_PER_ROW;
    int row = tile / BLOCKTEX_PER_ROW; /* from the top */

    uv[0] = col * step + BLOCKTEX_INSET;
    uv[2] = (col + 1) * step - BLOCKTEX_INSET;
    uv[3] = 1.0f - row * step - BLOCKTEX_INSET;
    uv[1] = 1.0f - (row + 1) * step + BLOCKTEX_INSET;
}
