#include "structure_templates.h"

#include <stddef.h>

#include "biome.h"
#include "block.h"
#include "terrain.h"

/* The village buildings (see structure_templates.h for the letters). Every
 * layer lists its rows back to front; the doorway is in the last row. */

/* 5 x 5: cobble foundation ring, planks floor, log corners, a window in
 * each side and the back, a pyramid roof. Door at column 2. */
static const char *const k_house_small[] = {
    /* 0 */ "ccccc", "cfffc", "cfffc", "cfffc", "ccfcc",
    /* 1 */ "FWWWF", "WT..W", "W...W", "W...W", "FWDWF",
    /* 2 */ "FW.WF", "W...W", ".....", "W...W", "FWDWF",
    /* 3 */ "FWWWF", "W...W", "W...W", "W...W", "FWWWF",
    /* 4 */ "RRRRR", "RRRRR", "RRRRR", "RRRRR", "RRRRR",
    /* 5 */ "     ", " RRR ", " RRR ", " RRR ", "     ",
    /* 6 */ "     ", "     ", "  R  ", "     ", "     ",
};

/* 7 x 8: two rooms with a doorway between them, windows both sides, a
 * stepped roof. Door at column 3. */
static const char *const k_house_large[] = {
    /* 0 */ "ccccccc", "cfffffc", "cfffffc", "cfffffc", "cfffffc", "cfffffc", "cfffffc", "cccfccc",
    /* 1 */ "FWWWWWF", "WT...TW", "W.....W", "WWW.WWW", "W.....W", "W.....W", "WT...TW", "FWWDWWF",
    /* 2 */ "FWW.WWF", ".......", "W.....W", "WWW.WWW", "W.....W", ".......", "W.....W", "FWWDWWF",
    /* 3 */ "FWWWWWF", "W.....W", "W.....W", "WWWWWWW", "W.....W", "W.....W", "W.....W", "FWWWWWF",
    /* 4 */ "RRRRRRR", "RRRRRRR", "RRRRRRR", "RRRRRRR", "RRRRRRR", "RRRRRRR", "RRRRRRR", "RRRRRRR",
    /* 5 */ "       ", " RRRRR ", " RRRRR ", " RRRRR ", " RRRRR ", " RRRRR ", " RRRRR ", "       ",
    /* 6 */ "       ", "       ", "  RRR  ", "  RRR  ", "  RRR  ", "  RRR  ", "       ", "       ",
    /* 7 */ "       ", "       ", "       ", "   R   ", "   R   ", "       ", "       ", "       ",
};

/* 4 x 4: one small room, windows on the sides, a flat roof. Door at
 * column 1. */
static const char *const k_hut[] = {
    /* 0 */ "cccc", "cffc", "cffc", "cfcc",
    /* 1 */ "FWWF", "WT.W", "W..W", "FDWF",
    /* 2 */ "FWWF", "...W", "W...", "FDWF",
    /* 3 */ "RRRR", "RRRR", "RRRR", "RRRR",
    /* 4 */ "    ", " RR ", " RR ", "    ",
};

/* 9 x 7: dirt with a water channel along the middle (ends closed, so
 * flowing water, row 4-04, stays in), a lit post at each corner. */
static const char *const k_farm[] = {
    /* 0 */ "sssssssss", "sssssssss", "sssssssss", "swwwwwwws", "sssssssss", "sssssssss", "sssssssss",
    /* 1 */ "F.......F", ".........", ".........", ".........", ".........", ".........", "F.......F",
    /* 2 */ "T.......T", ".........", ".........", ".........", ".........", ".........", "T.......T",
};

/* 5 x 4: cobble floor, a stone back wall, log posts, a forge block with a
 * torch on it, a flat roof; open at the front and the sides. */
static const char *const k_shed[] = {
    /* 0 */ "ccccc", "ccccc", "ccccc", "ccccc",
    /* 1 */ "FcccF", "..c..", ".....", "F...F",
    /* 2 */ "FcccF", "..T..", ".....", "F...F",
    /* 3 */ "FcccF", ".....", ".....", "F...F",
    /* 4 */ "RRRRR", "RRRRR", "RRRRR", "RRRRR",
};

static const char *const k_lamp[] = { "c", "F", "F", "T" };

/* 3 x 3: a ring of cobblestone round the water, posts at the corners and a
 * roof over all of it. */
static const char *const k_well[] = {
    /* 0 */ "ccc", "cwc", "ccc",
    /* 1 */ "ccc", "c.c", "ccc",
    /* 2 */ "F.F", "...", "F.F",
    /* 3 */ "F.F", "...", "F.F",
    /* 4 */ "RRR", "RRR", "RRR",
};

static const Template k_templates[TMPL_COUNT] = {
    [TMPL_HOUSE_SMALL] = { "small house", 5, 5, 7, 2, PART_FOUND, k_house_small },
    [TMPL_HOUSE_LARGE] = { "large house", 7, 8, 8, 3, PART_FOUND, k_house_large },
    [TMPL_HUT] = { "hut", 4, 4, 5, 1, PART_FOUND, k_hut },
    [TMPL_FARM] = { "farm", 9, 7, 3, -1, PART_SOIL, k_farm },
    [TMPL_SHED] = { "shed", 5, 4, 5, -1, PART_FOUND, k_shed },
    [TMPL_LAMP] = { "lamp post", 1, 1, 4, -1, PART_FOUND, k_lamp },
    [TMPL_WELL] = { "well", 3, 3, 5, -1, PART_FOUND, k_well },
};

const Template *template_get(int id)
{
    return id >= 0 && id < TMPL_COUNT ? &k_templates[id] : NULL;
}

static StructPart part_of_letter(char ch)
{
    switch (ch) {
    case '.': case 'D': return PART_AIR;
    case 'W': return PART_WALL;
    case 'F': return PART_FRAME;
    case 'f': return PART_FLOOR;
    case 'R': return PART_ROOF;
    case 'c': return PART_FOUND;
    case 's': return PART_SOIL;
    case 'w': return PART_WATER;
    case 'T': return PART_TORCH;
    default: return PART_SKIP;
    }
}

StructPart template_part(const Template *t, int ax, int y, int az)
{
    if (t == NULL || ax < 0 || ax >= t->w || y < 0 || y >= t->h || az < 0 || az >= t->d)
        return PART_SKIP;
    return part_of_letter(t->rows[y * t->d + az][ax]);
}

bool village_biome(int biome)
{
    return biome == BIOME_PLAINS || biome == BIOME_SAVANNA || biome == BIOME_DESERT ||
           biome == BIOME_TAIGA || biome == BIOME_SNOWY_PLAINS;
}

int village_style(uint32_t seed, int x, int z)
{
    return terrain_biome(seed, x, z) == BIOME_DESERT ? STYLE_DESERT : STYLE_PLAINS;
}

int style_block(int style, StructPart part)
{
    static const int k_style[STYLE_COUNT][PART_COUNT] = {
        [STYLE_PLAINS] = {
            [PART_SKIP] = BLOCK_AIR,
            [PART_AIR] = BLOCK_AIR,
            [PART_WALL] = BLOCK_PLANKS,
            [PART_FRAME] = BLOCK_LOG,
            [PART_FLOOR] = BLOCK_PLANKS,
            [PART_ROOF] = BLOCK_COBBLESTONE,
            [PART_FOUND] = BLOCK_COBBLESTONE,
            [PART_SOIL] = BLOCK_DIRT,
            [PART_WATER] = BLOCK_WATER,
            [PART_TORCH] = BLOCK_TORCH,
        },
        [STYLE_DESERT] = {
            [PART_SKIP] = BLOCK_AIR,
            [PART_AIR] = BLOCK_AIR,
            [PART_WALL] = BLOCK_SANDSTONE,
            [PART_FRAME] = BLOCK_SANDSTONE,
            [PART_FLOOR] = BLOCK_SANDSTONE,
            [PART_ROOF] = BLOCK_SANDSTONE,
            [PART_FOUND] = BLOCK_SANDSTONE,
            [PART_SOIL] = BLOCK_DIRT,
            [PART_WATER] = BLOCK_WATER,
            [PART_TORCH] = BLOCK_TORCH,
        },
    };

    if (style < 0 || style >= STYLE_COUNT)
        style = STYLE_PLAINS;
    return part >= 0 && part < PART_COUNT ? k_style[style][part] : BLOCK_AIR;
}

int style_path_block(int style)
{
    return style == STYLE_DESERT ? BLOCK_SANDSTONE : BLOCK_DIRT;
}

void struct_footprint(int w, int d, int rot, int *width, int *depth)
{
    *width = rot & 1 ? d : w;
    *depth = rot & 1 ? w : d;
}

void struct_map(int w, int d, int rot, int mirror, int ax, int az, int *lx, int *lz)
{
    if (mirror)
        ax = w - 1 - ax;
    switch (rot & 3) {
    case 0: *lx = ax; *lz = az; break;
    case 1: *lx = d - 1 - az; *lz = ax; break;
    case 2: *lx = w - 1 - ax; *lz = d - 1 - az; break;
    default: *lx = az; *lz = w - 1 - ax; break;
    }
}

void struct_unmap(int w, int d, int rot, int mirror, int lx, int lz, int *ax, int *az)
{
    switch (rot & 3) {
    case 0: *ax = lx; *az = lz; break;
    case 1: *az = d - 1 - lx; *ax = lz; break;
    case 2: *ax = w - 1 - lx; *az = d - 1 - lz; break;
    default: *az = lx; *ax = w - 1 - lz; break;
    }
    if (mirror)
        *ax = w - 1 - *ax;
}

int struct_rot_facing(int fx, int fz)
{
    if (fz > 0)
        return 0;
    if (fx < 0)
        return 1;
    if (fz < 0)
        return 2;
    return 3;
}

/* The ruins: how tall each wall column was before it fell down. */
static const char *const k_ruin_tower[] = { "45554", "5   5", "5 1 5", "5   5", "44 44" };
static const char *const k_ruin_wall[] = { "3   4   3", "443555344", "3   3   3" };
static const char *const k_ruin_house[] = { "333333", "3  3 3", "3  3 3", "3  3 3", "33 333" };
static const char *const k_ruin_pillars[] = {
    "4  1  3", "       ", "  1 1  ", "1  5  1", "  1 1  ", "       ", "5  1  2",
};

static const RuinShape k_ruins[RUIN_SHAPE_COUNT] = {
    [RUIN_TOWER] = { "tower", 5, 5, k_ruin_tower },
    [RUIN_WALL] = { "wall", 9, 3, k_ruin_wall },
    [RUIN_HOUSE] = { "house", 6, 5, k_ruin_house },
    [RUIN_PILLARS] = { "pillars", 7, 7, k_ruin_pillars },
};

const RuinShape *ruin_shape(int id)
{
    return id >= 0 && id < RUIN_SHAPE_COUNT ? &k_ruins[id] : NULL;
}

int ruin_shape_height(const RuinShape *s, int ax, int az)
{
    char ch;

    if (s == NULL || ax < 0 || ax >= s->w || az < 0 || az >= s->d)
        return 0;
    ch = s->rows[az][ax];
    return ch >= '1' && ch <= '9' ? ch - '0' : 0;
}
