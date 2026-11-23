#ifndef RUINS_H
#define RUINS_H

#include <stdbool.h>
#include <stdint.h>

#include "structure.h"

/* Ruins (roadmap row 4-05): broken cobblestone and mossy cobblestone walls,
 * part fallen, part sunk into the ground. They lie on plains and hills, and
 * on the sea floor under water.
 *
 * A ruin follows the ground: the stones of each column stand from a little
 * below the ground (RUIN_SINK) up to how much of that wall is left, so
 * nothing floats, and a column that has fallen is just not there. They only
 * ever put stone in (never air), so under the sea the gaps stay water. One
 * ruin per ruin region at most (structure.h); never near a village. */

#define RUIN_MARGIN 32    /* the middle is at least this far inside the region's edge */
#define RUIN_COLS 49      /* most columns a ruin covers (7 x 7) */
#define RUIN_SEA_DEEPEST 54 /* the sea floor is no deeper than this under a sea ruin */

typedef struct {
    bool valid;
    bool sea;            /* on the sea floor */
    uint8_t shape, rot, mirror;
    int sink;            /* how many layers of the walls are below the ground */
    int x0, z0, width, depth; /* the footprint, turned */
    uint32_t salt;       /* seeds which stones are gone and which are mossy */
    uint8_t gh[RUIN_COLS]; /* the ground height under each column, [z * width + x] */
} RuinPlan;

/* The ruin of region (rx, rz) (RUIN_REGION_CHUNKS chunks wide), planned
 * afresh: false when there is none. The same answer every time. */
bool ruin_plan(uint32_t seed, int rx, int rz, RuinPlan *out);

/* The same with the spawn column given (see village_plan_spawn()). */
bool ruin_plan_spawn(uint32_t seed, int rx, int rz, bool has_spawn, int sx, int sz,
                     RuinPlan *out);

/* The same from the cache; NULL when the region has no ruin. */
const RuinPlan *ruin_get(uint32_t seed, int rx, int rz);

/* Forget the cached plans. */
void ruin_cache_clear(void);

/* Write the part of the ruin inside the canvas. */
void ruin_apply(const StructCanvas *cv, const RuinPlan *p);

/* How many stones tall the wall in column (lx, lz) of the footprint (local
 * x, z) is now: 0 for a column with no wall or one that fell. The first
 * stone is p->sink layers below the top ground block's top. */
int ruin_column_cap(const RuinPlan *p, int lx, int lz);

#endif
