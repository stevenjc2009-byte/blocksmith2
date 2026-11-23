#ifndef HAND_H
#define HAND_H

#include <stdbool.h>

#include "camera.h"
#include "mat4.h"
#include "player.h"
#include "world.h"

/* What the player's hand does (roadmap row 3-05): R breaks the block under
 * the crosshair and throws its bits (particles.h), L puts the chosen block
 * against the face looked at, both swing the held block (held.h), and the
 * held block and the bits are drawn. 3DS only (it draws); the rules it uses
 * are in place.c, particles.c and held.c, which the PC tests run.
 *
 * The held block and the bits are lit by the torches round them: the bits
 * by the light of the cell they broke from (a torch's bits glow), the held
 * block by the light of the cell the eye is in, worked out again when the
 * eye moves to another cell, a torch is placed or broken, the hand changes
 * a block, or HAND_LIGHT_REFRESH seconds have gone (chunks loaded). */

#define HAND_LIGHT_REFRESH 0.5f /* seconds */

/* R: swing, break the block under the crosshair (if one is in reach), throw
 * its bits (and a torch's that stood on it) and rebuild the chunks it
 * changes. False only if a rebuild ran out of memory. */
bool hand_break(World *w, const Camera *cam);

/* L: swing, put block b against the face under the crosshair (not inside
 * the player) and rebuild the chunks round the new block. False only if a
 * rebuild ran out of memory. */
bool hand_place(World *w, const Camera *cam, const Player *player, int b);

/* Move the swing and the bits on by dt seconds. */
void hand_step(const World *w, float dt);

/* Draw the bits (in the world: mvp, after the chunks, with the world's fog)
 * and then block `held` (BLOCK_*; air: nothing) in the bottom right, over
 * everything (proj, no fog). After meshes_draw(); leaves the fog off. */
void hand_draw(const World *w, const Camera *cam, const Mat4 *proj, const Mat4 *mvp, int held);

/* Free the bits' and the held block's vertex memory. */
void hand_exit(void);

#endif
