#include <3ds.h>

#include "cloud.h"
#include "gpu.h"
#include "sky.h"
#include "skydraw.h"

/* The sun and moon squares, in linear memory for the GPU. Written each
 * frame before any draw: the last frame's gpu_frame_end() has waited for
 * the GPU, so it is no longer reading them. Made on first use; if there is
 * no memory for them the sky's colour and the light still change. */
static MeshVertex *s_quads;
static MeshVertex *s_clouds; /* CLOUD_VERTS_MAX, the same way (row 3-04) */
static MeshVertex *s_drops;  /* WEATHER_VERTS_MAX, the same way (row 3-06) */

void skydraw(float t, const Weather *weather, const World *world, const Camera *cam,
             const Mat4 *proj, const Mat4 *mvp, int view)
{
    SkyLook look;
    Camera turn = *cam;
    Mat4 turned, sky;
    float start, end, grey;
    int n, i;

    sky_look(t, &look);
    weather_sky(&look, weather->strength);
    gpu_set_clear(look.clear);
    gpu_set_fog_color(look.clear);
    if (!s_quads)
        s_quads = linearAlloc(SKY_QUADS_MAX * sizeof *s_quads);
    if (s_quads) {
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
    if (s_clouds) {
        n = cloud_build(cam->x, cam->z, t, s_clouds);
        if (n > 0) { /* only the vertices written this frame */
            grey = 1.0f - WEATHER_CLOUD_GREY * weather->strength;
            for (i = 0; i < n; i++) {
                s_clouds[i].r *= grey;
                s_clouds[i].g *= grey;
                s_clouds[i].b *= grey;
            }
            GSPGPU_FlushDataCache(s_clouds, n * sizeof *s_clouds);
            gpu_set_fog(cam->x, cam->z, CLOUD_FOG_START, CLOUD_RADIUS);
            gpu_draw(s_clouds, n, mvp);
        }
    }
    sky_fog(view, &start, &end);
    gpu_set_fog(cam->x, cam->z, start, end);
    if (!s_drops)
        s_drops = linearAlloc(WEATHER_VERTS_MAX * sizeof *s_drops);
    if (s_drops) {
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
}
