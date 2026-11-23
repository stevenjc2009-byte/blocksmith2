#include <3ds.h>
#include <math.h>

#include "block.h"
#include "blocktex.h"
#include "camera.h"
#include "chunk.h"
#include "chunk_mesh.h"
#include "fps.h"
#include "framebuffer.h"
#include "gpu.h"
#include "hotbar.h"
#include "look.h"
#include "mat4.h"
#include "mesh.h"
#include "place.h"
#include "player.h"
#include "raycast.h"
#include "settings.h"
#include "stream.h"
#include "walk.h"
#include "world.h"

/* Roadmap row 2-02: the endless world with a settings screen. The player
 * starts in chunk (0, 0) of gentle hills from a fixed seed, facing north.
 * The ground is made as the player walks (stream.h): the chunks out to the
 * view distance round the player's chunk are drawn (3 at start: 7 x 7), one
 * mesh each, and one more ring round them is kept; one chunk or mesh is made
 * each frame, nearest first, and chunks further away are put away (changes
 * to them are lost until saving, roadmap row 2-04). SELECT opens and closes
 * the settings screen on the bottom screen (settings.h): a slider for the
 * view distance, 2 to 5, moved by touch or the D-pad left and right; while
 * it is open the player stands still and R, L and the hotbar do nothing, but
 * the world goes on being made and drawn, so a new view distance shows at
 * once. The Circle
 * Pad walks, by where the camera faces; gravity holds the player on the
 * ground, blocks stop them, and walking into a one-block step hops up it (no
 * jump button). A B X Y and the C-stick look around. A crosshair marks the
 * middle of the top screen; R breaks the block under it, up to 5 blocks
 * away, at once (timed mining comes with tools, kickoff P-4), and L puts the
 * chosen block against the face looked at, never inside the player. The
 * meshes of the chunks a change touches are rebuilt. The bottom screen is
 * grass green with nine block slots along its foot (grass, dirt, stone,
 * then empty); tapping a slot chooses it. The game shows a new picture
 * every second screen refresh, 30 a second, and a counter in the bottom
 * screen's top-left corner shows the frames a second (its settings switch
 * comes in roadmap 0.7). START quits. */

#define PI 3.14159265f
#define WORLD_SEED 2026u /* typed and random seeds come later (kickoff W-10) */

static World s_world;
/* One mesh per world slot, for the chunk in that slot (world.h);
 * s_meshed[j][i] says slot [j][i]'s chunk has its mesh built, which can be
 * empty (s_mesh NULL). */
static MeshVertex *s_mesh[WORLD_SLOTS][WORLD_SLOTS];
static int s_verts[WORLD_SLOTS][WORLD_SLOTS];
static bool s_meshed[WORLD_SLOTS][WORLD_SLOTS];
static uint32_t s_atlas[BLOCKTEX_SIZE * BLOCKTEX_SIZE]; /* the hotbar's icons */
static Settings s_settings;

static void free_mesh(int i, int j)
{
    if (s_mesh[j][i]) {
        linearFree(s_mesh[j][i]);
        s_mesh[j][i] = NULL;
    }
    s_verts[j][i] = 0;
    s_meshed[j][i] = false;
}

static void free_meshes(void)
{
    int i, j;

    for (j = 0; j < WORLD_SLOTS; j++)
        for (i = 0; i < WORLD_SLOTS; i++)
            free_mesh(i, j);
}

/* (Re)build the mesh of loaded chunk (cx, cz) in linear memory for the GPU.
 * Only between frames: gpu_frame_end() has waited for the GPU, so the old
 * mesh is no longer being read. False if out of memory. */
static bool build_mesh(int cx, int cz)
{
    int i = world_slot(cx), j = world_slot(cz);
    const Chunk *c = world_chunk(&s_world, cx, cz);
    const Chunk *nb[NB_COUNT];
    int faces;

    free_mesh(i, j);
    if (c == NULL)
        return true;
    world_neighbours(&s_world, cx, cz, nb);
    faces = chunk_mesh_count(c, nb);
    s_meshed[j][i] = true;
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

#define STREAM_ALL (-1)

/* Keep the world round the player's chunk (pcx, pcz) out to view distance
 * view (stream.h): put away the far chunks and the meshes that should go,
 * then run up to jobs jobs (STREAM_ALL: until none are left). Between frames
 * only, like build_mesh(). False if out of memory for a mesh. */
static bool stream_frame(int pcx, int pcz, int view, int jobs)
{
    StreamJob job;
    int i, j;

    stream_put_away(&s_world, pcx, pcz, view);
    for (j = 0; j < WORLD_SLOTS; j++)
        for (i = 0; i < WORLD_SLOTS; i++)
            if (s_meshed[j][i] && !stream_keep_mesh(&s_world, i, j, pcx, pcz, view))
                free_mesh(i, j);
    while ((jobs == STREAM_ALL || jobs-- > 0) &&
           stream_next(&s_world, s_meshed, pcx, pcz, view, &job)) {
        if (job.kind == STREAM_LOAD) {
            free_mesh(world_slot(job.cx), world_slot(job.cz));
            world_load(&s_world, job.cx, job.cz);
        } else if (!build_mesh(job.cx, job.cz)) {
            return false;
        }
    }
    return true;
}

/* Rebuild the built meshes a change to block column (x, z) touches (a chunk
 * still waiting for its mesh gets the change when it is built). False if
 * out of memory. */
static bool rebuild_around(int x, int z)
{
    int touched[3][2];
    int n, k;

    n = world_touched_chunks(&s_world, x, z, touched);
    for (k = 0; k < n; k++)
        if (s_meshed[world_slot(touched[k][1])][world_slot(touched[k][0])] &&
            !build_mesh(touched[k][0], touched[k][1]))
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

/* The bottom screen: the settings screen while it is open, else grass green
 * with the hotbar along its foot; the frame counter in the top-left corner
 * either way. Its picture stands on its side like the top screen's
 * (gfxGetFramebuffer() calls it 240 wide and 320 tall), so the sizes are
 * passed swapped. */
static void draw_bottom(const Hotbar *hb, int fps_shown)
{
    u16 fw = 0, fh = 0;
    u8 *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &fw, &fh);

    if (s_settings.open) {
        settings_draw_bgr8(fb, fh, fw, &s_settings);
    } else {
        fb_fill_bgr8(fb, fw * fh, 80, 150, 60);
        hotbar_draw_bgr8(fb, fh, fw, hb, s_atlas);
    }
    fps_draw_bgr8(fb, fh, fw, fps_shown);
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
    Hotbar hotbar;
    FpsCounter fps;
    Mat4 proj;
    u64 last, paced;

    gfxInitDefault();
    world_init(&s_world, WORLD_SEED);
    blocktex_build(s_atlas);
    hotbar_init(&hotbar);
    settings_init(&s_settings);
    if (!gpu_init() || !stream_frame(0, 0, s_settings.view, STREAM_ALL)) {
        free_meshes();
        gpu_exit();
        gfxExit();
        return 1;
    }
    player_spawn(&player, &s_world, 0, 0);
    mat4_persp_tilt(&proj, 60.0f * PI / 180.0f, 400.0f / 240.0f, 0.1f, 160.0f);

    last = osGetTime();
    paced = svcGetSystemTick();
    fps_init(&fps, paced);
    while (aptMainLoop()) {
        LookInput in;
        Mat4 view, mvp;
        float yaw_rate, pitch_rate, vx, vz, dt;
        u64 now;
        int i, j;

        hidScanInput();
        if (hidKeysDown() & KEY_START)
            break;
        if (hidKeysDown() & KEY_SELECT)
            s_settings.open = !s_settings.open;

        /* Turn and walk by time passed, so the speed is the same at any
         * frame rate. A long pause (the Home Menu) counts as at most a tenth
         * of a second. */
        now = osGetTime();
        dt = (float)(now - last) / 1000.0f;
        last = now;
        if (dt > 0.1f)
            dt = 0.1f;
        if (s_settings.open) {
            /* The settings screen: the player stands still; a touch or the
             * D-pad moves the slider. */
            if (hidKeysHeld() & KEY_TOUCH) {
                touchPosition touch;

                hidTouchRead(&touch);
                settings_touch(&s_settings, touch.px, touch.py);
            }
            if (hidKeysDown() & KEY_DLEFT)
                settings_step(&s_settings, -1);
            if (hidKeysDown() & KEY_DRIGHT)
                settings_step(&s_settings, 1);
        } else {
            read_look(&in);
            look_rates(&in, &yaw_rate, &pitch_rate);
            camera_turn(&cam, yaw_rate * dt, pitch_rate * dt);
            read_walk(&cam, &vx, &vz);
            player_step(&player, &s_world, vx, vz, dt);
        }
        cam.x = player.x;
        cam.y = player.y + PLAYER_EYE;
        cam.z = player.z;
        /* One new chunk or mesh a frame round where the player now is. */
        if (!stream_frame(block_to_chunk((int)floorf(player.x)),
                          block_to_chunk((int)floorf(player.z)), s_settings.view, 1))
            break; /* out of memory for a mesh */
        if (!s_settings.open) {
            if ((hidKeysDown() & KEY_R) && !break_looked_at(&cam))
                break; /* out of memory for a mesh */
            if (hidKeysDown() & KEY_TOUCH) {
                touchPosition touch;

                hidTouchRead(&touch);
                hotbar_tap(&hotbar, touch.px, touch.py);
            }
            if ((hidKeysDown() & KEY_L) &&
                !place_looked_at(&cam, &player, hotbar_block(&hotbar)))
                break;
        }

        draw_bottom(&hotbar, fps.shown);
        gfxFlushBuffers();

        camera_view(&view, &cam);
        mat4_mul(&mvp, &proj, &view);
        gpu_frame_begin();
        for (j = 0; j < WORLD_SLOTS; j++)
            for (i = 0; i < WORLD_SLOTS; i++)
                gpu_draw(s_mesh[j][i], s_verts[j][i], &mvp);
        gpu_frame_end();
        draw_crosshair();

        /* Hand the picture over, then wait for screen refreshes until the
         * second one since the last frame's wait: 30 frames a second. */
        gfxSwapBuffers();
        do
            gspWaitForVBlank();
        while (!fps_pace_done(paced, svcGetSystemTick()));
        paced = svcGetSystemTick();
        fps_frame(&fps, paced);
    }

    free_meshes();
    gpu_exit();
    gfxExit();
    return 0;
}
