#ifndef HELD_H
#define HELD_H

#include "mat4.h"
#include "mesh.h"

/* The held block (roadmap row 3-05, kickoff I-7): the chosen hotbar slot's
 * block, shown in the bottom right of the top screen, turned so its top and
 * two sides show. R and L swing it: it dips forward and down and comes back
 * over HELD_SWING_TIME seconds. An empty slot shows nothing. Plain C, no
 * 3DS calls, so the PC tests run it; hand.c draws it over everything else
 * (no depth test: it is never hidden by a wall the player stands against).
 *
 * Its place is in view space (the camera at the origin looking down -z,
 * +x right, +y up, camera.h): its middle at (HELD_X, HELD_Y, HELD_Z), turned
 * HELD_TURN radians about the vertical, HELD_SIZE blocks across. */

#define HELD_X 0.72f
#define HELD_Y -0.31f
#define HELD_Z -1.3f
#define HELD_TURN 0.7853982f  /* 45 degrees: a corner towards the eye */
#define HELD_SIZE 0.45f
#define HELD_SWING_TIME 0.25f /* seconds */
#define HELD_SWING_TIP 0.6f   /* radians it tips forward at the swing's deepest */
#define HELD_SWING_DIP 0.16f  /* how far it dips down and in then */

typedef struct {
    float swing; /* seconds of the swing left; 0: still */
} Held;

/* Still. */
void held_init(Held *h);

/* Start a swing, or start it again if one is going. */
void held_swing(Held *h);

/* Move the swing on by dt seconds. */
void held_step(Held *h, float dt);

/* How deep in the swing it is: 0 still, rising to 1 halfway through and
 * back to 0 at the end. */
float held_dip(const Held *h);

/* The held block's vertices: block `block` (BLOCK_*) as mesh_block() makes
 * it, moved so the middle of its cell is at the origin, with torch light at
 * least t on every vertex (a torch keeps its own glow). Returns the number
 * of vertices written: 0 for air or a value that is no block. */
int held_build(int block, float t, MeshVertex out[MESH_BLOCK_VERTS]);

/* Where held_build()'s vertices go in view space, swing and all. Put a
 * projection in front of it to draw. */
void held_place(Mat4 *out, const Held *h);

#endif
