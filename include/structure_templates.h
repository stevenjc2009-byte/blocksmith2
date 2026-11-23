#ifndef STRUCTURE_TEMPLATES_H
#define STRUCTURE_TEMPLATES_H

#include <stdbool.h>
#include <stdint.h>

/* The shapes villages and ruins are built from (roadmap row 4-05). Pure C.
 *
 * A village building is a template: a box of w x h x d cells, each one a
 * letter for an abstract PART, not a block. A village style (the palette)
 * says which block each part is, so one template can be built in oak,
 * sandstone or spruce (oak for most villages, sandstone in deserts). The
 * layers go from the floor up; each layer is d rows, the BACK row first and
 * the FRONT row (the wall with the door, facing the path) last, each row w
 * letters, west to east. The letters:
 *
 *   ' ' leave the cell as it is     '.' air (the cell is cleared)
 *   'D' air too: a doorway cell     'W' wall       'F' frame (corner posts)
 *   'f' floor                       'R' roof       'c' foundation stone
 *   's' soil (farm rows)            'w' water      'T' torch
 *
 * Layer 0 is the floor, laid at the ground height of the door's path cell
 * (so a doorway is level with its path). */

typedef enum {
    PART_SKIP,
    PART_AIR,
    PART_WALL,
    PART_FRAME,
    PART_FLOOR,
    PART_ROOF,
    PART_FOUND,
    PART_SOIL,
    PART_WATER,
    PART_TORCH,
    PART_COUNT
} StructPart;

typedef enum {
    STYLE_PLAINS, /* oak planks and logs, cobblestone roofs and foundations */
    STYLE_DESERT, /* sandstone throughout, sandstone paths */
    STYLE_COUNT
} VillageStyleId;

/* True for a biome a village can stand in, as in Minecraft 1.17: plains,
 * savanna, desert, taiga and snowy plains. The biome of the village's
 * middle column decides. */
bool village_biome(int biome);

/* The style of the village whose middle is world column (x, z): sandstone
 * in a desert, oak (the plains style) everywhere else. */
int village_style(uint32_t seed, int x, int z);

/* The BLOCK_* a part is in a style (BLOCK_AIR for PART_AIR and PART_SKIP). */
int style_block(int style, StructPart part);

/* The block a village's path columns are made of: dirt, as Minecraft
 * 1.17's dirt paths; sandstone in a desert village (dirt on sand would look
 * out of place). */
int style_path_block(int style);

typedef enum {
    TMPL_HOUSE_SMALL, /* 5 x 5, pyramid roof */
    TMPL_HOUSE_LARGE, /* 7 x 8, two rooms */
    TMPL_HUT,         /* 4 x 4, flat roof */
    TMPL_FARM,        /* 9 x 7 dirt rows with a water channel, no door */
    TMPL_SHED,        /* 5 x 4 open shed with a forge */
    TMPL_LAMP,        /* 1 x 1 post with a torch on top */
    TMPL_WELL,        /* 3 x 3 well with water, in the middle of the village */
    TMPL_COUNT
} TemplateId;

typedef struct {
    const char *name;
    int w, d, h;             /* cells across (x), deep (z, front last) and layers */
    int door_x;              /* the doorway's column in the front row, or -1 */
    StructPart found;        /* what fills the ground under the floor */
    const char *const *rows; /* layer y, row z is rows[y * d + z] */
} Template;

const Template *template_get(int id);

/* The part of template t at cell (ax, y, az): PART_SKIP outside the box. */
StructPart template_part(const Template *t, int ax, int y, int az);

/* Turning a w x d box. rot is the number of quarter turns (0 to 3) that
 * puts the front row on +z (0), -x (1), -z (2) or +x (3); mirror flips x
 * first. struct_footprint() is the turned box's size, struct_map() takes a
 * cell of the template to its place in the turned box (lx 0 .. W - 1, lz
 * 0 .. D - 1), struct_unmap() the other way. */
void struct_footprint(int w, int d, int rot, int *width, int *depth);
void struct_map(int w, int d, int rot, int mirror, int ax, int az, int *lx, int *lz);
void struct_unmap(int w, int d, int rot, int mirror, int lx, int lz, int *ax, int *az);

/* The rot (above) of a building whose front faces (fx, fz), one of the four
 * unit steps. */
int struct_rot_facing(int fx, int fz);

/* Ruins: a shape is a map of how tall the broken wall is above each cell, a
 * digit 1 to 9 (' ': nothing there), rows from the back like a template. */
typedef enum { RUIN_TOWER, RUIN_WALL, RUIN_HOUSE, RUIN_PILLARS, RUIN_SHAPE_COUNT } RuinShapeId;

typedef struct {
    const char *name;
    int w, d;
    const char *const *rows; /* d rows of w characters */
} RuinShape;

const RuinShape *ruin_shape(int id);

/* How tall the shape is at cell (ax, az), 0 for none or outside. */
int ruin_shape_height(const RuinShape *s, int ax, int az);

#endif
