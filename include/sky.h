#ifndef SKY_H
#define SKY_H

#include <stdbool.h>
#include <stdint.h>

#include "mesh.h"

/* Day and night (roadmap row 3-02, kickoff W-7: a full cycle). Plain C, no
 * 3DS calls, so the PC tests run it; skydraw.c puts it on the screen.
 *
 * The time of day is a number from 0 up to (not including) 1 that wraps
 * round: 0 is sunrise, the sun on the east horizon; 0.25 noon, the sun
 * straight overhead; 0.5 sunset, the sun on the west horizon; 0.75
 * midnight, the moon overhead. The sun and moon go round east, up, west and
 * under, always opposite each other. A whole day and night takes
 * SKY_DAY_SECONDS of play.
 *
 * The world is lit by how high the sun is: fully once it is a little above
 * the horizon, dimmed to SKY_NIGHT_LIGHT once it is a little below, with a
 * smooth fade between. The sky is the same blue as before row 3-02 for most
 * of the day, glows orange round sunrise and sunset, and goes a deep blue at
 * night. */

/* A test ROM can be built with a shorter day or another start time
 * (-DSKY_DAY_SECONDS=60.0f, -DSKY_NEW_WORLD=0.75f). */
#ifndef SKY_DAY_SECONDS
#define SKY_DAY_SECONDS 1200.0f /* 20 minutes */
#endif
/* A new world (and one saved before row 3-02) starts in the morning: the
 * sun a quarter of the way up, already full daylight. */
#ifndef SKY_NEW_WORLD
#define SKY_NEW_WORLD 0.06f
#endif

#define SKY_NIGHT_LIGHT 0.35f /* how bright the world is at night */
#define SKY_DAY_RGB 0x6EAAF0u /* the sky blue of rows 1-01 to 3-01 */
#define SKY_NIGHT_RGB 0x0E1228u
#define SKY_GLOW_RGB 0xF08C50u /* sunrise and sunset */

/* The sun and moon: squares facing the eye, SKY_DIST away (inside the far
 * plane, 160), drawn behind everything. */
#define SKY_DIST 100.0f
#define SKY_SUN_HALF 8.0f  /* half the sun's width: about 9 degrees across */
#define SKY_MOON_HALF 6.0f
#define SKY_QUADS_MAX (2 * MESH_FACE_VERTS)

typedef struct {
    float sun[3];      /* unit vector towards the sun; the moon is -sun */
    uint32_t clear;    /* the sky's colour, 0xRRGGBBAA */
    float light;       /* the world's brightness, SKY_NIGHT_LIGHT .. 1 */
    float sun_rgb[3];  /* the sun's colour: white, orange near the horizon */
    float moon_rgb[3]; /* the moon's colour: white (rain fades it, weather.h) */
    bool sun_shown;    /* any part of the sun is above the horizon */
    bool moon_shown;
} SkyLook;

/* The world's fog (row 3-04) starts this far out of its end (sky_fog()). */
#define SKY_FOG_START 0.6f

/* The time of day after `seconds` more of play, kept in 0 .. 1. */
float sky_advance(float t, float seconds);

/* How the sky and world look at time of day t (0 .. 1). */
void sky_look(float t, SkyLook *out);

/* The sun and moon squares that are shown, round the eye at the origin
 * (draw them with the view's turn but not its position, so they never come
 * nearer): each six vertices, counter-clockwise seen from the eye, the sun
 * with TILE_SUN in its colour, the moon with TILE_MOON in its own (white). Returns
 * how many vertices were written: 0, 6 or 12 (SKY_QUADS_MAX). */
int sky_quads(const SkyLook *look, MeshVertex out[SKY_QUADS_MAX]);

/* The world's fog at view distance `view` (row 3-04; stream.h): *end is
 * the nearest the edge of the drawn world can be, across the ground, so the
 * edge is always all fog and never seen; *start is SKY_FOG_START of it. */
void sky_fog(int view, float *start, float *end);

#endif
