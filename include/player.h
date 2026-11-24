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

/* Water (roadmap row 4-01): the body is "in water" when the block at its
 * feet is water. In water it floats with the water line SWIM_LINE up the
 * body (chest high, so the eye is 0.42 above the surface), walks at half
 * speed, and pushing into a bank one block above the surface kicks it up
 * and out. There is no dive button. */
#define PLAYER_SWIM_LINE 1.2f   /* feet to water line when floating */
#define PLAYER_SWIM_SCALE 0.5f  /* walking speed in water, against on land */
#define PLAYER_SWIM_PULL 4.0f   /* speed toward the float height, per block off it */
#define PLAYER_SWIM_MAX 3.0f    /* fastest the water carries it up or down */
#define PLAYER_SWIM_EASE 12.0f  /* how fast its speed settles on that, a second */
#define PLAYER_SWIM_KICK 12.0f  /* up speed to climb out: rises 2.57, clears 2.2 */

/* Flowing water (row 4-04): in water, the current of the cell at the feet
 * (water_current(), water.h: one block long, along the flow) carries the
 * body PLAYER_CURRENT blocks a second on top of walking, which is 0.014
 * blocks each Minecraft tick (1/20 s), Minecraft's push. Still and
 * falling water have no current. In flowing water thinner than still
 * water, the surface the body floats at is lower by as much as the cell's
 * drawn top is (water_height()). */
#define PLAYER_CURRENT 0.28f

typedef struct {
    float x, y, z;   /* feet, middle of the box */
    float vy;        /* up speed, blocks a second */
    bool on_ground;  /* standing on something after the last step */
} Player;

/* Does block (x, y, z) stop the player? Solid blocks (block_solid()) do;
 * air and torches (row 3-03) do not. */
bool player_solid(const World *w, int x, int y, int z);

/* Would the body fit with its feet at (x, y, z), touching no solid block? */
bool player_fits(const World *w, float x, float y, float z);

/* Is the body in water: is the block at its feet water? Sets *surface, if
 * given, to the height of the top of that run of water. */
bool player_in_water(const World *w, const Player *p, float *surface);

/* Stand the player in the middle of column (x, z), on its top solid block
 * (over any torch standing on it). */
void player_spawn(Player *p, const World *w, int x, int z);

/* Move for dt seconds, walking at (vx, vz) blocks a second (from
 * walk_velocity()). Falls under gravity, stops flush against blocks, and
 * when walking into a one-block step while standing, hops so the next
 * steps carry it up; in water, floats and swims (see above). Moves in pieces under half a block, so even a long dt
 * cannot pass through a block. */
void player_step(Player *p, const World *w, float vx, float vz, float dt);

#endif
