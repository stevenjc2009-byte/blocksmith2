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
