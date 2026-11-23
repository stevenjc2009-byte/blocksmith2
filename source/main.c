#include <3ds.h>
#include <math.h>

#include "block.h"
#include "blocktex.h"
#include "camera.h"
#include "chunk.h"
#include "controls.h"
#include "fps.h"
#include "frame.h"
#include "framebuffer.h"
#include "gpu.h"
#include "hotbar.h"
#include "lidsave.h"
#include "look.h"
#include "mat4.h"
#include "meshes.h"
#include "place.h"
#include "player.h"
#include "raycast.h"
#include "save.h"
#include "saveui.h"
#include "settings.h"
#include "sky.h"
#include "skydraw.h"
#include "start.h"
#include "walk.h"
#include "world.h"

/* Roadmap row 3-02: the endless world, a settings screen, a seed for each
 * new world, saving, a list of the saved worlds, and day and night. With
 * worlds saved the
 * game starts on the world list (worldlist.h), over the sky blue top
 * screen, the world played last chosen: A (or OPEN) opens the chosen world
 * (its changed chunks, where the player stood and looked, the hotbar), Y
 * (or RENAME) gives it a new name, L (or DELETE) deletes it after asking,
 * and X (or NEW) goes to the new-world screen (newworld.h), which is where
 * the game starts with no worlds saved. It shows the new world's seed,
 * random at first: X (or its button) rolls a new random seed, Y (or its
 * button) opens the 3DS keyboard to type one (a whole number is that seed,
 * words become one, seed.h), A (or PLAY) makes a new world from that seed,
 * in the next free folder on the SD card (save.h), and B (or BACK) goes
 * back to the list. The view distance and the world played last are kept
 * too. A new player starts in chunk (0, 0) of the seed's gentle
 * hills, facing north. The ground is made as the player walks (stream.h):
 * the chunks out to the view distance round the player's chunk are drawn
 * (3 at start: 7 x 7), one mesh each, and one more ring round them is kept;
 * one chunk or mesh is made each frame, nearest first, and chunks further
 * away are put away, changed ones saved to the SD card as they go (and
 * loaded from it when they come back). SELECT opens and closes the settings
 * screen on the bottom screen (settings.h): a slider for the view distance,
 * 2 to 5, moved by touch or the D-pad left and right; while it is open the
 * player stands still and R, L, SAVE and the hotbar do nothing, but the
 * world goes on being made and drawn, so a new view distance shows at once.
 * The Circle Pad walks, by where the camera faces; gravity holds the player
 * on the ground, blocks stop them, and walking into a one-block step hops up
 * it (no jump button). A B X Y look around, and so does the C-stick on a
 * New 3DS (an Old 3DS has none). A crosshair marks the middle of the top
 * screen; R breaks the block under it, up to 5 blocks away, at once (timed
 * mining comes with tools, kickoff P-4), and L puts the chosen block against
 * the face looked at, never inside the player. The meshes of the chunks a
 * change touches are rebuilt. The bottom screen is grass green with nine
 * block slots along its foot (grass, dirt, stone, then empty); tapping a
 * slot chooses it. SAVE in its top-right corner saves the world (saveui.h),
 * and so does an autosave every 3 minutes, closing the lid and HOME
 * (lidsave.h); a message says how it went, after the lid or HOME once the
 * game is back. The
 * game shows a new picture every second screen refresh, 30 a second, and a
 * counter in the bottom screen's top-left corner shows the frames a second
 * (its settings switch comes in roadmap 0.7). START saves and quits; on the
 * start screens it just quits. A day and night takes 20 minutes (sky.h):
 * the sun rises in the east and sets in the west, the sky glows orange
 * round both, the moon crosses the night sky and the world grows dim; the
 * time of day is saved with the world. The chunk meshes are in meshes.c,
 * the start screens' loop in start.c, frame pacing and the crosshair in
 * frame.c, the Circle Pad and look keys read in controls.c, the sky drawn
 * in skydraw.c. */

#define PI 3.14159265f

static World s_world;
static uint32_t s_atlas[BLOCKTEX_SIZE * BLOCKTEX_SIZE]; /* the hotbar's icons */
static Settings s_settings;
static SaveWorld s_save;
static bool s_saving;       /* s_save is open: the world has a folder */
static SaveSettings s_kept; /* settings.dat, as read or last written */
static SaveUiMessage s_msg; /* the save message, shown until s_msg_until */
static u64 s_msg_until;
static LidSave s_lid; /* the lid closing or HOME saves the game */
static float s_daytime = SKY_NEW_WORLD; /* the time of day (sky.h) */

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
    return meshes_rebuild_around(&s_world, hit.x, hit.z);
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
    return meshes_rebuild_around(&s_world, x, z);
}

/* The bottom screen: the settings screen while it is open, else grass green
 * with the hotbar along its foot, the SAVE button and the save message
 * while it lasts; the frame counter in the top-left corner either way. Its
 * picture stands on its side like the top screen's (gfxGetFramebuffer()
 * calls it 240 wide and 320 tall), so the sizes are passed swapped. */
static void draw_bottom(const Hotbar *hb, int fps_shown)
{
    u16 fw = 0, fh = 0;
    u8 *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &fw, &fh);

    if (s_settings.open) {
        settings_draw_bgr8(fb, fh, fw, &s_settings);
    } else {
        fb_fill_bgr8(fb, fw * fh, 80, 150, 60);
        hotbar_draw_bgr8(fb, fh, fw, hb, s_atlas);
        saveui_draw_bgr8(fb, fh, fw, osGetTime() < s_msg_until ? s_msg : SAVEUI_NONE);
    }
    fps_draw_bgr8(fb, fh, fw, fps_shown);
}

/* Save the game: the world's changed chunks, then world.dat (seed, where
 * the player stands and looks, the hotbar, the time of day), then
 * settings.dat. True only if all of it was written; false too when the
 * world has no folder (the settings are still written). */
static bool save_game(const Player *p, const Camera *cam, const Hotbar *hb)
{
    SaveInfo info;
    bool ok = false;
    int k;

    if (s_saving) {
        ok = world_save_changed(&s_world) == 0;
        info.seed = s_world.seed;
        info.x = p->x;
        info.y = p->y;
        info.z = p->z;
        info.yaw = cam->yaw;
        info.pitch = cam->pitch;
        for (k = 0; k < HOTBAR_SLOTS; k++)
            info.hotbar[k] = (uint8_t)hb->block[k];
        info.chosen = (uint8_t)hb->chosen;
        info.daytime = s_daytime;
        ok = save_world_write(&s_save, &info) && ok;
    }
    s_kept.view = s_settings.view;
    return save_settings_write(SAVE_ROOT, &s_kept) && ok;
}

/* Show how a save went, for SAVEUI_MSG_MS. */
static void show_saved(bool ok)
{
    s_msg = ok ? SAVEUI_SAVED : SAVEUI_FAILED;
    s_msg_until = osGetTime() + SAVEUI_MSG_MS;
}

/* The game as it stands, for lidsave.h to save when the lid closes or HOME
 * is pressed (libctru calls it from aptMainLoop(), between frames). */
typedef struct {
    const Player *p;
    const Camera *cam;
    const Hotbar *hb;
} Live;

static bool save_live(void *ctx)
{
    const Live *l = ctx;

    return save_game(l->p, l->cam, l->hb);
}

int main(void)
{
    /* Facing north, looking a little down; the eye follows the player. */
    Camera cam = { 0.0f, 0.0f, 0.0f, 0.0f, -10.0f * PI / 180.0f };
    Player player;
    Hotbar hotbar;
    Live live = { &player, &cam, &hotbar };
    FpsCounter fps;
    Mat4 proj;
    SaveInfo info;
    u64 last, paced, saved_at;
    uint32_t seed;
    u32 stale;
    StartChoice start;
    bool cont = false, ok;
    int k, world = 0, pcx = 0, pcz = 0;

    gfxInitDefault();
    blocktex_build(s_atlas);
    hotbar_init(&hotbar);
    settings_init(&s_settings);
    /* The kept settings and the world played last, from the SD card. */
    s_kept.view = s_settings.view;
    s_kept.last = 0;
    save_root_make(SAVE_ROOT);
    if (save_settings_read(SAVE_ROOT, &s_kept))
        s_settings.view = s_kept.view;
    if (!gpu_init()) {
        gpu_exit();
        gfxExit();
        return 1;
    }
    paced = svcGetSystemTick();
    fps_init(&fps, paced);
    start = start_choose_world(&seed, &s_kept, &world, &fps, &paced);
    if (start == START_QUIT) {
        gpu_exit();
        gfxExit();
        return 0;
    }
    if (start == START_OPEN) {
        cont = save_world_open(&s_save, SAVE_ROOT, world, false) &&
               save_world_read(&s_save, &info);
        if (cont)
            seed = info.seed;
        else
            save_world_close(&s_save); /* a new world from the seed shown, then */
    }
    if (cont) {
        s_saving = true;
        s_kept.last = world;
    } else {
        int id = save_free_id(SAVE_ROOT);

        s_saving = id > 0 && save_world_open(&s_save, SAVE_ROOT, id, true);
        if (s_saving)
            s_kept.last = id;
    }
    world_init(&s_world, seed);
    if (s_saving)
        world_set_store(&s_world, &s_save.store);
    if (cont) {
        pcx = block_to_chunk((int)floorf(info.x));
        pcz = block_to_chunk((int)floorf(info.z));
    }
    if (!meshes_stream(&s_world, pcx, pcz, s_settings.view, MESHES_ALL)) {
        meshes_free();
        save_world_close(&s_save);
        gpu_exit();
        gfxExit();
        return 1;
    }
    if (cont) {
        player.x = info.x;
        player.y = info.y;
        player.z = info.z;
        player.vy = 0.0f;
        player.on_ground = false;
        cam.yaw = info.yaw;
        cam.pitch = info.pitch;
        camera_turn(&cam, 0.0f, 0.0f); /* keeps them in range */
        for (k = 0; k < HOTBAR_SLOTS; k++)
            hotbar.block[k] = info.hotbar[k];
        hotbar.chosen = info.chosen;
        s_daytime = info.daytime;
    } else {
        player_spawn(&player, &s_world, 0, 0);
        save_game(&player, &cam, &hotbar); /* so the new world can be carried on at once */
    }
    mat4_persp_tilt(&proj, 60.0f * PI / 180.0f, 400.0f / 240.0f, 0.1f, 160.0f);

    /* Making the world took a while: time, frame pacing and the frame count
     * start again from now. A (PLAY or OPEN) also turns right, so keys
     * still held from the start screens do nothing until they are let go. */
    last = osGetTime();
    saved_at = last;
    paced = svcGetSystemTick();
    fps_init(&fps, paced);
    hidScanInput();
    stale = hidKeysHeld();
    lidsave_init(&s_lid, save_live, &live);
    lidsave_hook_start(&s_lid);
    while (aptMainLoop()) {
        LookInput in;
        Mat4 view, mvp;
        float yaw_rate, pitch_rate, vx, vz, dt;
        u64 now;

        hidScanInput();
        stale &= hidKeysHeld(); /* a key let go is no longer left out */
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
        s_daytime = sky_advance(s_daytime, dt);
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
            read_look(&in, stale);
            look_rates(&in, &yaw_rate, &pitch_rate);
            camera_turn(&cam, yaw_rate * dt, pitch_rate * dt);
            read_walk(&cam, &vx, &vz);
            player_step(&player, &s_world, vx, vz, dt);
        }
        cam.x = player.x;
        cam.y = player.y + PLAYER_EYE;
        cam.z = player.z;
        /* One new chunk or mesh a frame round where the player now is. */
        if (!meshes_stream(&s_world, block_to_chunk((int)floorf(player.x)),
                           block_to_chunk((int)floorf(player.z)), s_settings.view, 1))
            break; /* out of memory for a mesh */
        if (!s_settings.open) {
            if ((hidKeysDown() & KEY_R) && !break_looked_at(&cam))
                break; /* out of memory for a mesh */
            if (hidKeysDown() & KEY_TOUCH) {
                touchPosition touch;

                hidTouchRead(&touch);
                if (saveui_hit(touch.px, touch.py)) {
                    show_saved(save_game(&player, &cam, &hotbar));
                    saved_at = osGetTime();
                } else {
                    hotbar_tap(&hotbar, touch.px, touch.py);
                }
            }
            if ((hidKeysDown() & KEY_L) &&
                !place_looked_at(&cam, &player, hotbar_block(&hotbar)))
                break;
        }
        if (save_due(osGetTime(), saved_at)) {
            show_saved(save_game(&player, &cam, &hotbar));
            saved_at = osGetTime();
        }
        if (lidsave_back(&s_lid, &ok)) { /* back from the lid closed or HOME */
            show_saved(ok);
            saved_at = osGetTime();
        }
        if (s_world.lost) { /* a changed chunk put away could not be saved */
            s_world.lost = 0;
            show_saved(false);
        }

        draw_bottom(&hotbar, fps.shown);
        gfxFlushBuffers();

        camera_view(&view, &cam);
        mat4_mul(&mvp, &proj, &view);
        gpu_frame_begin();
        skydraw(s_daytime, &cam, &proj);
        meshes_draw(&mvp);
        gpu_frame_end();
        frame_crosshair();
        frame_end(&fps, &paced);
    }

    lidsave_hook_stop();
    save_game(&player, &cam, &hotbar); /* START, or the system closing the game */
    save_world_close(&s_save);
    meshes_free();
    skydraw_exit();
    gpu_exit();
    gfxExit();
    return 0;
}
