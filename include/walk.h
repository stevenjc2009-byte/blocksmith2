#ifndef WALK_H
#define WALK_H

/* Walking with the Circle Pad (roadmap row 1-07, kickoff C-1). Pure C, no
 * 3DS calls, so the PC tests can run it. */

#define WALK_SPEED 4.3f    /* blocks a second at a full push */
#define WALK_PAD_DEAD 15   /* Circle Pad pushes this small or smaller are ignored */
#define WALK_PAD_FULL 150  /* Circle Pad push that gives full speed (it reaches about 156) */

/* Walking velocity in blocks a second along world x (east) and z (south),
 * from the Circle Pad (right and up positive) and the camera's yaw: up
 * walks the way the camera faces, right steps to the camera's right. Speed
 * grows with how far the pad is pushed and never goes past WALK_SPEED,
 * diagonals included. */
void walk_velocity(int pad_x, int pad_y, float yaw, float *vx, float *vz);

#endif
