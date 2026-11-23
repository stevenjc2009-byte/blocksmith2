#ifndef CONTROLS_H
#define CONTROLS_H

#include <3ds.h>

#include "camera.h"
#include "look.h"

/* Reading the Circle Pad, the look keys and the C-stick while playing. 3DS
 * calls, so not in the PC tests; what they turn into is worked out in
 * walk.c and look.c, which are. */

/* Walking speed from the Circle Pad, by where the camera faces. */
void read_walk(const Camera *cam, float *vx, float *vz);

/* The look keys held, leaving out those in ignore. */
void read_look(LookInput *in, u32 ignore);

#endif
