#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font.h"
#include "framebuffer.h"
#include "newworld.h"
#include "saveui.h"
#include "settings.h"
#include "test.h"
#include "worldlist.h"

/* Row 2-05: the world list and its delete screen as drawn (split from
 * test_worldlist.c, which checks the choosing, rows and buttons); row
 * 2-07: the backup buttons, the restore and replace screens, the notes. */

#define BW 320
#define BH 240
#define GUARD 64
#define ROWS WORLDLIST_ROWS

static unsigned char s_pic[BW * BH * 3 + GUARD];
static unsigned char s_ref[BW * BH * 3];
static unsigned char s_was[BW * BH * 3];
static int s_backed; /* check_list(): the chosen world has a backup */

/* Screen pixel (x, y) of a picture kept the way fb_put_bgr8() describes:
 * column by column, bottom row first, blue green red. */
static const unsigned char *at(const unsigned char *buf, int x, int y)
{
    return &buf[(x * BH + (BH - 1 - y)) * 3];
}

static int is_rgb(const unsigned char *p, int r, int g, int b)
{
    return p[0] == b && p[1] == g && p[2] == r;
}

static int is_bg(int x, int y)
{
    return is_rgb(at(s_pic, x, y), SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B);
}

/* The pixels in columns x0..x1-1, rows y0..y1-1 that differ from text drawn
 * by the font alone from (tx, ty) on colour (br, bg, bb); *lit the text's
 * own pixels there, so empty text cannot pass. */
static int text_wrong(int x0, int x1, int y0, int y1, int tx, int ty, const char *text, int scale,
                      int r, int g, int b, int br, int bg, int bb, int *lit)
{
    int x, y, wrong = 0;

    fb_fill_bgr8(s_ref, BW * BH, (unsigned char)br, (unsigned char)bg, (unsigned char)bb);
    font_draw_bgr8(s_ref, BW, BH, tx, ty, text, scale, (unsigned char)r, (unsigned char)g,
                   (unsigned char)b);
    *lit = 0;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) {
            *lit += is_rgb(at(s_ref, x, y), r, g, b);
            wrong += memcmp(at(s_pic, x, y), at(s_ref, x, y), 3) != 0;
        }
    return wrong;
}

/* text_wrong() with the text's middle column on mid. */
static int centred_wrong(int x0, int x1, int y0, int y1, int mid, int ty, const char *text,
                         int scale, int r, int g, int b, int *lit)
{
    return text_wrong(x0, x1, y0, y1, mid - font_text_width(text, scale) / 2, ty, text, scale, r,
                      g, b, SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B, lit);
}

/* The background on every pixel one step round the box at (x0, y0). */
static int ring_bg(int x0, int y0, int w, int h)
{
    int x, y, ok = 1;

    for (x = x0 - 1; x <= x0 + w; x++)
        ok &= is_bg(x, y0 - 1) && is_bg(x, y0 + h);
    for (y = y0; y < y0 + h; y++)
        ok &= is_bg(x0 - 1, y) && is_bg(x0 + w, y);
    return ok;
}

/* A button at (x0, y0): its colour, its words white in the middle, the
 * background round it. */
static void check_button(int x0, int y0, const char *text, int r, int g, int b)
{
    int lit, ty = y0 + (WORLDLIST_BUTTON_H - FONT_H * 2) / 2;

    CHECK_EQ_INT(text_wrong(x0, x0 + WORLDLIST_BUTTON_W, y0, y0 + WORLDLIST_BUTTON_H,
                            x0 + WORLDLIST_BUTTON_W / 2 - font_text_width(text, 2) / 2, ty, text,
                            2, 255, 255, 255, r, g, b, &lit), 0);
    CHECK_EQ_INT(lit > 60, 1);
    CHECK_EQ_INT(ring_bg(x0, y0, WORLDLIST_BUTTON_W, WORLDLIST_BUTTON_H), 1);
}

static void draw_list(const WorldList *l, char names[ROWS][WORLDNAME_SIZE])
{
    int n, guard_ok = 1;

    memset(s_pic, 0x77, sizeof s_pic);
    memset(s_pic + BW * BH * 3, 0xA5, GUARD);
    worldlist_draw_bgr8(s_pic, BW, BH, l, (const char(*)[WORLDNAME_SIZE])names);
    for (n = 0; n < GUARD; n++)
        guard_ok &= s_pic[BW * BH * 3 + n] == 0xA5;
    CHECK_EQ_INT(guard_ok, 1);
}

/* The list drawn: "WORLDS" and the count, a row for each world shown (the
 * chosen one blue) with its name, the buttons (RESTORE only if s_backed),
 * background elsewhere (empty rows too, whatever names[] holds for them). */
static void check_list(int count, int sel, char names[ROWS][WORLDNAME_SIZE])
{
    WorldList l;
    char text[32];
    int i, x, y, lit, wrong, white, grey, bg_ok = 1;
    int band = WORLDLIST_TITLE_Y + FONT_H * 2;

    worldlist_init(&l, count, sel);
    l.backed_up = s_backed != 0;
    draw_list(&l, names);
    /* The title band against both lines drawn alone. */
    snprintf(text, sizeof text, "%d OF %d", count > 0 ? l.sel + 1 : 0, count);
    fb_fill_bgr8(s_ref, BW * BH, SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B);
    font_draw_bgr8(s_ref, BW, BH, WORLDLIST_TITLE_X, WORLDLIST_TITLE_Y, "WORLDS", 2, 255, 255, 255);
    font_draw_bgr8(s_ref, BW, BH, WORLDLIST_COUNT_RIGHT - font_text_width(text, 2),
                   WORLDLIST_TITLE_Y, text, 2, 200, 200, 200);
    wrong = white = grey = 0;
    for (y = 0; y < band; y++)
        for (x = 0; x < BW; x++) {
            wrong += memcmp(at(s_pic, x, y), at(s_ref, x, y), 3) != 0;
            white += is_rgb(at(s_pic, x, y), 255, 255, 255);
            grey += is_rgb(at(s_pic, x, y), 200, 200, 200);
        }
    CHECK_EQ_INT(wrong, 0);
    CHECK_EQ_INT(white > 60 && grey > 40, 1);
    /* The rows. */
    for (i = 0; i < ROWS && l.top + i < count; i++) {
        int y0 = WORLDLIST_ROW_Y + i * WORLDLIST_ROW_STEP, chosen = l.top + i == l.sel;

        CHECK_EQ_INT(text_wrong(WORLDLIST_ROW_X, WORLDLIST_ROW_X + WORLDLIST_ROW_W, y0,
                                y0 + WORLDLIST_ROW_H, WORLDLIST_ROW_X + WORLDLIST_NAME_INSET,
                                y0 + (WORLDLIST_ROW_H - FONT_H * 2) / 2, names[i], 2, 255, 255,
                                255, chosen ? NEWWORLD_PLAY_R : NEWWORLD_BUTTON_R,
                                chosen ? NEWWORLD_PLAY_G : NEWWORLD_BUTTON_G,
                                chosen ? NEWWORLD_PLAY_B : NEWWORLD_BUTTON_B, &lit), 0);
        CHECK_EQ_INT(lit > 8, 1);
        CHECK_EQ_INT(ring_bg(WORLDLIST_ROW_X, y0, WORLDLIST_ROW_W, WORLDLIST_ROW_H), 1);
    }
    check_button(WORLDLIST_LEFT_X, WORLDLIST_TOP_Y, "A  OPEN", NEWWORLD_PLAY_R, NEWWORLD_PLAY_G,
                 NEWWORLD_PLAY_B);
    check_button(WORLDLIST_RIGHT_X, WORLDLIST_TOP_Y, "X  NEW", NEWWORLD_BUTTON_R,
                 NEWWORLD_BUTTON_G, NEWWORLD_BUTTON_B);
    check_button(WORLDLIST_LEFT_X, WORLDLIST_LOW_Y, "Y  RENAME", NEWWORLD_BUTTON_R,
                 NEWWORLD_BUTTON_G, NEWWORLD_BUTTON_B);
    check_button(WORLDLIST_RIGHT_X, WORLDLIST_LOW_Y, "L  DELETE", NEWWORLD_BUTTON_R,
                 NEWWORLD_BUTTON_G, NEWWORLD_BUTTON_B);
    check_button(WORLDLIST_LEFT_X, WORLDLIST_BACKUP_Y, "R  BACK UP", NEWWORLD_BUTTON_R,
                 NEWWORLD_BUTTON_G, NEWWORLD_BUTTON_B);
    if (s_backed)
        check_button(WORLDLIST_RIGHT_X, WORLDLIST_BACKUP_Y, "B  RESTORE", NEWWORLD_BUTTON_R,
                     NEWWORLD_BUTTON_G, NEWWORLD_BUTTON_B);
    for (y = band; y < BH; y++)
        for (x = 0; x < BW; x++) {
            WorldListButton b = worldlist_button_at(x, y);

            if (worldlist_row_at(&l, x, y) < 0 &&
                (b == WORLDLIST_NONE || (b == WORLDLIST_RESTORE && !s_backed)))
                bg_ok &= is_bg(x, y);
        }
    CHECK_EQ_INT(bg_ok, 1);
}

static void test_draw(void)
{
    char names[ROWS][WORLDNAME_SIZE] = { "WORLD 1", "MY HOUSE", "WWWWWWWWWWWWWWWW", "-" };
    char junk[ROWS][WORLDNAME_SIZE] = { "A", "JUNK ROW", "JUNK ROW", "JUNK ROW" };
    WorldList l;
    int i;

    check_list(4, 0, names);
    check_list(4, 3, names);
    check_list(9, 7, names);    /* "8 OF 9", rows 4..7 */
    check_list(9999, 9998, names);
    check_list(1, 0, junk);     /* the junk in rows 1..3 is not drawn */
    check_list(2, 1, junk);
    check_list(0, 0, junk);     /* "0 OF 0", no rows */
    s_backed = 1;               /* with a backup: RESTORE too */
    check_list(4, 0, names);
    check_list(9, 7, names);
    check_list(1, 0, junk);
    s_backed = 0;
    /* A backup changes RESTORE's box and nothing else. */
    worldlist_init(&l, 4, 2);
    draw_list(&l, names);
    memcpy(s_was, s_pic, BW * BH * 3);
    l.backed_up = true;
    draw_list(&l, names);
    {
        int x, y, box_diff = 0, other_same = 1;

        for (y = 0; y < BH; y++)
            for (x = 0; x < BW; x++) {
                int same = memcmp(at(s_was, x, y), at(s_pic, x, y), 3) == 0;

                if (worldlist_button_at(x, y) == WORLDLIST_RESTORE)
                    box_diff += !same;
                else
                    other_same &= same;
            }
        CHECK_EQ_INT(other_same, 1);
        CHECK_EQ_INT(box_diff > WORLDLIST_BUTTON_W * WORLDLIST_BUTTON_H * 3 / 4, 1);
    }
    /* The chosen row is what changes between two choices. */
    worldlist_init(&l, 4, 0);
    draw_list(&l, names);
    memcpy(s_ref, s_pic, BW * BH * 3);
    worldlist_choose(&l, 1);
    draw_list(&l, names);
    {
        int x, y, rows_diff = 0, other_same = 1;

        for (y = WORLDLIST_ROW_Y; y < BH; y++)
            for (x = 0; x < BW; x++) {
                int same = memcmp(at(s_ref, x, y), at(s_pic, x, y), 3) == 0;
                int r = worldlist_row_at(&l, x, y);

                if (r == 0 || r == 1)
                    rows_diff += !same;
                else
                    other_same &= same;
            }
        CHECK_EQ_INT(other_same, 1);
        /* Every pixel of rows 0 and 1 bar their names' letters. */
        CHECK_EQ_INT(rows_diff > 2 * WORLDLIST_ROW_W * WORLDLIST_ROW_H * 3 / 4, 1);
    }
    for (i = 0; i < ROWS; i++)
        CHECK_EQ_INT(worldname_valid(names[i]), 1);
}

/* An asking screen for one name: title, the name, the warning, the button
 * that does it (red) and KEEP. */
static void check_question(WorldListAsking kind, const char *name, const char *title,
                           const char *warn, const char *left)
{
    int n, x, y, lit, guard_ok = 1, bg_ok = 1;
    int title_end = WORLDLIST_ASK_TITLE_Y + FONT_H * 3;

    memset(s_pic, 0x77, sizeof s_pic);
    memset(s_pic + BW * BH * 3, 0xA5, GUARD);
    worldlist_question_draw_bgr8(s_pic, BW, BH, kind, name);
    for (n = 0; n < GUARD; n++)
        guard_ok &= s_pic[BW * BH * 3 + n] == 0xA5;
    CHECK_EQ_INT(guard_ok, 1);
    CHECK_EQ_INT(centred_wrong(0, BW, WORLDLIST_ASK_TITLE_Y, title_end, BW / 2,
                               WORLDLIST_ASK_TITLE_Y, title, 3, 255, 255, 255, &lit), 0);
    CHECK_EQ_INT(lit > 150, 1);
    CHECK_EQ_INT(centred_wrong(0, BW, WORLDLIST_ASK_NAME_Y, WORLDLIST_ASK_NAME_Y + FONT_H * 2,
                               BW / 2, WORLDLIST_ASK_NAME_Y, name, 2, SETTINGS_GOLD_R,
                               SETTINGS_GOLD_G, SETTINGS_GOLD_B, &lit), 0);
    CHECK_EQ_INT(lit > 8, 1);
    CHECK_EQ_INT(centred_wrong(0, BW, WORLDLIST_ASK_WARN_Y, WORLDLIST_ASK_WARN_Y + FONT_H * 2,
                               BW / 2, WORLDLIST_ASK_WARN_Y, warn, 2, 200, 200, 200, &lit), 0);
    CHECK_EQ_INT(lit > 100, 1);
    check_button(WORLDLIST_LEFT_X, WORLDLIST_TOP_Y, left, WORLDLIST_RED_R, WORLDLIST_RED_G,
                 WORLDLIST_RED_B);
    check_button(WORLDLIST_RIGHT_X, WORLDLIST_TOP_Y, "B  KEEP", NEWWORLD_BUTTON_R,
                 NEWWORLD_BUTTON_G, NEWWORLD_BUTTON_B);
    for (y = 0; y < BH; y++) {
        int words = (y >= WORLDLIST_ASK_TITLE_Y && y < title_end) ||
                    (y >= WORLDLIST_ASK_NAME_Y && y < WORLDLIST_ASK_NAME_Y + FONT_H * 2) ||
                    (y >= WORLDLIST_ASK_WARN_Y && y < WORLDLIST_ASK_WARN_Y + FONT_H * 2);

        for (x = 0; x < BW; x++)
            if (!words && worldlist_ask_at(x, y) == WORLDLIST_ASK_NONE)
                bg_ok &= is_bg(x, y);
    }
    CHECK_EQ_INT(bg_ok, 1);
}

static void check_ask(const char *name)
{
    check_question(WORLDLIST_ASKING_DELETE, name, "DELETE WORLD", "IT CANNOT BE UNDONE",
                   "A  DELETE");
    memcpy(s_was, s_pic, BW * BH * 3);
    worldlist_ask_draw_bgr8(s_pic, BW, BH, name); /* the delete screen, the same */
    CHECK_EQ_INT(memcmp(s_was, s_pic, BW * BH * 3), 0);
    check_question(WORLDLIST_ASKING_RESTORE, name, "RESTORE WORLD", "ITS BACKUP REPLACES IT",
                   "A  RESTORE");
    check_question(WORLDLIST_ASKING_REPLACE, name, "REPLACE BACKUP", "THE OLD BACKUP IS LOST",
                   "A  BACK UP");
}

/* A note over the list: its words centred in its box, red if failed,
 * nothing outside the box changed. */
static void check_note(const char *text, int failed)
{
    char names[ROWS][WORLDNAME_SIZE] = { "WORLD 1", "MY HOUSE", "WWWWWWWWWWWWWWWW", "-" };
    WorldList l;
    int x, y, lit, same = 1, tw = font_text_width(text, 2);
    int x0 = (BW - tw) / 2 - SAVEUI_MSG_PAD, x1 = x0 + tw + 2 * SAVEUI_MSG_PAD;

    worldlist_init(&l, 4, 1);
    draw_list(&l, names);
    memcpy(s_was, s_pic, BW * BH * 3);
    worldlist_note_draw_bgr8(s_pic, BW, BH, text, failed != 0);
    for (y = 0; y < BH; y++)
        for (x = 0; x < BW; x++)
            if (x < x0 || x >= x1 || y < SAVEUI_MSG_Y || y >= SAVEUI_MSG_Y + SAVEUI_MSG_H)
                same &= memcmp(at(s_was, x, y), at(s_pic, x, y), 3) == 0;
    CHECK_EQ_INT(same, 1);
    CHECK_EQ_INT(text_wrong(x0, x1, SAVEUI_MSG_Y, SAVEUI_MSG_Y + SAVEUI_MSG_H, (BW - tw) / 2,
                            SAVEUI_MSG_Y + 8, text, 2, failed ? SAVEUI_FAIL_R : 255,
                            failed ? SAVEUI_FAIL_G : 255, failed ? SAVEUI_FAIL_B : 255,
                            SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B, &lit), 0);
    CHECK_EQ_INT(lit > 40, 1);
    CHECK_EQ_INT(x0 > 0 && x1 < BW, 1);
}

static void test_ask_draw(void)
{
    check_ask("WORLD 1");
    check_ask("MY HOUSE");
    check_ask("WWWWWWWWWWWWWWWW");
    check_note("BACKUP MADE", 0);
    check_note("WORLD RESTORED", 0);
    check_note("BACKUP FAILED", 1);
    check_note("RESTORE FAILED", 1);
    CHECK_EQ_INT(font_text_width("DELETE WORLD", 3) + 20 < BW, 1);
    CHECK_EQ_INT(font_text_width("REPLACE BACKUP", 3) + 20 < BW, 1);
    CHECK_EQ_INT(font_text_width("ITS BACKUP REPLACES IT", 2) + 20 < BW, 1);
    CHECK_EQ_INT(WORLDLIST_ASK_TITLE_Y + FONT_H * 3 < WORLDLIST_ASK_NAME_Y, 1);
    CHECK_EQ_INT(WORLDLIST_ASK_NAME_Y + FONT_H * 2 < WORLDLIST_ASK_WARN_Y, 1);
    CHECK_EQ_INT(WORLDLIST_ASK_WARN_Y + FONT_H * 2 < WORLDLIST_TOP_Y, 1);
}

void test_worldlist_draw(void)
{
    test_draw();
    test_ask_draw();
}
