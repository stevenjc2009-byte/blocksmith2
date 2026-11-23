#ifndef PLACE_H
#define PLACE_H

#include <stdbool.h>

#include "player.h"
#include "raycast.h"
#include "world.h"

/* Placing blocks (roadmap row 1-09): L puts a block against the face of the
 * block under the crosshair. Pure C, no 3DS calls, so the PC tests can run
 * it. */

/* The spot a hit's face opens onto, where a new block goes: one block from
 * the hit block, out through that face. */
void place_spot(const RayHit *hit, int *x, int *y, int *z);

/* Does block (x, y, z) overlap the player's body? Touching a face is not
 * overlapping, the same rule player_fits() uses, so a block placed beside
 * the player never leaves them stuck. */
bool place_hits_player(const Player *p, int x, int y, int z);

/* Put block b in the spot the hit's face opens onto, written to *x, *y, *z.
 * False, and nothing changes, if b is air or not a block, if that spot is
 * outside the world or not air, or if the new block would overlap the
 * player. */
bool place_block(World *w, const Player *p, const RayHit *hit, int b, int *x, int *y, int *z);

#endif
