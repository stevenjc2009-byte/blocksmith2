#include <3ds.h>

#include "block.h"
#include "camera.h"
#include "chunk.h"
#include "chunk_mesh.h"
#include "framebuffer.h"
#include "gpu.h"
#include "look.h"
#include "mat4.h"
#include "mesh.h"

/* Roadmap row 1-05: one chunk. A 16 x 128 x 16 column: stone up to y 59,
 * three layers of dirt, grass on top (y 63), with a 4 x 4 pit three deep in
 * the middle of the top. Only faces next to air are drawn. The camera hangs
 * south of the chunk looking north and down at it; A B X Y and the C-stick
 * turn it. The bottom screen stays a solid colour. START quits. */

#define PI 3.14159265f

static Chunk s_chunk;

static void fill_screen(gfxScreen_t screen, u8 r, u8 g, u8 b)
{
    u16 width = 0, height = 0;
    u8 *fb = gfxGetFramebuffer(screen, GFX_LEFT, &width, &height);
    fb_fill_bgr8(fb, width * height, r, g, b);
}

static void build_chunk(Chunk *c)
{
    int x, y, z;

    chunk_clear(c);
    for (y = 0; y < 64; y++)
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++)
                chunk_set(c, x, y, z, y < 60 ? BLOCK_STONE : y < 63 ? BLOCK_DIRT : BLOCK_GRASS);
    for (y = 61; y < 64; y++) /* the pit */
        for (z = 6; z < 10; z++)
            for (x = 6; x < 10; x++)
                chunk_set(c, x, y, z, BLOCK_AIR);
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
    /* 18 blocks south of the chunk's south wall (z = 16), 8 above its top
     * (y = 64), level with its middle, facing north, looking down 30 degrees. */
    Camera cam = { 8.0f, 72.0f, 34.0f, 0.0f, -30.0f * PI / 180.0f };
    MeshVertex *mesh = NULL;
    Mat4 proj;
    u64 last;
    int faces, verts = -1;

    gfxInitDefault();
    build_chunk(&s_chunk);
    faces = chunk_mesh_count(&s_chunk, NULL);
    if (gpu_init()) {
        mesh = linearAlloc(faces * MESH_FACE_VERTS * sizeof *mesh);
        if (mesh)
            verts = chunk_mesh_build(&s_chunk, NULL, 0.0f, 0.0f, mesh, faces * MESH_FACE_VERTS);
    }
    if (verts <= 0) {
        if (mesh)
            linearFree(mesh);
        gpu_exit();
        gfxExit();
        return 1;
    }
    GSPGPU_FlushDataCache(mesh, verts * sizeof *mesh);
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
        gpu_draw(mesh, verts, &mvp);
        gpu_frame_end();

        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    linearFree(mesh);
    gpu_exit();
    gfxExit();
    return 0;
}
