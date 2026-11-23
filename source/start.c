#include <3ds.h>
#include <string.h>

#include "bgjob.h"
#include "frame.h"
#include "gpu.h"
#include "newworld.h"
#include "save.h"
#include "seed.h"
#include "start.h"
#include "worldlist.h"
#include "worldname.h"

/* The start screens' loop (start.h): the world list, its asking screens
 * (delete, restore, replace a backup) and the new-world screen. */

/* Holding the D-pad on the world list moves on after REPEAT_DELAY scans,
 * then every REPEAT_EVERY (30 scans a second). */
#define REPEAT_DELAY 15
#define REPEAT_EVERY 4

typedef enum {
    SCREEN_LIST, /* the world list */
    SCREEN_ASK,  /* delete or restore the chosen world, or replace its backup? */
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

/* The names in the list's rows, and whether each has a backup, from the
 * SD card. Read on a scroll or after a change, never on a cursor move: an
 * SD card read there dropped a frame (row 2-07). */
static void read_names(const WorldList *l, char names[WORLDLIST_ROWS][WORLDNAME_SIZE],
                       bool backed[WORLDLIST_ROWS])
{
    int i;

    for (i = 0; i < WORLDLIST_ROWS && l->top + i < l->count; i++) {
        save_name_read(SAVE_ROOT, s_ids[l->top + i], names[i]);
        backed[i] = save_backup_exists(SAVE_ROOT, s_ids[l->top + i]);
    }
}

/* A note over the world list, shown until its time is up. */
typedef struct {
    const char *text;
    bool failed;
    u64 until; /* osGetTime() */
} Note;

static void note_show(Note *n, bool ok, const char *done, const char *failed)
{
    n->text = ok ? done : failed;
    n->failed = !ok;
    n->until = osGetTime() + WORLDLIST_NOTE_MS;
}

/* A backup, a restore or a delete of one world, run on bgjob.h's thread
 * (fix batch, problem 6: a backup stopped the list for half a second or
 * more; overnight run 4-01: a delete for 6 to 9 frames). While it runs the
 * list is drawn with a note and keys and touches wait; nothing else here
 * touches the SD card or s_ids meanwhile. The rows' names are read again
 * on the thread too, once it is done (on the game's thread they took a
 * tenth of a second). */
typedef enum { JOB_BACKUP, JOB_RESTORE, JOB_DELETE } JobKind;

typedef struct {
    int id;
    JobKind kind;
    WorldList list;   /* the list as it started; only a delete changes it */
    SaveSettings kept; /* a delete: the settings, last played world and all */
    char names[WORLDLIST_ROWS][WORLDNAME_SIZE];
    bool backed[WORLDLIST_ROWS];
} Copying;

static Copying s_copying;

/* A delete: if the world is gone from the SD card afterwards it goes from
 * the list too (and is no longer the world played last). */
static int delete_world(Copying *c)
{
    WorldList *l = &c->list;

    save_world_delete(SAVE_ROOT, c->id);
    if (save_world_exists(SAVE_ROOT, c->id))
        return 0;
    memmove(&s_ids[l->sel], &s_ids[l->sel + 1], (size_t)(l->count - l->sel - 1) * sizeof s_ids[0]);
    worldlist_remove(l);
    if (c->kept.last == c->id) {
        c->kept.last = 0;
        save_settings_write(SAVE_ROOT, &c->kept);
    }
    return 1;
}

static int copy_world(void *arg)
{
    Copying *c = arg;
    int ok = c->kind == JOB_DELETE    ? delete_world(c)
             : c->kind == JOB_RESTORE ? save_backup_restore(SAVE_ROOT, c->id)
                                      : save_backup_make(SAVE_ROOT, c->id);

    read_names(&c->list, c->names, c->backed); /* RESTORE shown, an old name back */
    return ok;
}

static void copy_start(Note *n, const WorldList *l, const SaveSettings *kept, JobKind kind)
{
    static const char *const k_running[] = { "BACKING UP", "RESTORING", "DELETING" };

    s_copying.id = s_ids[l->sel];
    s_copying.kind = kind;
    s_copying.list = *l;
    s_copying.kept = *kept;
    bgjob_start(copy_world, &s_copying);
    n->text = k_running[kind];
    n->failed = false;
    n->until = UINT64_MAX;
}

/* The job has ended: its note (none after a delete, as before it ran on
 * the thread), the rows' names and backups as it left them, the list and
 * the world played last after a delete, and true. */
static bool copy_done(Note *n, char names[WORLDLIST_ROWS][WORLDNAME_SIZE],
                      bool backed[WORLDLIST_ROWS], WorldList *l, SaveSettings *kept)
{
    int ok;

    if (!bgjob_collect(&ok))
        return false;
    memcpy(names, s_copying.names, sizeof s_copying.names);
    memcpy(backed, s_copying.backed, sizeof s_copying.backed);
    if (s_copying.kind == JOB_DELETE) {
        *l = s_copying.list;
        kept->last = s_copying.kept.last;
        n->text = NULL;
    } else if (s_copying.kind == JOB_RESTORE) {
        note_show(n, ok, "WORLD RESTORED", "RESTORE FAILED");
    } else {
        note_show(n, ok, "BACKUP MADE", "BACKUP FAILED");
    }
    return true;
}

StartChoice start_choose_world(uint32_t *seed, SaveSettings *kept, int *world, FpsCounter *fps,
                               u64 *paced)
{
    char names[WORLDLIST_ROWS][WORLDNAME_SIZE], asked[WORLDNAME_SIZE];
    bool backed[WORLDLIST_ROWS] = { false }; /* the rows' worlds have a backup */
    WorldList list;
    Screen screen;
    WorldListAsking asking = WORLDLIST_ASKING_DELETE;
    Note note = { NULL, false, 0 };
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
        if (bgjob_started()) { /* a backup, a restore or a delete: only START, after it */
            if (copy_done(&note, names, backed, &list, kept)) {
                names_top = list.top; /* read on the thread */
                if (list.count == 0)
                    screen = SCREEN_NEW; /* the last world was deleted */
            }
            down &= KEY_START;
            moved = 0;
        }
        if (down & KEY_START) {
            bgjob_wait();
            copy_done(&note, names, backed, &list, kept);
            return START_QUIT;
        }
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
            if (names_top != list.top) {
                read_names(&list, names, backed);
                names_top = list.top;
            }
            list.backed_up = list.sel >= list.top && list.sel - list.top < WORLDLIST_ROWS &&
                             backed[list.sel - list.top];
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
                asking = WORLDLIST_ASKING_DELETE;
                save_name_read(SAVE_ROOT, s_ids[list.sel], asked);
                screen = SCREEN_ASK;
            } else if (((down & KEY_R) || b == WORLDLIST_BACKUP) && list.backed_up) {
                asking = WORLDLIST_ASKING_REPLACE;
                save_name_read(SAVE_ROOT, s_ids[list.sel], asked);
                screen = SCREEN_ASK;
            } else if ((down & KEY_R) || b == WORLDLIST_BACKUP) {
                copy_start(&note, &list, kept, JOB_BACKUP);
            } else if (((down & KEY_B) || b == WORLDLIST_RESTORE) && list.backed_up) {
                asking = WORLDLIST_ASKING_RESTORE;
                save_name_read(SAVE_ROOT, s_ids[list.sel], asked);
                screen = SCREEN_ASK;
            }
        } else if (screen == SCREEN_ASK) {
            WorldListAsk a = worldlist_ask_at(tx, ty);

            if (((down & KEY_A) || a == WORLDLIST_ASK_DELETE) && asking == WORLDLIST_ASKING_DELETE) {
                copy_start(&note, &list, kept, JOB_DELETE);
                screen = SCREEN_LIST;
            } else if ((down & KEY_A) || a == WORLDLIST_ASK_DELETE) {
                copy_start(&note, &list, kept,
                           asking == WORLDLIST_ASKING_RESTORE ? JOB_RESTORE : JOB_BACKUP);
                screen = SCREEN_LIST;
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
                read_names(&list, names, backed);
                names_top = list.top;
            }
            worldlist_draw_bgr8(fb, fh, fw, &list, (const char(*)[WORLDNAME_SIZE])names);
            if (note.text != NULL && osGetTime() < note.until)
                worldlist_note_draw_bgr8(fb, fh, fw, note.text, note.failed);
        } else if (screen == SCREEN_ASK) {
            worldlist_question_draw_bgr8(fb, fh, fw, asking, asked);
        } else {
            newworld_draw_bgr8(fb, fh, fw, *seed, list.count > 0);
        }
        fps_draw_bgr8(fb, fh, fw, fps->shown);
        gfxFlushBuffers();
        gpu_frame_begin();
        gpu_frame_end();
        frame_end(fps, paced);
    }
    bgjob_wait(); /* the system closing the game */
    copy_done(&note, names, backed, &list, kept);
    return START_QUIT;
}
