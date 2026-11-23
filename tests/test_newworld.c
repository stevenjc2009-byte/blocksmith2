#include <string.h>

#include "font.h"
#include "framebuffer.h"
#include "newworld.h"
#include "seed.h"
#include "test.h"

/* Row 2-03: the new-world screen, its seed and its three buttons; row
 * 2-05: its BACK button, shown when reached from the world list. */

#define BW 320
#define BH 240
#define GUARD 64

static unsigned char s_pic[BW * BH * 3 + GUARD];
static unsigned char s_ref[BW * BH * 3];

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

static void test_buttons_at(void)
{
    static const struct {
        int top;
        NewWorldButton b;
    } rows[3] = { { NEWWORLD_RANDOM_Y, NEWWORLD_RANDOM },
                  { NEWWORLD_TYPE_Y, NEWWORLD_TYPE },
                  { NEWWORLD_PLAY_Y, NEWWORLD_PLAY } };
    int i, x, y, inside_ok = 0, inside_n = 0, none_n = 0, none_ok = 0;

    for (i = 0; i < 3; i++) {
        int top = rows[i].top, l = NEWWORLD_BUTTON_X, r = NEWWORLD_BUTTON_X + NEWWORLD_BUTTON_W - 1,
            bot = top + NEWWORLD_BUTTON_H - 1;

        /* Every corner and the middle press it... */
        CHECK_EQ_INT(newworld_button_at(l, top, false), rows[i].b);
        CHECK_EQ_INT(newworld_button_at(r, top, false), rows[i].b);
        CHECK_EQ_INT(newworld_button_at(l, bot, false), rows[i].b);
        CHECK_EQ_INT(newworld_button_at(r, bot, false), rows[i].b);
        CHECK_EQ_INT(newworld_button_at(160, top + NEWWORLD_BUTTON_H / 2, false), rows[i].b);
        /* ...one step outside each edge does not. */
        CHECK_EQ_INT(newworld_button_at(l - 1, top + 5, false), NEWWORLD_NONE);
        CHECK_EQ_INT(newworld_button_at(r + 1, top + 5, false), NEWWORLD_NONE);
        CHECK_EQ_INT(newworld_button_at(160, top - 1, false), NEWWORLD_NONE);
        CHECK_EQ_INT(newworld_button_at(160, bot + 1, false), NEWWORLD_NONE);
    }
    /* Over the whole screen: a point presses the button whose box holds it,
     * and nothing anywhere else (the words, the gaps, the edges). */
    for (y = -5; y < BH + 5; y++)
        for (x = -5; x < BW + 5; x++) {
            NewWorldButton want = NEWWORLD_NONE;

            for (i = 0; i < 3; i++)
                if (x >= NEWWORLD_BUTTON_X && x < NEWWORLD_BUTTON_X + NEWWORLD_BUTTON_W &&
                    y >= rows[i].top && y < rows[i].top + NEWWORLD_BUTTON_H)
                    want = rows[i].b;
            if (want == NEWWORLD_NONE) {
                none_n++;
                none_ok += newworld_button_at(x, y, false) == NEWWORLD_NONE;
            } else {
                inside_n++;
                inside_ok += newworld_button_at(x, y, false) == want;
            }
        }
    CHECK_EQ_INT(inside_ok, inside_n);
    CHECK_EQ_INT(inside_n, 3 * NEWWORLD_BUTTON_W * NEWWORLD_BUTTON_H);
    CHECK_EQ_INT(none_ok, none_n);
    /* The buttons fit on the screen, below the seed, without touching. */
    CHECK_EQ_INT(NEWWORLD_SEED_Y + FONT_H * 2 < NEWWORLD_RANDOM_Y, 1);
    CHECK_EQ_INT(NEWWORLD_RANDOM_Y + NEWWORLD_BUTTON_H < NEWWORLD_TYPE_Y, 1);
    CHECK_EQ_INT(NEWWORLD_TYPE_Y + NEWWORLD_BUTTON_H < NEWWORLD_PLAY_Y, 1);
    CHECK_EQ_INT(NEWWORLD_PLAY_Y + NEWWORLD_BUTTON_H <= BH, 1);
    CHECK_EQ_INT(NEWWORLD_BUTTON_X >= 0 && NEWWORLD_BUTTON_X + NEWWORLD_BUTTON_W <= BW, 1);
}

/* The rows y0..y1-1 between columns x0..x1-1 of the picture match text
 * drawn by the font alone from row ty, its middle column on mid, on colour
 * (br, bg, bb)?
 * The pixels that differ; *lit the text's own pixels so empty text cannot
 * pass. */
static int text_wrong(int x0, int x1, int y0, int y1, int ty, int mid, const char *text, int scale,
                      int r, int g, int b, int br, int bg, int bb, int *lit)
{
    int x, y, wrong = 0;

    fb_fill_bgr8(s_ref, BW * BH, (unsigned char)br, (unsigned char)bg, (unsigned char)bb);
    font_draw_bgr8(s_ref, BW, BH, mid - font_text_width(text, scale) / 2, ty, text, scale,
                   (unsigned char)r, (unsigned char)g, (unsigned char)b);
    *lit = 0;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) {
            *lit += is_rgb(at(s_ref, x, y), r, g, b);
            wrong += memcmp(at(s_pic, x, y), at(s_ref, x, y), 3) != 0;
        }
    return wrong;
}

/* One box at (x0, y0), bw x bh: filled with its colour (bar its words), its
 * words white and centred, and the background one pixel round it. */
static void check_box(int x0, int y0, int bw, int bh, const char *text, int r, int g, int b,
                      int min_lit)
{
    int lit, x, y, edge_bg = 0, edge_n = 0;
    int ty = y0 + (bh - FONT_H * 2) / 2;

    CHECK_EQ_INT(text_wrong(x0, x0 + bw, y0, y0 + bh, ty, x0 + bw / 2, text, 2, 255, 255, 255, r,
                            g, b, &lit), 0);
    CHECK_EQ_INT(lit > min_lit, 1);
    /* The words sit in the middle rows: above and below them, fill only;
     * and they fit across the box with room either side. */
    CHECK_EQ_INT(ty - y0 >= 4 && y0 + bh - (ty + FONT_H * 2) >= 4, 1);
    CHECK_EQ_INT(font_text_width(text, 2) + 8 <= bw, 1);
    for (x = x0 - 1; x <= x0 + bw; x++) {
        edge_n += 2;
        edge_bg += is_rgb(at(s_pic, x, y0 - 1), NEWWORLD_BG_R, NEWWORLD_BG_G, NEWWORLD_BG_B);
        edge_bg += is_rgb(at(s_pic, x, y0 + bh), NEWWORLD_BG_R, NEWWORLD_BG_G, NEWWORLD_BG_B);
    }
    for (y = y0; y < y0 + bh; y++) {
        edge_n += 2;
        edge_bg += is_rgb(at(s_pic, x0 - 1, y), NEWWORLD_BG_R, NEWWORLD_BG_G, NEWWORLD_BG_B);
        edge_bg += is_rgb(at(s_pic, x0 + bw, y), NEWWORLD_BG_R, NEWWORLD_BG_G, NEWWORLD_BG_B);
    }
    CHECK_EQ_INT(edge_bg, edge_n);
}

/* One of the three big buttons, from row top. */
static void check_button(int top, const char *text, int r, int g, int b)
{
    check_box(NEWWORLD_BUTTON_X, top, NEWWORLD_BUTTON_W, NEWWORLD_BUTTON_H, text, r, g, b, 100);
}

/* The whole screen drawn for one seed; back: reached from the world list
 * (BACK in the top-right corner). */
static void check_drawn(uint32_t seed, bool back)
{
    char text[SEED_TEXT_SIZE];
    int lit, n, guard_ok = 1, x, y, bg_rows = 0;

    memset(s_pic, 0x77, sizeof s_pic);
    memset(s_pic + BW * BH * 3, 0xA5, GUARD);
    newworld_draw_bgr8(s_pic, BW, BH, seed, back);
    for (n = 0; n < GUARD; n++)
        guard_ok &= s_pic[BW * BH * 3 + n] == 0xA5;
    CHECK_EQ_INT(guard_ok, 1);

    /* The words, each exactly the font's letters, centred on the screen. */
    CHECK_EQ_INT(text_wrong(0, BW, NEWWORLD_TITLE_Y, NEWWORLD_TITLE_Y + FONT_H * 3,
                            NEWWORLD_TITLE_Y, BW / 2, "NEW WORLD", 3, 255, 255, 255,
                            NEWWORLD_BG_R, NEWWORLD_BG_G, NEWWORLD_BG_B, &lit), 0);
    CHECK_EQ_INT(lit > 100, 1);
    CHECK_EQ_INT(text_wrong(0, BW, NEWWORLD_LABEL_Y, NEWWORLD_LABEL_Y + FONT_H * 2,
                            NEWWORLD_LABEL_Y, BW / 2, "SEED", 2, 200, 200, 200, NEWWORLD_BG_R,
                            NEWWORLD_BG_G, NEWWORLD_BG_B, &lit), 0);
    CHECK_EQ_INT(lit > 40, 1);
    seed_to_text(seed, text);
    CHECK_EQ_INT(text_wrong(0, BW, NEWWORLD_SEED_Y, NEWWORLD_SEED_Y + FONT_H * 2, NEWWORLD_SEED_Y,
                            BW / 2, text, 2, SETTINGS_GOLD_R, SETTINGS_GOLD_G, SETTINGS_GOLD_B,
                            NEWWORLD_BG_R, NEWWORLD_BG_G, NEWWORLD_BG_B, &lit), 0);
    CHECK_EQ_INT(lit >= 8, 1);

    check_button(NEWWORLD_RANDOM_Y, "X  RANDOM SEED", NEWWORLD_BUTTON_R, NEWWORLD_BUTTON_G,
                 NEWWORLD_BUTTON_B);
    check_button(NEWWORLD_TYPE_Y, "Y  TYPE A SEED", NEWWORLD_BUTTON_R, NEWWORLD_BUTTON_G,
                 NEWWORLD_BUTTON_B);
    check_button(NEWWORLD_PLAY_Y, "A  PLAY", NEWWORLD_PLAY_R, NEWWORLD_PLAY_G, NEWWORLD_PLAY_B);
    if (back)
        check_box(NEWWORLD_BACK_X, NEWWORLD_BACK_Y, NEWWORLD_BACK_W, NEWWORLD_BACK_H, "B  BACK",
                  NEWWORLD_BUTTON_R, NEWWORLD_BUTTON_G, NEWWORLD_BUTTON_B, 60);

    /* Background everywhere else: above the title, between the lines, the
     * bands left and right of the buttons, and the corner BACK takes when
     * it is not shown. */
    for (y = 0; y < BH; y++) {
        int row_bg = 1;
        int in_words = (y >= NEWWORLD_TITLE_Y && y < NEWWORLD_TITLE_Y + FONT_H * 3) ||
                       (y >= NEWWORLD_LABEL_Y && y < NEWWORLD_LABEL_Y + FONT_H * 2) ||
                       (y >= NEWWORLD_SEED_Y && y < NEWWORLD_SEED_Y + FONT_H * 2);

        for (x = 0; x < BW; x++) {
            int in_button = newworld_button_at(x, y, back) != NEWWORLD_NONE;

            if (in_button || in_words)
                continue;
            row_bg &= is_rgb(at(s_pic, x, y), NEWWORLD_BG_R, NEWWORLD_BG_G, NEWWORLD_BG_B);
        }
        bg_rows += row_bg;
    }
    CHECK_EQ_INT(bg_rows, BH);
}

static void test_draw(void)
{
    char a[SEED_TEXT_SIZE], b[SEED_TEXT_SIZE];
    int x, y, outside_same = 1, inside_diff = 0;

    check_drawn(2026, false);
    check_drawn(0, false);
    check_drawn(0x80000000u, false); /* "-2147483648", the longest */
    check_drawn(0x7FFFFFFFu, false);
    check_drawn(seed_random(77), false);
    check_drawn(2026, true);
    check_drawn(0x80000000u, true);
    /* BACK changes the BACK box and nothing else. */
    newworld_draw_bgr8(s_pic, BW, BH, 2026, true);
    memcpy(s_ref, s_pic, BW * BH * 3);
    newworld_draw_bgr8(s_pic, BW, BH, 2026, false);
    for (y = 0; y < BH; y++)
        for (x = 0; x < BW; x++) {
            int same = memcmp(at(s_ref, x, y), at(s_pic, x, y), 3) == 0;

            if (newworld_button_at(x, y, true) == NEWWORLD_BACK)
                inside_diff += !same;
            else
                outside_same &= same;
        }
    CHECK_EQ_INT(outside_same, 1);
    CHECK_EQ_INT(inside_diff, NEWWORLD_BACK_W * NEWWORLD_BACK_H);
    /* Two seeds, two pictures: the seed line is what changes. */
    newworld_draw_bgr8(s_pic, BW, BH, 2026, false);
    memcpy(s_ref, s_pic, BW * BH * 3);
    newworld_draw_bgr8(s_pic, BW, BH, 2027, false);
    CHECK_EQ_INT(memcmp(s_ref, s_pic, BW * BH * 3) != 0, 1);
    seed_to_text(2026, a);
    seed_to_text(2027, b);
    CHECK_EQ_INT(strcmp(a, b) != 0, 1);
    /* The longest seed fits on the screen with room either side. */
    seed_to_text(0x80000000u, a);
    CHECK_EQ_INT(font_text_width(a, 2) < BW - 40, 1);
}

/* With BACK shown: it answers in its corner, the other buttons as before;
 * without it nothing answers there. */
static void test_back_at(void)
{
    int l = NEWWORLD_BACK_X, r = NEWWORLD_BACK_X + NEWWORLD_BACK_W - 1;
    int top = NEWWORLD_BACK_Y, bot = NEWWORLD_BACK_Y + NEWWORLD_BACK_H - 1;
    int x, y, same = 1, back_n = 0;

    CHECK_EQ_INT(newworld_button_at(l, top, true), NEWWORLD_BACK);
    CHECK_EQ_INT(newworld_button_at(r, top, true), NEWWORLD_BACK);
    CHECK_EQ_INT(newworld_button_at(l, bot, true), NEWWORLD_BACK);
    CHECK_EQ_INT(newworld_button_at(r, bot, true), NEWWORLD_BACK);
    CHECK_EQ_INT(newworld_button_at(l - 1, top + 5, true), NEWWORLD_NONE);
    CHECK_EQ_INT(newworld_button_at(r + 1, top + 5, true), NEWWORLD_NONE);
    CHECK_EQ_INT(newworld_button_at(l + 5, top - 1, true), NEWWORLD_NONE);
    CHECK_EQ_INT(newworld_button_at(l + 5, bot + 1, true), NEWWORLD_NONE);
    CHECK_EQ_INT(newworld_button_at(l + 5, top + 5, false), NEWWORLD_NONE);
    for (y = -5; y < BH + 5; y++)
        for (x = -5; x < BW + 5; x++) {
            NewWorldButton with = newworld_button_at(x, y, true),
                           without = newworld_button_at(x, y, false);

            if (with == NEWWORLD_BACK)
                back_n++;
            else
                same &= with == without;
            same &= without != NEWWORLD_BACK;
        }
    CHECK_EQ_INT(same, 1);
    CHECK_EQ_INT(back_n, NEWWORLD_BACK_W * NEWWORLD_BACK_H);
    /* On the screen, above the title, clear of the frames-a-second counter
     * in the top-left corner (fps.h: about 40 pixels wide). */
    CHECK_EQ_INT(NEWWORLD_BACK_Y >= 1 && NEWWORLD_BACK_Y + NEWWORLD_BACK_H < NEWWORLD_TITLE_Y, 1);
    CHECK_EQ_INT(NEWWORLD_BACK_X > BW / 2 && NEWWORLD_BACK_X + NEWWORLD_BACK_W < BW, 1);
}

void test_newworld(void)
{
    test_buttons_at();
    test_back_at();
    test_draw();
}
