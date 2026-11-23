#ifndef SKYDRAW_H
#define SKYDRAW_H

#include "camera.h"
#include "mat4.h"

/* Day and night on the top screen (roadmap row 3-02): the sky's colour, the
 * sun and moon, and the world's light, from sky.h; the clouds (cloud.h) and
 * the fog (row 3-04). 3DS calls, so not in the PC tests.
 *
 * Call between gpu_frame_begin() and the world's draws: it sets the colour
 * the frame is cleared to and the fog's to the sky's, draws the sun and moon
 * behind everything (`cam`'s turn and `proj`, not its position, so they
 * never come nearer), sets the light the world is drawn with, draws the
 * clouds round the eye with `mvp` (the world's), lit by that light and
 * fogged out by CLOUD_RADIUS, then sets the world's fog for view distance
 * `view` (sky_fog()). */
void skydraw(float t, const Camera *cam, const Mat4 *proj, const Mat4 *mvp, int view);

/* Free the sun and moon's and the clouds' vertex memory. */
void skydraw_exit(void);

#endif
