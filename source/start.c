#include <3ds.h>
#include <string.h>

#include "frame.h"
#include "gpu.h"
#include "newworld.h"
#include "save.h"
#include "seed.h"
#include "start.h"
#include "worldlist.h"
#include "worldname.h"

/* The start screens' loop (start.h): the world list, its delete screen and
 * the new-world screen. */

/* Holding the D-pad on the world list moves on after REPEAT_DELAY scans,
 * then every REPEAT_EVERY (30 scans a second). */
#define REPEAT_DELAY 15
#define REPEAT_EVERY 4

typedef enum {
    SCREEN_LIST, /* the world list */
    SCREEN_ASK,  /* delete the chosen world? */
    SCREEN_NEW   /* the new-world screen */
} Screen;

static int s_ids[SAVE_WORLDS_MAX]; /* the saved worlds' numbers, the list's order */

/* Y on the new-world screen: the 3DS keyboard, to type a seed. Only OK
 * with something typed changes *seed; Cancel leaves it. The keys pressed to
 * close the keyboard can still be held when it returns, so one scan takes
 * them in and they do not press anything on the new-world screen. */
static void type_seed(uint32_t *seed)
{
    SwkbdState kb;
    char text[SEED_TYPED_MAX * 4 + 1]; /* UTF-8: up to 4 bytes a character */

    swkbdInit(&kb, SWKBD_TYPE_NORMAL, 2, SEED_TYPED_MAX);
    swkbdSetHintText(&kb, "A number, or any words");
    swkbdSetValidation(&kb, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    text[0] = '\0';
    if (swkbdInputText(&kb, text, sizeof text) == SWKBD_BUTTON_CONFIRM)
        seed_from_text(text, seed);
    hidScanInput();
}

/* Y on the world list: the 3DS keyboard with the world's name in it. OK
 * with a name left after worldname_clean() gives world id that name;
 * Cancel leaves it. One scan afterwards, as type_seed() does. */
static void type_name(int id)
{
    SwkbdState kb;
    char name[WORLDNAME_SIZE], clean[WORLDNAME_SIZE];
    char text[WORLDNAME_MAX * 4 + 1];

    save_name_read(SAVE_ROOT, id, name);
    swkbdInit(&kb, SWKBD_TYPE_NORMAL, 2, WORLDNAME_MAX);
    swkbdSetHintText(&kb, "The world's name");
    swkbdSetInitialText(&kb, name);
    swkbdSetValidation(&kb, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    text[0] = '\0';
    if (swkbdInputText(&kb, text, sizeof text) == SWKBD_BUTTON_CONFIRM &&
        worldname_clean(text, clean))
        save_name_write(SAVE_ROOT, id, clean);
    hidScanInput();
}

/* The names in the list's rows, from the SD card. */
static void read_names(const WorldList *l, char names[WORLDLIST_ROWS][WORLDNAME_SIZE])
{
    int i;

    for (i = 0; i < WORLDLIST_ROWS && l->top + i < l->count; i++)
        save_name_read(SAVE_ROOT, s_ids[l->top + i], names[i]);
}

/* A on the delete screen: delete the chosen world. If it is gone from the
 * SD card afterwards it goes from the list too (and is no longer the world
 * played last). */
static void delete_chosen(WorldList *l, SaveSettings *kept)
{
    int id = s_ids[l->sel];

    save_world_delete(SAVE_ROOT, id);
    if (save_world_exists(SAVE_ROOT, id))
        return;
    memmove(&s_ids[l->sel], &s_ids[l->sel + 1], (size_t)(l->count - l->sel - 1) * sizeof s_ids[0]);
    worldlist_remove(l);
    if (kept->last == id) {
        kept->last = 0;
        save_settings_write(SAVE_ROOT, kept);
    }
}

StartChoice start_choose_world(uint32_t *seed, SaveSettings *kept, int *world, FpsCounter *fps,
                               u64 *paced)
{
    char names[WORLDLIST_ROWS][WORLDNAME_SIZE], asked[WORLDNAME_SIZE];
    WorldList list;
    Screen screen;
    int i, sel = 0, names_top = -1;
    int count = save_world_ids(SAVE_ROOT, s_ids, SAVE_WORLDS_MAX);

    for (i = 0; i < count; i++)
        if (s_ids[i] == kept->last)
            sel = i;
    worldlist_init(&list, count, sel);
    screen = count > 0 ? SCREEN_LIST : SCREEN_NEW;
    *seed = seed_random(svcGetSystemTick());
    hidSetRepeatParameters(REPEAT_DELAY, REPEAT_EVERY);
    while (aptMainLoop()) {
        u32 down, moved;
        int tx = -1, ty = -1;
        u16 fw = 0, fh = 0;
        u8 *fb;

        hidScanInput();
        down = hidKeysDown();
        moved = hidKeysDownRepeat();
        if (down & KEY_START)
            return START_QUIT;
        if (down & KEY_TOUCH) {
            touchPosition touch;

            hidTouchRead(&touch);
            tx = touch.px;
            ty = touch.py;
        }
        if (screen == SCREEN_LIST) {
            WorldListButton b = worldlist_button_at(tx, ty);
            int row = worldlist_row_at(&list, tx, ty);

            if (row >= 0)
                worldlist_choose(&list, row);
            if (moved & KEY_DUP)
                worldlist_move(&list, -1);
            if (moved & KEY_DDOWN)
                worldlist_move(&list, 1);
            if ((down & KEY_A) || b == WORLDLIST_OPEN) {
                *world = s_ids[list.sel];
                return START_OPEN;
            }
            if ((down & KEY_X) || b == WORLDLIST_NEW) {
                screen = SCREEN_NEW;
            } else if ((down & KEY_Y) || b == WORLDLIST_RENAME) {
                type_name(s_ids[list.sel]);
                names_top = -1;
            } else if ((down & KEY_L) || b == WORLDLIST_DELETE) {
                save_name_read(SAVE_ROOT, s_ids[list.sel], asked);
                screen = SCREEN_ASK;
            }
        } else if (screen == SCREEN_ASK) {
            WorldListAsk a = worldlist_ask_at(tx, ty);

            if ((down & KEY_A) || a == WORLDLIST_ASK_DELETE) {
                delete_chosen(&list, kept);
                names_top = -1;
                screen = list.count > 0 ? SCREEN_LIST : SCREEN_NEW;
            } else if ((down & KEY_B) || a == WORLDLIST_ASK_KEEP) {
                screen = SCREEN_LIST;
            }
        } else {
            bool back = list.count > 0;
            NewWorldButton tapped = newworld_button_at(tx, ty, back);

            if (back && ((down & KEY_B) || tapped == NEWWORLD_BACK))
                screen = SCREEN_LIST;
            else if ((down & KEY_A) || tapped == NEWWORLD_PLAY)
                return START_NEW;
            else if ((down & KEY_X) || tapped == NEWWORLD_RANDOM)
                *seed = seed_random(svcGetSystemTick());
            else if ((down & KEY_Y) || tapped == NEWWORLD_TYPE)
                type_seed(seed);
        }

        fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &fw, &fh);
        if (screen == SCREEN_LIST) {
            if (names_top != list.top) {
                read_names(&list, names);
                names_top = list.top;
            }
            worldlist_draw_bgr8(fb, fh, fw, &list, (const char(*)[WORLDNAME_SIZE])names);
        } else if (screen == SCREEN_ASK) {
            worldlist_ask_draw_bgr8(fb, fh, fw, asked);
        } else {
            newworld_draw_bgr8(fb, fh, fw, *seed, list.count > 0);
        }
        fps_draw_bgr8(fb, fh, fw, fps->shown);
        gfxFlushBuffers();
        gpu_frame_begin();
        gpu_frame_end();
        frame_end(fps, paced);
    }
    return START_QUIT;
}
