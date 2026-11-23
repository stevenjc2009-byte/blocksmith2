#include <3ds.h>

#include "block.h"
#include "framebuffer.h"
#include "gpu.h"
#include "mat4.h"
#include "mesh.h"

/* Roadmap row 1-03: one grass block, textured with tiles drawn in code,
 * turning on the top screen. The bottom screen stays a solid colour.
 * START quits. */

#define PI 3.14159265f
#define SPIN_MS 6000 /* one full turn every 6 seconds: 60 degrees a second */

static void fill_screen(gfxScreen_t screen, u8 r, u8 g, u8 b)
{
    u16 width = 0, height = 0;
    u8 *fb = gfxGetFramebuffer(screen, GFX_LEFT, &width, &height);
    fb_fill_bgr8(fb, width * height, r, g, b);
}

/* Camera 2.6 units back, looking down a little (the cube tipped 30 degrees
 * towards it), the cube turned `angle` about its upright axis. */
static void cube_matrix(Mat4 *mvp, float angle)
{
    Mat4 proj, m, t;

    mat4_persp_tilt(&proj, 60.0f * PI / 180.0f, 400.0f / 240.0f, 0.1f, 100.0f);
    mat4_translate(&m, 0.0f, 0.0f, -2.6f);
    mat4_rotate_x(&t, 30.0f * PI / 180.0f);
    mat4_mul(&m, &m, &t);
    mat4_rotate_y(&t, angle);
    mat4_mul(&m, &m, &t);
    mat4_mul(mvp, &proj, &m);
}

int main(void)
{
    MeshVertex *cube = NULL;
    u64 start;
    int ok;

    gfxInitDefault();
    ok = gpu_init();
    if (ok) {
        cube = linearAlloc(MESH_BLOCK_VERTS * sizeof *cube);
        ok = cube && mesh_block(cube, -0.5f, -0.5f, -0.5f, BLOCK_GRASS) == MESH_BLOCK_VERTS;
    }
    if (!ok) {
        if (cube)
            linearFree(cube);
        gpu_exit();
        gfxExit();
        return 1;
    }
    GSPGPU_FlushDataCache(cube, MESH_BLOCK_VERTS * sizeof *cube);

    start = osGetTime();
    while (aptMainLoop()) {
        Mat4 mvp;
        u64 ms;

        hidScanInput();
        if (hidKeysDown() & KEY_START)
            break;

        fill_screen(GFX_BOTTOM, 80, 150, 60);  /* grass green */
        gfxFlushBuffers();

        ms = (osGetTime() - start) % SPIN_MS;
        cube_matrix(&mvp, (float)ms * (2.0f * PI / SPIN_MS));

        gpu_frame_begin();
        gpu_draw(cube, MESH_BLOCK_VERTS, &mvp);
        gpu_frame_end();

        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    linearFree(cube);
    gpu_exit();
    gfxExit();
    return 0;
}
