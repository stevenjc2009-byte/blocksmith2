#ifndef VILLAGE_SITE_H
#define VILLAGE_SITE_H

#include <stdbool.h>
#include <stdint.h>

/* The ground round a village's middle, looked at once (roadmap row 4-05):
 * the height and kind of every column from 44 blocks west and north of the
 * middle to 44 east and south of it. Village planning works from this grid
 * instead of asking terrain_column() again and again. One grid is held at a
 * time: village_site.c is only used by village.c while it plans. */

#define SITE_HALF 44
#define SITE_SIZE (2 * SITE_HALF + 1)

/* Fill the grid round world column (cx, cz) from the seed. */
void site_load(uint32_t seed, int cx, int cz);

/* True for a column of plains or hills inside the grid; false for water,
 * beach, river, mountains and anything outside it. */
bool site_land(int x, int z);

/* y of the top ground block of a column inside the grid, -1 outside. */
int site_height(int x, int z);

#endif
