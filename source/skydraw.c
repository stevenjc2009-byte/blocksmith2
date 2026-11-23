#include <3ds.h>

#include "gpu.h"
#include "sky.h"
#include "skydraw.h"

/* The sun and moon squares, in linear memory for the GPU. Written each
 * frame before any draw: the last frame's gpu_frame_end() has waited for
 * the GPU, so it is no longer reading them. Made on first use; if there is
 * no memory for them the sky's colour and the light still change. */
static MeshVertex *s_quads;

void skydraw(float t, const Camera *cam, const Mat4 *proj)
{
    SkyLook look;
    Camera turn = *cam;
    Mat4 view, mvp;
    int n;

    sky_look(t, &look);
    gpu_set_clear(look.clear);
    if (!s_quads)
        s_quads = linearAlloc(SKY_QUADS_MAX * sizeof *s_quads);
    if (s_quads) {
        n = sky_quads(&look, s_quads);
        GSPGPU_FlushDataCache(s_quads, SKY_QUADS_MAX * sizeof *s_quads);
        turn.x = turn.y = turn.z = 0.0f;
        camera_view(&view, &turn);
        mat4_mul(&mvp, proj, &view);
        gpu_draw_sky(s_quads, n, &mvp);
    }
    gpu_set_light(look.light);
}

void skydraw_exit(void)
{
    if (s_quads)
        linearFree(s_quads);
    s_quads = NULL;
}
