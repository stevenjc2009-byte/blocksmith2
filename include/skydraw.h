#ifndef SKYDRAW_H
#define SKYDRAW_H

#include "camera.h"
#include "mat4.h"

/* Day and night on the top screen (roadmap row 3-02): the sky's colour, the
 * sun and moon, and the world's light, from sky.h. 3DS calls, so not in the
 * PC tests.
 *
 * Call between gpu_frame_begin() and the world's draws: it sets the colour
 * the frame is cleared to, draws the sun and moon behind everything (`cam`'s
 * turn and `proj`, not its position, so they never come nearer), then sets
 * the light the world is drawn with. */
void skydraw(float t, const Camera *cam, const Mat4 *proj);

/* Free the sun and moon's vertex memory. */
void skydraw_exit(void);

#endif
