#ifndef RAYCAST_H
#define RAYCAST_H

#include <stdbool.h>

#include "world.h"

/* Which block the player is looking at (roadmap row 1-08). Pure C, so the
 * PC tests can run it. */

#define RAY_REACH 5.0f /* blocks from the eye: how far the player can reach */

typedef struct {
    int x, y, z;  /* the block hit */
    int face;     /* FACE_* (block.h) of that block the ray came in through */
    float dist;   /* blocks from the start to where the ray met that face */
} RayHit;

/* Walk the ray from (ox, oy, oz) along (dx, dy, dz) (any length but zero)
 * block by block, up to reach blocks. True, with *hit filled, at the first
 * block that is not air. The block the ray starts in is not checked.
 * Nothing outside the world is ever hit: world_get() says air there. */
bool raycast(const World *w, float ox, float oy, float oz,
             float dx, float dy, float dz, float reach, RayHit *hit);

#endif
