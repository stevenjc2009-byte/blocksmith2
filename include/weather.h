#ifndef WEATHER_H
#define WEATHER_H

#include <stdbool.h>
#include <stdint.h>

#include "biome.h"
#include "camera.h"
#include "mesh.h"
#include "sky.h"
#include "world.h"

/* Weather (roadmap row 3-06, kickoff W-9): rain, and snow in cold places.
 * Plain C, no 3DS calls, so the PC tests run it; skydraw.c puts it on the
 * screen.
 *
 * The sky is clear for WEATHER_CLEAR_MIN to WEATHER_CLEAR_MAX seconds of
 * play, then it rains for WEATHER_RAIN_MIN to WEATHER_RAIN_MAX, then it is
 * clear again, and so on; each spell's length is picked at random from a
 * sequence that starts from the world's seed and is saved with the world.
 * The rain comes and goes over WEATHER_FADE seconds (the strength, 0 clear
 * to 1 full rain): the more of it, the more columns it falls in, the greyer
 * the sky and clouds and the darker the world (WEATHER_DIM less light in
 * full rain); the sun and moon fade into the grey and are gone by half.
 *
 * It falls in the block columns within WEATHER_RADIUS of the eye, across
 * the ground, WEATHER_PER_COLUMN drops a column, each at its own place in
 * the column, from above the eye down to the top of the column's highest
 * block (never under a roof). In a warm column a drop is a thin streak of
 * rain falling fast, in a cold one a flake of snow falling slowly and
 * drifting. A drop belongs to its column of the world, not to the eye, so
 * walking does not drag the rain along.
 *
 * Which places are cold, and which dry, comes from the biomes (row 4-02,
 * biome.h): snow in the snowy temperature band and on mountains from the
 * snow line up (lower where it is cold), rain elsewhere, and nothing at
 * all in deserts, savannas and badlands (which come from the same climate:
 * a dry place is dry over its seas too). */

#define WEATHER_CLEAR_MIN 600.0f  /* seconds: 10 to 25 minutes clear */
#define WEATHER_CLEAR_MAX 1500.0f
#define WEATHER_RAIN_MIN 180.0f   /* seconds: 3 to 6 minutes of rain */
#define WEATHER_RAIN_MAX 360.0f
#define WEATHER_FADE 10.0f        /* seconds for the rain to come or go */
#define WEATHER_DIM 0.25f         /* the share of the light full rain takes */
#define WEATHER_GREY 0.75f        /* how far full rain turns the sky grey */
#define WEATHER_CLOUD_GREY 0.4f   /* the share of the clouds' brightness it takes */

#define WEATHER_RADIUS 8          /* blocks across the ground */
#define WEATHER_PER_COLUMN 2
#define WEATHER_BAND 16.0f        /* a column's drops repeat every this many blocks up */
#define WEATHER_RAIN_SPEED 12.0f  /* blocks a second down */
#define WEATHER_RAIN_LEN 1.0f     /* a streak's height */
#define WEATHER_RAIN_WIDTH (1.0f / 16.0f)
#define WEATHER_SNOW_SPEED 2.0f
#define WEATHER_SNOW_SIZE 0.125f  /* a flake's width and height */
#define WEATHER_SNOW_SWAY 0.3f    /* blocks either way it drifts */
#define WEATHER_SNOW_SWAY_TIME 4.0f /* seconds for one drift there and back */
/* The clock the drops fall by goes back to 0 here; every fall and drift
 * above comes round a whole number of times in it, so nothing jumps. */
#define WEATHER_CLOCK_WRAP 3600.0f

#define WEATHER_COLD (BIOME_T_COOL / BIOME_UNIT) /* a place is cold below this, of 0 .. 255 */

#define WEATHER_COLUMNS_MAX ((2 * WEATHER_RADIUS + 1) * (2 * WEATHER_RADIUS + 1))
#define WEATHER_VERTS_MAX (WEATHER_COLUMNS_MAX * WEATHER_PER_COLUMN * MESH_FACE_VERTS)

/* A test ROM can start a new world (or one saved before row 3-06) in full
 * rain (-DWEATHER_NEW_RAIN=1), and can make every column west of x =
 * WEATHER_TEST_COLD_X cold and every other warm, with no dry place at all
 * (-DWEATHER_TEST_COLD_X=8; a very low or high value: all warm or all
 * cold). */
#ifndef WEATHER_NEW_RAIN
#define WEATHER_NEW_RAIN 0
#endif

typedef struct {
    bool raining;    /* rain is falling or coming */
    float left;      /* seconds until it turns */
    float strength;  /* 0 clear .. 1 full rain, following raining */
    float clock;     /* seconds the drops fall by, 0 .. WEATHER_CLOCK_WRAP */
    uint32_t rng;    /* the random sequence for the spells' lengths */
} Weather;

/* A new world's weather (and that of a world saved before row 3-06):
 * clear, the rain's first spell picked from the seed's sequence. */
void weather_init(Weather *w, uint32_t seed);
/* The weather `dt` more seconds of play on. */
void weather_step(Weather *w, float dt);
/* The climate's temperature at world column (x, z), 0 .. 255 (biome.h:
 * the same map the biomes use); a column is cold below WEATHER_COLD. */
int weather_temp(uint32_t seed, int x, int z);
/* Is world column (x, z) cold: does snow fall at ground level there? */
bool weather_cold(uint32_t seed, int x, int z);
/* Does snow fall on column (x, z) when its highest block is at y `top` - 1:
 * cold, or on a mountain at or above the snow line (biome_snow_line())? */
bool weather_snows(uint32_t seed, int x, int z, int top);
/* Is world column (x, z) dry: no rain or snow at all (desert, savanna and
 * badlands climates)? */
bool weather_dry(uint32_t seed, int x, int z);
/* The sky and world as the weather leaves them: nothing changed at
 * strength 0. */
void weather_sky(SkyLook *look, float strength);
/* The drops falling round the eye now, as squares (two triangles each,
 * counter-clockwise seen from the eye), plain white tile TILE_CLOUD,
 * no torch light. Returns how many vertices were written. */
int weather_build(const Weather *w, const World *world, const Camera *cam,
                  MeshVertex out[WEATHER_VERTS_MAX]);

#endif
