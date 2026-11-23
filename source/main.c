#include <3ds.h>

#include "block.h"
#include "camera.h"
#include "framebuffer.h"
#include "gpu.h"
#include "look.h"
#include "mat4.h"
#include "mesh.h"

/* Roadmap row 1-04: look around. A 9 x 9 grass floor with a stone pillar
 * to the north and a dirt pillar to the east. The C-stick (New 3DS) or
 * A B X Y turn the camera. The bottom screen stays a solid colour.
 * START quits. */

#define PI 3.14159265f
#define FLOOR_HALF 4                 /* floor runs from -4 to 4 in x and z */
#define FLOOR_BLOCKS ((2 * FLOOR_HALF + 1) * (2 * FLOOR_HALF + 1))
#define SCENE_BLOCKS (FLOOR_BLOCKS + 4)
#define SCENE_VERTS (SCENE_BLOCKS * MESH_BLOCK_VERTS)

static void fill_screen(gfxScreen_t screen, u8 r, u8 g, u8 b)
{
    u16 width = 0, height = 0;
    u8 *fb = gfxGetFramebuffer(screen, GFX_LEFT, &width, &height);
    fb_fill_bgr8(fb, width * height, r, g, b);
}

/* Floor blocks fill y 0..1; the pillars stand two blocks high on it. */
static int build_scene(MeshVertex *out)
{
    int n = 0, x, z, y;

    for (z = -FLOOR_HALF; z <= FLOOR_HALF; z++)
        for (x = -FLOOR_HALF; x <= FLOOR_HALF; x++)
            n += mesh_block(out + n, (float)x, 0.0f, (float)z, BLOCK_GRASS);
    for (y = 1; y <= 2; y++) {
        n += mesh_block(out + n, 0.0f, (float)y, -4.0f, BLOCK_STONE); /* north */
        n += mesh_block(out + n, 4.0f, (float)y, 0.0f, BLOCK_DIRT);   /* east */
    }
    return n;
}

static void read_look(LookInput *in)
{
    u32 held = hidKeysHeld();
    circlePosition cs = { 0, 0 };

    hidCstickRead(&cs);
    in->up = (held & KEY_X) != 0;
    in->down = (held & KEY_B) != 0;
    in->left = (held & KEY_Y) != 0;
    in->right = (held & KEY_A) != 0;
    in->cx = cs.dx;
    in->cy = cs.dy;
}

int main(void)
{
    /* Standing on the middle floor block (top at y = 1), eye 1.6 above it,
     * facing north and looking down a little. */
    Camera cam = { 0.5f, 2.6f, 0.5f, 0.0f, -15.0f * PI / 180.0f };
    MeshVertex *scene = NULL;
    Mat4 proj;
    u64 last;
    int verts = 0;

    gfxInitDefault();
    if (gpu_init()) {
        scene = linearAlloc(SCENE_VERTS * sizeof *scene);
        if (scene)
            verts = build_scene(scene);
    }
    if (verts != SCENE_VERTS) {
        if (scene)
            linearFree(scene);
        gpu_exit();
        gfxExit();
        return 1;
    }
    GSPGPU_FlushDataCache(scene, SCENE_VERTS * sizeof *scene);
    mat4_persp_tilt(&proj, 60.0f * PI / 180.0f, 400.0f / 240.0f, 0.1f, 100.0f);

    last = osGetTime();
    while (aptMainLoop()) {
        LookInput in;
        Mat4 view, mvp;
        float yaw_rate, pitch_rate, dt;
        u64 now;

        hidScanInput();
        if (hidKeysDown() & KEY_START)
            break;

        /* Turn by time passed, so the speed is the same at any frame rate.
         * A long pause (the Home Menu) counts as at most a tenth of a second. */
        now = osGetTime();
        dt = (float)(now - last) / 1000.0f;
        last = now;
        if (dt > 0.1f)
            dt = 0.1f;
        read_look(&in);
        look_rates(&in, &yaw_rate, &pitch_rate);
        camera_turn(&cam, yaw_rate * dt, pitch_rate * dt);

        fill_screen(GFX_BOTTOM, 80, 150, 60);  /* grass green */
        gfxFlushBuffers();

        camera_view(&view, &cam);
        mat4_mul(&mvp, &proj, &view);
        gpu_frame_begin();
        gpu_draw(scene, verts, &mvp);
        gpu_frame_end();

        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    linearFree(scene);
    gpu_exit();
    gfxExit();
    return 0;
}
