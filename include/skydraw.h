#ifndef SKYDRAW_H
#define SKYDRAW_H

#include "camera.h"
#include "mat4.h"
#include "weather.h"
#include "world.h"

/* Day and night on the top screen (roadmap row 3-02): the sky's colour, the
 * sun and moon, and the world's light, from sky.h; the clouds (cloud.h) and
 * the fog (row 3-04); the weather (weather.h, row 3-06). 3DS calls, so not
 * in the PC tests.
 *
 * Call between gpu_frame_begin() and the world's draws: it sets the colour
 * the frame is cleared to and the fog's to the sky's, greyed by the rain,
 * draws the sun and moon behind everything (`cam`'s turn and `proj`, not its
 * position, so they never come nearer), sets the light the world is drawn
 * with, draws the clouds round the eye with `mvp` (the world's), lit by that
 * light, greyed by the rain and fogged out by CLOUD_RADIUS, sets the world's
 * fog for view distance `view` (sky_fog()), then draws the rain or snow
 * falling round the eye in `world`. With the eye under water (row 4-01,
 * water_eye_under()) the sky and the fog are water_fog_color() instead,
 * the fog runs from WATER_FOG_START to WATER_FOG_END, and the sun, moon,
 * clouds and rain are not drawn. */
void skydraw(float t, const Weather *weather, const World *world, const Camera *cam,
             const Mat4 *proj, const Mat4 *mvp, int view);

/* Free the sun and moon's, the clouds' and the weather's vertex memory. */
void skydraw_exit(void);

#endif
