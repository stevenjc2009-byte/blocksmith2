#ifndef WATER_RULES_H
#define WATER_RULES_H

#include <stdbool.h>

#include "world.h"

/* The rules of a water cell's turn (row 4-04, water_flow.h), apart from
 * the queue. They only read the world, so the stability test can ask what
 * a turn would do without doing it. Pure C, so the PC tests can run them. */

/* One cell a turn would write: block b at (x, y, z). */
typedef struct {
    int x, y, z, b;
} WaterWrite;

/* Most cells water spreads to in one turn: down, or the four sides. */
#define WATER_SPREAD_MAX 4

/* What flowing or falling cell (x, y, z) should now be (water_flow.h, step
 * 1): BLOCK_WATER, BLOCK_FALL, a BLOCK_FLOW_* or BLOCK_AIR. */
int water_rules_new(const World *w, int x, int y, int z);

/* Where water b at (x, y, z) spreads (water_flow.h, step 2): writes the
 * cells and what each becomes to out, down first or the sides in FACE_*
 * order, and returns how many (0 to WATER_SPREAD_MAX). */
int water_rules_spread(const World *w, int x, int y, int z, int b,
                       WaterWrite out[WATER_SPREAD_MAX]);

/* Is every chunk within WATER_FLOW_REACH blocks of column (x, z) loaded, so
 * a turn there reads only real blocks? */
bool water_rules_ready(const World *w, int x, int z);

/* Would a turn of cell (x, y, z) change anything: would the cell become
 * something else, or spread? False for a cell that is not water. */
bool water_rules_would_change(const World *w, int x, int y, int z);

#endif
