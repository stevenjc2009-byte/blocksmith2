#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>

#include "world.h"

/* The player's body (roadmap row 1-07): gravity, no walking through
 * blocks, and an automatic hop up one-block steps; there is no jump button
 * (kickoff C-4). Pure C, no 3DS calls, so the PC tests can run it.
 *
 * The body is a box 0.6 blocks wide and deep and 1.8 tall, standing on
 * (x, y, z): y is the feet, x and z the middle. The eye is 1.62 up. Below
 * the world counts as solid, and so does every chunk that is not loaded
 * (a wall, so nobody walks into ground not made yet); above it is air. */

#define PLAYER_HALF 0.3f       /* half the width and depth */
#define PLAYER_HEIGHT 1.8f
#define PLAYER_EYE 1.62f       /* feet to eye */
#define PLAYER_GRAVITY 28.0f   /* blocks a second, each second */
#define PLAYER_FALL_MAX 40.0f  /* fastest fall, blocks a second */
#define PLAYER_HOP_SPEED 8.4f  /* rises 1.26 blocks: clears a one-block step */

typedef struct {
    float x, y, z;   /* feet, middle of the box */
    float vy;        /* up speed, blocks a second */
    bool on_ground;  /* standing on something after the last step */
} Player;

/* Does block (x, y, z) stop the player? */
bool player_solid(const World *w, int x, int y, int z);

/* Would the body fit with its feet at (x, y, z), touching no solid block? */
bool player_fits(const World *w, float x, float y, float z);

/* Stand the player in the middle of column (x, z), on its top block. */
void player_spawn(Player *p, const World *w, int x, int z);

/* Move for dt seconds, walking at (vx, vz) blocks a second (from
 * walk_velocity()). Falls under gravity, stops flush against blocks, and
 * when walking into a one-block step while standing, hops so the next
 * steps carry it up. Moves in pieces under half a block, so even a long dt
 * cannot pass through a block. */
void player_step(Player *p, const World *w, float vx, float vz, float dt);

#endif
