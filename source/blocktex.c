#include "blocktex_internal.h"

/* Grass round the colour (r, g, b), with lighter and darker blades. */
static uint32_t grass_blades(int x, int y, int r, int g, int b)
{
    uint32_t h = hash3((uint32_t)x, (uint32_t)y, 1);
    int d = wobble(h, 16);

    if ((h >> 12) % 7 == 0)
        d -= 22; /* a darker blade */
    return rgb(r + d * 6 / 10, g + d, b + d * 4 / 10);
}

/* Grass seen from above: light green. Light on purpose: the mesh multiplies
 * it by the biome's tint (biome.h), which can only darken a colour, and the
 * plains tint brings it back to the green it was before row 4-02 (about
 * 88, 150, 50). */
static uint32_t grass_texel(int x, int y)
{
    return grass_blades(x, y, 118, 178, 84);
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
    /* The side is not tinted (that would darken its dirt too), so its
     * fringe keeps the plains green the grass always had. */
    return y < depth ? grass_blades(x, y, 88, 150, 50) : dirt_texel(x, y + 7);
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

/* The clouds (row 3-04): plain white; their faces are shaded by brightness. */
static uint32_t cloud_texel(int x, int y)
{
    (void)x;
    (void)y;
    return rgb(255, 255, 255);
}

/* Sand (row 4-01): pale yellow, with darker grains and the odd bright
 * one. */
static uint32_t sand_texel(int x, int y)
{
    uint32_t h = hash3((uint32_t)x, (uint32_t)y, 9);
    int d = wobble(h, 7);

    if ((h >> 12) % 8 == 0)
        return rgb(192 + d, 176 + d, 128 + d); /* a darker grain */
    if ((h >> 12) % 11 == 1)
        return rgb(236 + d, 228 + d, 192 + d); /* a bright one */
    return rgb(219 + d, 207 + d, 163 + d);
}

/* Water (row 4-01): blue, with soft lighter ripples running across in a
 * gentle zigzag; the rows and columns repeat every 8, so tiles side by side
 * join up. Every texel is BLOCKTEX_WATER_ALPHA solid. */
static uint32_t water_texel(int x, int y)
{
    int d = wobble(hash3((uint32_t)x, (uint32_t)y, 10), 5);
    int zig = (x & 7) < 4 ? 4 - (x & 7) : (x & 7) - 4; /* 4 3 2 1 0 1 2 3 */
    int phase = (y + zig / 2) & 7;
    int up = phase == 0 ? 26 : phase == 1 ? 12 : 0;   /* the crest, then its edge */

    return (rgb(46 + d + up * 3 / 4, 98 + d + up, 196 + d + up * 3 / 4) & ~0xFFu) |
           BLOCKTEX_WATER_ALPHA;
}

/* Snow: white with pale blue-grey shading and the odd bluer speck. */
static uint32_t snow_texel(int x, int y)
{
    uint32_t h = hash3((uint32_t)x, (uint32_t)y, 11);
    int d = wobble(h, 5);

    if ((h >> 12) % 9 == 0)
        return rgb(212 + d, 224 + d, 240 + d / 2);
    return rgb(244 + d, 248 + d, 252);
}

/* Snowy grass side: dirt with a ragged fringe of snow along the top. */
static uint32_t snowy_side_texel(int x, int y)
{
    uint32_t h = hash3((uint32_t)x, 0, 12);
    int depth = 3 + (int)(h % 3);

    if ((h >> 8) % 4 == 0)
        depth += 2;
    return y < depth ? snow_texel(x, y) : dirt_texel(x, y + 7);
}

/* Ice: pale blue, with lighter streaks running one way and faint cracks. */
static uint32_t ice_texel(int x, int y)
{
    uint32_t h = hash3((uint32_t)x, (uint32_t)y, 13);
    int d = wobble(h, 5);
    int streak = ((x + y) % 7 == 0 || (x + y + 3) % 11 == 0) ? 18 : 0;

    if ((h >> 12) % 17 == 0)
        d -= 18; /* a crack */
    return rgb(150 + d + streak, 196 + d + streak * 3 / 4, 238 + d / 2 + streak / 3);
}

/* Sandstone: sand-coloured. The top is smooth; the side has layers. */
static uint32_t sandstone_top_texel(int x, int y)
{
    int d = wobble(hash3((uint32_t)x, (uint32_t)y, 14), 4);

    return rgb(214 + d, 198 + d, 148 + d);
}

static uint32_t sandstone_side_texel(int x, int y)
{
    uint32_t h = hash3((uint32_t)x, (uint32_t)y, 15);
    int d = wobble(h, 4);

    if (y == 3 || y == 9 || y == 10)
        d -= 22; /* a groove between layers */
    else if (y == 2 || y == 8)
        d += 8;
    return rgb(212 + d, 195 + d, 144 + d);
}

static uint32_t red_sand_texel(int x, int y)
{
    uint32_t h = hash3((uint32_t)x, (uint32_t)y, 16);
    int d = wobble(h, 7);

    if ((h >> 12) % 8 == 0)
        return rgb(160 + d, 78 + d, 32 + d); /* a darker grain */
    if ((h >> 12) % 11 == 1)
        return rgb(212 + d, 128 + d, 62 + d);
    return rgb(190 + d, 102 + d, 44 + d);
}

/* Terracotta: a flat colour (r, g, b) with a faint grain; id keeps each
 * colour's grain different. */
static uint32_t terracotta_texel(int x, int y, int r, int g, int b, uint32_t id)
{
    uint32_t h = hash3((uint32_t)x, (uint32_t)y, id);
    int d = wobble(h, 4);

    if ((h >> 12) % 10 == 0)
        d -= 6;
    return rgb(r + d, g + d * 8 / 10, b + d * 7 / 10);
}

/* Gravel: grey and brown stones two or three texels across. */
static uint32_t gravel_texel(int x, int y)
{
    uint32_t stone = hash3((uint32_t)(x + y / 3) / 3, (uint32_t)(y + x / 4) / 3, 17);
    uint32_t h = hash3((uint32_t)x, (uint32_t)y, 18);
    int v = 110 + (int)(stone % 56) - 28 + wobble(h, 7);

    if (stone % 5 == 0)
        return rgb(v + 14, v + 4, v - 12); /* a brownish stone */
    return rgb(v, v, v + 2);
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
    case TILE_CLOUD:      return cloud_texel(x, y);
    case TILE_SAND:       return sand_texel(x, y);
    case TILE_WATER:      return water_texel(x, y);
    case TILE_SNOW:       return snow_texel(x, y);
    case TILE_SNOWY_GRASS_SIDE: return snowy_side_texel(x, y);
    case TILE_ICE:        return ice_texel(x, y);
    case TILE_SANDSTONE_TOP:  return sandstone_top_texel(x, y);
    case TILE_SANDSTONE_SIDE: return sandstone_side_texel(x, y);
    case TILE_RED_SAND:   return red_sand_texel(x, y);
    case TILE_TERRACOTTA: return terracotta_texel(x, y, 152, 94, 67, 19);
    case TILE_TERRACOTTA_WHITE:  return terracotta_texel(x, y, 209, 178, 161, 20);
    case TILE_TERRACOTTA_YELLOW: return terracotta_texel(x, y, 186, 133, 36, 21);
    case TILE_TERRACOTTA_RED:    return terracotta_texel(x, y, 143, 61, 46, 22);
    case TILE_TERRACOTTA_BROWN:  return terracotta_texel(x, y, 77, 51, 36, 23);
    case TILE_GRAVEL:     return gravel_texel(x, y);
    case TILE_PLANKS:     return blocktex_planks_texel(x, y);
    case TILE_LOG_SIDE:   return blocktex_log_side_texel(x, y);
    case TILE_LOG_TOP:    return blocktex_log_top_texel(x, y);
    case TILE_COBBLE:     return blocktex_cobble_texel(x, y);
    case TILE_MOSSY_COBBLE: return blocktex_mossy_cobble_texel(x, y);
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
