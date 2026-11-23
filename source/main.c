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
#include "place.h"
#include "player.h"
#include "raycast.h"
#include "walk.h"
#include "world.h"

/* Roadmap row 1-09: placing blocks. The player stands in the middle of the
 * small world (4 x 4 chunks of gentle hills from a fixed seed, one mesh per
 * chunk) facing north. The Circle Pad walks, by where the camera faces;
 * gravity holds the player on the ground, blocks stop them, and walking into
 * a one-block step hops up it (no jump button). The world's sides are an
 * invisible wall. A B X Y and the C-stick look around. A crosshair marks the
 * middle of the top screen; R breaks the block under it, up to 5 blocks
 * away, at once (timed mining comes with tools, kickoff P-4), and L puts a
 * stone block against the face looked at, never inside the player. The
 * meshes of the chunks a change touches are rebuilt. The bottom screen
 * stays a solid colour. START quits. */

#define PI 3.14159265f
#define WORLD_SEED 2026u /* typed and random seeds come later (kickoff W-10) */
#define HAND_BLOCK BLOCK_STONE /* until the hotbar (roadmap row 1-10) */

static World s_world;
static MeshVertex *s_mesh[WORLD_CHUNKS][WORLD_CHUNKS];
static int s_verts[WORLD_CHUNKS][WORLD_CHUNKS];

static void fill_screen(gfxScreen_t screen, u8 r, u8 g, u8 b)
{
    u16 width = 0, height = 0;
    u8 *fb = gfxGetFramebuffer(screen, GFX_LEFT, &width, &height);
    fb_fill_bgr8(fb, width * height, r, g, b);
}

static void free_mesh(int i, int j)
{
    if (s_mesh[j][i]) {
        linearFree(s_mesh[j][i]);
        s_mesh[j][i] = NULL;
    }
    s_verts[j][i] = 0;
}

static void free_meshes(void)
{
    int i, j;

    for (j = 0; j < WORLD_CHUNKS; j++)
        for (i = 0; i < WORLD_CHUNKS; i++)
            free_mesh(i, j);
}

/* (Re)build the mesh of chunk [j][i] in linear memory for the GPU. Only
 * between frames: gpu_frame_end() has waited for the GPU, so the old mesh is
 * no longer being read. False if out of memory. */
static bool build_mesh(int i, int j)
{
    int cx = WORLD_CHUNK_MIN + i, cz = WORLD_CHUNK_MIN + j;
    const Chunk *c = world_chunk(&s_world, cx, cz);
    const Chunk *nb[NB_COUNT];
    int faces;

    free_mesh(i, j);
    world_neighbours(&s_world, cx, cz, nb);
    faces = chunk_mesh_count(c, nb);
    if (faces == 0)
        return true;
    s_mesh[j][i] = linearAlloc(faces * MESH_FACE_VERTS * sizeof(MeshVertex));
    if (!s_mesh[j][i])
        return false;
    s_verts[j][i] = chunk_mesh_build(c, nb, (float)(cx * CHUNK_SIZE), (float)(cz * CHUNK_SIZE),
                                     s_mesh[j][i], faces * MESH_FACE_VERTS);
    if (s_verts[j][i] <= 0) {
        free_mesh(i, j);
        return false;
    }
    GSPGPU_FlushDataCache(s_mesh[j][i], s_verts[j][i] * sizeof(MeshVertex));
    return true;
}

/* One mesh per chunk. False if out of memory. */
static bool build_meshes(void)
{
    int i, j;

    for (j = 0; j < WORLD_CHUNKS; j++)
        for (i = 0; i < WORLD_CHUNKS; i++)
            if (!build_mesh(i, j))
                return false;
    return true;
}

/* Rebuild the meshes a change to block column (x, z) touches. False if out
 * of memory. */
static bool rebuild_around(int x, int z)
{
    int touched[3][2];
    int n, k;

    n = world_touched_chunks(x, z, touched);
    for (k = 0; k < n; k++)
        if (!build_mesh(touched[k][0] - WORLD_CHUNK_MIN, touched[k][1] - WORLD_CHUNK_MIN))
            return false;
    return true;
}

/* The block under the crosshair, if one is in reach. */
static bool looked_at(const Camera *cam, RayHit *hit)
{
    float f[3];

    camera_forward(cam, f);
    return raycast(&s_world, cam->x, cam->y, cam->z, f[0], f[1], f[2], RAY_REACH, hit);
}

/* R: break the block under the crosshair and rebuild the chunks it changes.
 * False only if a rebuild ran out of memory. */
static bool break_looked_at(const Camera *cam)
{
    RayHit hit;

    if (!looked_at(cam, &hit) || !world_break(&s_world, hit.x, hit.y, hit.z))
        return true;
    return rebuild_around(hit.x, hit.z);
}

/* L: put block b against the face under the crosshair (not inside the
 * player) and rebuild the chunks round the new block, which can be in the
 * next chunk over from the block looked at. False only if a rebuild ran out
 * of memory. */
static bool place_looked_at(const Camera *cam, const Player *player, int b)
{
    RayHit hit;
    int x, y, z;

    if (!looked_at(cam, &hit) || !place_block(&s_world, player, &hit, b, &x, &y, &z))
        return true;
    return rebuild_around(x, z);
}

/* The crosshair, drawn by the CPU straight into the top screen's picture
 * after the GPU has copied the frame there. The 3DS keeps that picture
 * standing on its side: gfxGetFramebuffer() calls the top screen 240 wide
 * and 400 tall, so they are passed swapped. Its cache lines are thrown away
 * first (the GPU wrote the memory behind the CPU's back, and an old line
 * written back would put last frame's pixels over this one), then written
 * out. */
static void draw_crosshair(void)
{
    u16 fw = 0, fh = 0;
    u8 *fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &fw, &fh);
    u32 size = (u32)fw * fh * 3;

    GSPGPU_InvalidateDataCache(fb, size);
    fb_crosshair_bgr8(fb, fh, fw);
    GSPGPU_FlushDataCache(fb, size);
}

/* Walking speed from the Circle Pad, by where the camera faces. */
static void read_walk(const Camera *cam, float *vx, float *vz)
{
    circlePosition pad = { 0, 0 };

    hidCircleRead(&pad);
    walk_velocity(pad.dx, pad.dy, cam->yaw, vx, vz);
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
    /* Facing north, looking a little down; the eye follows the player. */
    Camera cam = { 0.0f, 0.0f, 0.0f, 0.0f, -10.0f * PI / 180.0f };
    Player player;
    Mat4 proj;
    u64 last;

    gfxInitDefault();
    world_generate(&s_world, WORLD_SEED);
    player_spawn(&player, &s_world, 0, 0);
    if (!gpu_init() || !build_meshes()) {
        free_meshes();
        gpu_exit();
        gfxExit();
        return 1;
    }
    mat4_persp_tilt(&proj, 60.0f * PI / 180.0f, 400.0f / 240.0f, 0.1f, 160.0f);

    last = osGetTime();
    while (aptMainLoop()) {
        LookInput in;
        Mat4 view, mvp;
        float yaw_rate, pitch_rate, vx, vz, dt;
        u64 now;
        int i, j;

        hidScanInput();
        if (hidKeysDown() & KEY_START)
            break;

        /* Turn and walk by time passed, so the speed is the same at any
         * frame rate. A long pause (the Home Menu) counts as at most a tenth
         * of a second. */
        now = osGetTime();
        dt = (float)(now - last) / 1000.0f;
        last = now;
        if (dt > 0.1f)
            dt = 0.1f;
        read_look(&in);
        look_rates(&in, &yaw_rate, &pitch_rate);
        camera_turn(&cam, yaw_rate * dt, pitch_rate * dt);
        read_walk(&cam, &vx, &vz);
        player_step(&player, &s_world, vx, vz, dt);
        cam.x = player.x;
        cam.y = player.y + PLAYER_EYE;
        cam.z = player.z;
        if ((hidKeysDown() & KEY_R) && !break_looked_at(&cam))
            break; /* out of memory for a mesh */
        if ((hidKeysDown() & KEY_L) && !place_looked_at(&cam, &player, HAND_BLOCK))
            break;

        fill_screen(GFX_BOTTOM, 80, 150, 60);  /* grass green */
        gfxFlushBuffers();

        camera_view(&view, &cam);
        mat4_mul(&mvp, &proj, &view);
        gpu_frame_begin();
        for (j = 0; j < WORLD_CHUNKS; j++)
            for (i = 0; i < WORLD_CHUNKS; i++)
                gpu_draw(s_mesh[j][i], s_verts[j][i], &mvp);
        gpu_frame_end();
        draw_crosshair();

        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    free_meshes();
    gpu_exit();
    gfxExit();
    return 0;
}
