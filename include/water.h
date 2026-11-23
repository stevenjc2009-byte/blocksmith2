#ifndef WATER_H
#define WATER_H

#include <stdbool.h>
#include <stdint.h>

#include "blocktex.h"
#include "world.h"

/* Water (roadmap row 4-01): still, see-through water in seas and rivers.
 * It is not solid (block.h), so the player and light go through it; the
 * aiming ray passes through it (raycast.h), a placed block takes its place
 * (place.h), and it cannot be broken (world.h), held (hotbar.h) or placed.
 * Its triangles are kept apart from the solid ones and drawn after them,
 * blended (water_mesh.h). Pure C, so the PC tests can run it. */

/* A water cell with no water above it is only WATER_TOP high: its surface
 * sits WATER_TOP_TEXELS of the tile's texels up the cell (0.875), a little
 * under the ground beside it, as in Minecraft (about 0.89). A whole number
 * of texels, so the lowered sides show whole rows of the tile. */
#define WATER_TOP_TEXELS 14
#define WATER_TOP ((float)WATER_TOP_TEXELS / (float)BLOCKTEX_TILE)

/* Under water the fog closes in: none out to WATER_FOG_START blocks from
 * the eye, all fog from WATER_FOG_END on (across the ground, like all fog,
 * gpu.h). */
#define WATER_FOG_START 1.0f
#define WATER_FOG_END 12.0f

/* Is the eye at (x, y, z) under water: in a water cell and below its
 * surface (WATER_TOP up the cell, or the whole cell when there is water
 * above)? */
bool water_eye_under(const World *w, float x, float y, float z);

/* The colour of the fog and the sky under water, 0xRRGGBBAA: a deep blue
 * times the world's light (sky.h: 1 by day, less at night), alpha 0xFF. */
uint32_t water_fog_color(float light);

#endif
