#ifndef LOOK_H
#define LOOK_H

#include <stdbool.h>

/* Turning the camera (roadmap row 1-04, kickoff C-2): the C-stick on New
 * 3DS, or A B X Y on any 3DS, laid out as a diamond: X looks up, B down,
 * Y left, A right. */

typedef struct {
    bool up, down, left, right; /* X, B, Y, A held */
    int cx, cy;                 /* C-stick, right and up positive; 0 on Old 3DS */
} LookInput;

#define LOOK_DEG_PER_S 120.0f /* full turning speed; a look-speed setting comes later (SC-5) */
#define LOOK_STICK_DEAD 15    /* C-stick pushes this small or smaller are ignored */
#define LOOK_STICK_FULL 140   /* C-stick push that gives full speed */

/* Turning speeds in radians a second: yaw positive turns right, pitch
 * positive looks up. Buttons give full speed, the C-stick up to full speed
 * by how far it is pushed; together they never go past full speed. */
void look_rates(const LookInput *in, float *yaw_rate, float *pitch_rate);

#endif
