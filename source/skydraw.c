#include <3ds.h>
#include <string.h>

#include "cloud.h"
#include "gpu.h"
#include "sky.h"
#include "skydraw.h"
#include "water.h"

/* The sun and moon squares, in linear memory for the GPU. Written each
 * frame before any draw: the last frame's gpu_frame_end() has waited for
 * the GPU, so it is no longer reading them. Made on first use; if there is
 * no memory for them the sky's colour and the light still change. */
static MeshVertex *s_quads;
static MeshVertex *s_clouds; /* CLOUD_VERTS_MAX (row 3-04), the same way, but */
static CloudCells s_cells;   /* written only when these cells or this grey */
static float s_grey = -1.0f; /* change (fix batch, problem 9; -1: never) */
static int s_cloud_n;
static MeshVertex *s_drops;  /* WEATHER_VERTS_MAX, the same way (row 3-06) */

void skydraw(float t, const Weather *weather, const World *world, const Camera *cam,
             const Mat4 *proj, const Mat4 *mvp, int view)
{
    SkyLook look;
    Camera turn = *cam;
    Mat4 turned, sky;
    float start, end, grey;
    bool under = water_eye_under(world, cam->x, cam->y, cam->z); /* row 4-01 */
    int n, i;

    sky_look(t, &look);
    weather_sky(&look, weather->strength);
    if (under)
        look.clear = water_fog_color(look.light);
    gpu_set_clear(look.clear);
    gpu_set_fog_color(look.clear);
    if (!s_quads)
        s_quads = linearAlloc(SKY_QUADS_MAX * sizeof *s_quads);
    if (s_quads && !under) {
        n = sky_quads(&look, s_quads);
        GSPGPU_FlushDataCache(s_quads, SKY_QUADS_MAX * sizeof *s_quads);
        turn.x = turn.y = turn.z = 0.0f;
        camera_view(&turned, &turn);
        mat4_mul(&sky, proj, &turned);
        gpu_draw_sky(s_quads, n, &sky); /* no fog yet: gpu_frame_begin() */
    }
    gpu_set_light(look.light);
    if (!s_clouds)
        s_clouds = linearAlloc(CLOUD_VERTS_MAX * sizeof *s_clouds);
    if (s_clouds && !under) {
        /* Built in the pattern, before the drift; the matrix moves them
         * on, and the fog is measured from the eye in the pattern too. */
        float shift = cloud_shift(t), px = cam->x - shift;
        CloudCells cells;
        Mat4 move, moved;

        cloud_cells(px, cam->z, &cells);
        grey = 1.0f - WEATHER_CLOUD_GREY * weather->strength;
        if (grey != s_grey || memcmp(&cells, &s_cells, sizeof cells) != 0) {
            n = s_cloud_n = cloud_build(px, cam->z, 0.0f, s_clouds);
            for (i = 0; i < n; i++) {
                s_clouds[i].r *= grey;
                s_clouds[i].g *= grey;
                s_clouds[i].b *= grey;
            }
            GSPGPU_FlushDataCache(s_clouds, n * sizeof *s_clouds);
            s_cells = cells;
            s_grey = grey;
        }
        if (s_cloud_n > 0) {
            mat4_translate(&move, shift, 0.0f, 0.0f);
            mat4_mul(&moved, mvp, &move);
            gpu_set_fog(px, cam->z, CLOUD_FOG_START, CLOUD_RADIUS);
            gpu_draw(s_clouds, s_cloud_n, &moved);
        }
    }
    sky_fog(view, &start, &end);
    if (under) {
        start = WATER_FOG_START;
        end = WATER_FOG_END;
    }
    gpu_set_fog(cam->x, cam->z, start, end);
    if (!s_drops)
        s_drops = linearAlloc(WEATHER_VERTS_MAX * sizeof *s_drops);
    if (s_drops && !under) { /* the rain stops at the water: none to see */
        n = weather_build(weather, world, cam, s_drops);
        if (n > 0) {
            GSPGPU_FlushDataCache(s_drops, n * sizeof *s_drops);
            gpu_draw(s_drops, n, mvp);
        }
    }
}

void skydraw_exit(void)
{
    if (s_quads)
        linearFree(s_quads);
    if (s_clouds)
        linearFree(s_clouds);
    if (s_drops)
        linearFree(s_drops);
    s_quads = s_clouds = s_drops = NULL;
    s_grey = -1.0f;
}
