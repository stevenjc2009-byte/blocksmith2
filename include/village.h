#ifndef VILLAGE_H
#define VILLAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "structure.h"

/* One village (roadmap row 4-05): where it is and everything in it, as
 * planned for one village region (structure.h).
 *
 * The layout: a plaza of 5 x 5 columns round a well in the middle; up to
 * four straight paths (arms) running out from it on x and z, one block
 * wide, each column at its own ground height (the top ground block is the
 * path block); a spur or two branching off the arms; a lamp post beside
 * each path; and 5 to 10 buildings along the paths, each facing its path,
 * its doorway (or front) on the path's edge. Every building is laid with
 * its floor at the ground height of the path column in front of its door,
 * with foundation stone filled under it down to the ground and the air
 * cleared over it, so nothing floats and nothing is buried.
 *
 * A site is only used where every column the village covers is plains or
 * hills (never water, beach, river or mountains) and each building's
 * ground is fairly level (VILLAGE_FIT). Everything stays within
 * VILLAGE_RADIUS of the middle. */

#define VILLAGE_RADIUS 40
#define VILLAGE_FIT 3        /* a building's ground may be this far above or below its floor */
#define VILLAGE_MARGIN 64    /* the middle is at least this far inside the region's edge */
#define VILLAGE_MAX_BUILD 24 /* well, lamps and buildings */
#define VILLAGE_MAX_PATH 224
#define VILLAGE_COLS 72      /* most columns a building covers (9 x 7 farm: 63, 7 x 8 house: 56) */

typedef struct {
    uint8_t tmpl;            /* TMPL_* */
    uint8_t rot, mirror;     /* structure_templates.h */
    int x0, z0;              /* the north-west corner of its footprint */
    int width, depth;        /* the footprint turned: x and z size */
    int fy;                  /* y of its floor layer */
    int clear_top;           /* the air above it is cleared up to this y */
    uint8_t gh[VILLAGE_COLS]; /* the ground height under each footprint column, [z * width + x] */
} VillageBuilding;

typedef struct {
    int x, z;
    uint8_t y; /* the ground height there: the path block is at this y */
} VillagePathCol;

typedef struct {
    bool valid;      /* false: no village in this region */
    int cx, cz;      /* the middle: the well's centre column */
    int style;       /* VillageStyleId */
    int nbuild;      /* entries of b: [0] is the well, then lamps and buildings */
    int nhouses;     /* how many of them are buildings (not the well or lamps) */
    int npath;
    int x0, z0, x1, z1; /* the box round everything, inclusive */
    VillageBuilding b[VILLAGE_MAX_BUILD];
    VillagePathCol path[VILLAGE_MAX_PATH]; /* plaza and paths, columns of dirt */
} VillagePlan;

/* The village of region (rx, rz) (regions are VILLAGE_REGION_CHUNKS chunks
 * wide, region 0 holds chunks 0 to 23), planned afresh: out->valid is false
 * when there is none. The same answer every time. */
bool village_plan(uint32_t seed, int rx, int rz, VillagePlan *out);

/* The same with the spawn column given instead of looked up (the tests use
 * it to put the spawn on top of a village). has_spawn false: no spawn. */
bool village_plan_spawn(uint32_t seed, int rx, int rz, bool has_spawn, int sx, int sz,
                        VillagePlan *out);

/* The same from the cache; NULL when the region has no village. The
 * pointer lasts until the next call. */
const VillagePlan *village_get(uint32_t seed, int rx, int rz);

/* Forget the cached plans. */
void village_cache_clear(void);

/* Write the part of the plan inside the canvas. */
void village_apply(const StructCanvas *cv, const VillagePlan *p);

/* The footprint's column heights etc. of the building's door: the world
 * position of the cell in front of its doorway (the path column), true if
 * the template has a doorway. */
bool village_door_outside(const VillageBuilding *b, int *x, int *z);

#endif
