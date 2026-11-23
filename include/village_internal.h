#ifndef VILLAGE_INTERNAL_H
#define VILLAGE_INTERNAL_H

#include "village.h"
#include "village_site.h"

/* What the village planner (village.c) and its helpers share (roadmap row
 * 4-05). Not for the rest of the game. */

#define MAX_PATHS 8
#define MAX_LANES 16
enum { OCC_FREE, OCC_PATH, OCC_BUILD, OCC_WELL, OCC_LAMP, OCC_OUT = 255 };
typedef struct {
    int ox, oz, dx, dz, n; /* column i is (ox + dx * i, oz + dz * i) */
    bool arm;
} Path;

typedef struct {
    int path, side, cursor; /* the side of the path (+1 / -1) buildings go on */
} Lane;

typedef struct {
    uint32_t seed, base;
    bool has_spawn;
    int spx, spz;
    VillagePlan *p;
    Path paths[MAX_PATHS];
    int npaths;
    Lane lanes[MAX_LANES];
    int nlanes;
} Ctx;

int occ_at(const Ctx *c, int x, int z);
void occ_set(const Ctx *c, int x, int z, int v);
void occ_clear(void);
uint32_t hv(const Ctx *c, int a, int b);
bool near_spawn(const Ctx *c, int x0, int z0, int W, int D, int margin);
void put_building(Ctx *c, int tmpl, int rot, int mirror, int x0, int z0, int W, int D, int fy, int mark);
void add_path_col(Ctx *c, int x, int z);
void place_lamps(Ctx *c);

#endif
