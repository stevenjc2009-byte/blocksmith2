#include <stdio.h>
#include <string.h>

#include "font.h"
#include "framebuffer.h"
#include "settings.h"
#include "test.h"

/* Row 2-02: the settings screen and its view-distance slider. */

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

static int is_gold(const unsigned char *p)
{
    return is_rgb(p, SETTINGS_GOLD_R, SETTINGS_GOLD_G, SETTINGS_GOLD_B);
}

/* Pixels in the box x0..x1-1, y0..y1-1 whose colour is gold. */
static int gold_in(int x0, int y0, int x1, int y1)
{
    int x, y, n = 0;

    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            n += is_gold(at(s_pic, x, y));
    return n;
}

/* Does the band of rows y0..y1-1 of the settings picture match text drawn
 * by the font alone, its middle column on the screen's (BW / 2), on the
 * background? Counts the pixels that differ, and the text's own pixels so
 * an empty line cannot pass. */
static int line_wrong(int y0, int y1, const char *text, int scale, int r, int g, int b, int *lit)
{
    int x, y, wrong = 0;

    fb_fill_bgr8(s_ref, BW * BH, SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B);
    font_draw_bgr8(s_ref, BW, BH, BW / 2 - font_text_width(text, scale) / 2, y0, text, scale,
                   (unsigned char)r, (unsigned char)g, (unsigned char)b);
    *lit = 0;
    for (y = y0; y < y1; y++)
        for (x = 0; x < BW; x++) {
            *lit += is_rgb(at(s_ref, x, y), r, g, b);
            wrong += memcmp(at(s_pic, x, y), at(s_ref, x, y), 3) != 0;
        }
    return wrong;
}

static void test_init(void)
{
    Settings s;

    memset(&s, 0x5A, sizeof s);
    settings_init(&s);
    CHECK_EQ_INT(s.open, 0);
    CHECK_EQ_INT(s.view, 3);
    CHECK_EQ_INT(STREAM_VIEW_DEFAULT, 3);
}

static void test_stops(void)
{
    CHECK_EQ_INT(settings_stop_x(2), 40);
    CHECK_EQ_INT(settings_stop_x(3), 120);
    CHECK_EQ_INT(settings_stop_x(4), 200);
    CHECK_EQ_INT(settings_stop_x(5), 280);
    /* Nearest stop; half way goes right; past the ends stays on the end. */
    CHECK_EQ_INT(settings_view_at(40), 2);
    CHECK_EQ_INT(settings_view_at(79), 2);
    CHECK_EQ_INT(settings_view_at(80), 3);
    CHECK_EQ_INT(settings_view_at(120), 3);
    CHECK_EQ_INT(settings_view_at(159), 3);
    CHECK_EQ_INT(settings_view_at(160), 4);
    CHECK_EQ_INT(settings_view_at(239), 4);
    CHECK_EQ_INT(settings_view_at(240), 5);
    CHECK_EQ_INT(settings_view_at(280), 5);
    CHECK_EQ_INT(settings_view_at(0), 2);
    CHECK_EQ_INT(settings_view_at(1), 2);
    CHECK_EQ_INT(settings_view_at(319), 5);
    CHECK_EQ_INT(settings_view_at(-1), 2);
    CHECK_EQ_INT(settings_view_at(-100), 2);
    CHECK_EQ_INT(settings_view_at(-1000), 2);
    CHECK_EQ_INT(settings_view_at(5000), 5);
}

static void test_touch_and_step(void)
{
    Settings s;

    settings_init(&s);
    /* On the knob's rows and the numbers' rows, not above or below. */
    CHECK_EQ_INT(settings_touch(&s, 280, SETTINGS_TOUCH_Y0 - 1), 0);
    CHECK_EQ_INT(settings_touch(&s, 280, SETTINGS_TOUCH_Y1), 0);
    CHECK_EQ_INT(settings_touch(&s, 280, 30), 0);
    CHECK_EQ_INT(settings_touch(&s, 280, 220), 0);
    CHECK_EQ_INT(s.view, 3);
    CHECK_EQ_INT(settings_touch(&s, 275, SETTINGS_TOUCH_Y0), 1);
    CHECK_EQ_INT(s.view, 5);
    CHECK_EQ_INT(settings_touch(&s, 300, 115), 0); /* already there */
    CHECK_EQ_INT(s.view, 5);
    CHECK_EQ_INT(settings_touch(&s, 10, SETTINGS_TOUCH_Y1 - 1), 1);
    CHECK_EQ_INT(s.view, 2);
    CHECK_EQ_INT(settings_touch(&s, 190, 115), 1);
    CHECK_EQ_INT(s.view, 4);
    CHECK_EQ_INT(SETTINGS_TOUCH_Y0 <= SETTINGS_KNOB_Y, 1);
    CHECK_EQ_INT(SETTINGS_KNOB_Y + SETTINGS_KNOB_H <= SETTINGS_TOUCH_Y1, 1);
    CHECK_EQ_INT(SETTINGS_NUMBER_Y + FONT_H * 2 <= SETTINGS_TOUCH_Y1, 1);

    /* The D-pad: one stop at a time, stopping at the ends. */
    CHECK_EQ_INT(settings_step(&s, 1), 1);
    CHECK_EQ_INT(s.view, 5);
    CHECK_EQ_INT(settings_step(&s, 1), 0);
    CHECK_EQ_INT(s.view, 5);
    CHECK_EQ_INT(settings_step(&s, -1), 1);
    CHECK_EQ_INT(settings_step(&s, -1), 1);
    CHECK_EQ_INT(settings_step(&s, -1), 1);
    CHECK_EQ_INT(s.view, 2);
    CHECK_EQ_INT(settings_step(&s, -1), 0);
    CHECK_EQ_INT(s.view, 2);
    CHECK_EQ_INT(settings_step(&s, 1), 1);
    CHECK_EQ_INT(s.view, 3);
    /* Neither changes whether the screen is open. */
    CHECK_EQ_INT(s.open, 0);
    s.open = true;
    settings_step(&s, 1);
    settings_touch(&s, 40, 115);
    CHECK_EQ_INT(s.open, 1);
}

/* One view distance drawn: the words, the knob over its stop only, its
 * number gold and the others grey, and nothing outside the picture. */
static void check_drawn(int view)
{
    Settings s;
    char value[16];
    int v, n, lit, guard_ok = 1, bg_corners;

    settings_init(&s);
    s.view = view;
    memset(s_pic, 0x77, sizeof s_pic);
    memset(s_pic + BW * BH * 3, 0xA5, GUARD);
    settings_draw_bgr8(s_pic, BW, BH, &s);
    for (n = 0; n < GUARD; n++)
        guard_ok &= s_pic[BW * BH * 3 + n] == 0xA5;
    CHECK_EQ_INT(guard_ok, 1);

    /* The background reaches every corner and the empty bands. */
    bg_corners = is_rgb(at(s_pic, 0, 0), SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B) +
                 is_rgb(at(s_pic, BW - 1, 0), SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B) +
                 is_rgb(at(s_pic, 0, BH - 1), SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B) +
                 is_rgb(at(s_pic, BW - 1, BH - 1), SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B) +
                 is_rgb(at(s_pic, 160, 190), SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B) +
                 is_rgb(at(s_pic, 160, 20), SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B);
    CHECK_EQ_INT(bg_corners, 6);

    /* The lines of words, each exactly the font's letters, centred. */
    CHECK_EQ_INT(line_wrong(SETTINGS_TITLE_Y, SETTINGS_TITLE_Y + FONT_H * 3, "SETTINGS", 3, 255, 255, 255, &lit), 0);
    CHECK_EQ_INT(lit > 100, 1);
    CHECK_EQ_INT(line_wrong(SETTINGS_LABEL_Y, SETTINGS_LABEL_Y + FONT_H * 2, "VIEW DISTANCE", 2, 200, 200, 200, &lit), 0);
    CHECK_EQ_INT(lit > 100, 1);
    snprintf(value, sizeof value, "%d CHUNKS", view);
    CHECK_EQ_INT(line_wrong(SETTINGS_VALUE_Y, SETTINGS_VALUE_Y + FONT_H * 2, value, 2, 255, 255, 255, &lit), 0);
    CHECK_EQ_INT(lit > 100, 1);
    CHECK_EQ_INT(line_wrong(SETTINGS_HINT_Y, SETTINGS_HINT_Y + FONT_H * 2, "SELECT TO CLOSE", 2, 150, 150, 150, &lit), 0);
    CHECK_EQ_INT(lit > 100, 1);

    /* The knob: solid gold, 16 x 28, its middle column (the ninth) on the
     * chosen stop; no gold over any other stop's knob place. */
    for (v = STREAM_VIEW_MIN; v <= STREAM_VIEW_MAX; v++) {
        int x = settings_stop_x(v);

        n = gold_in(x - 8, SETTINGS_KNOB_Y, x + 8, SETTINGS_KNOB_Y + SETTINGS_KNOB_H);
        CHECK_EQ_INT(n, v == view ? 16 * 28 : 0);
    }
    CHECK_EQ_INT(gold_in(settings_stop_x(view) - 9, SETTINGS_KNOB_Y, settings_stop_x(view) - 8,
                         SETTINGS_KNOB_Y + SETTINGS_KNOB_H), 0);
    CHECK_EQ_INT(gold_in(settings_stop_x(view) + 8, SETTINGS_KNOB_Y, settings_stop_x(view) + 9,
                         SETTINGS_KNOB_Y + SETTINGS_KNOB_H), 0);
    CHECK_EQ_INT(gold_in(0, SETTINGS_KNOB_Y - 1, BW, SETTINGS_KNOB_Y), 0);
    CHECK_EQ_INT(gold_in(0, SETTINGS_KNOB_Y + SETTINGS_KNOB_H, BW, SETTINGS_NUMBER_Y), 0);

    /* The track joins the stops beside the knob; a mark stands at each
     * stop the knob is not on. */
    for (v = STREAM_VIEW_MIN; v <= STREAM_VIEW_MAX; v++) {
        int x = settings_stop_x(v);

        if (v == view)
            continue;
        CHECK_EQ_INT(is_rgb(at(s_pic, x, SETTINGS_TRACK_Y - 6), 120, 120, 120), 1);
        CHECK_EQ_INT(is_rgb(at(s_pic, x - 1, SETTINGS_TRACK_Y + 9), 120, 120, 120), 1);
        CHECK_EQ_INT(is_rgb(at(s_pic, x, SETTINGS_TRACK_Y - 7), SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B), 1);
        CHECK_EQ_INT(is_rgb(at(s_pic, x + 1, SETTINGS_TRACK_Y - 6), SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B), 1);
    }
    CHECK_EQ_INT(is_rgb(at(s_pic, 60, SETTINGS_TRACK_Y), 120, 120, 120), 1);
    CHECK_EQ_INT(is_rgb(at(s_pic, 260, SETTINGS_TRACK_Y + 3), 120, 120, 120), 1);
    CHECK_EQ_INT(is_rgb(at(s_pic, 160, SETTINGS_TRACK_Y + 4), SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B), 1);
    /* Nothing left of the first stop's mark (columns 39 and 40) or right
     * of the last one's (279 and 280), unless the knob is there. */
    CHECK_EQ_INT(is_rgb(at(s_pic, 37, SETTINGS_TRACK_Y + 1), SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B) ||
                 view == STREAM_VIEW_MIN, 1);
    CHECK_EQ_INT(is_rgb(at(s_pic, 290, SETTINGS_TRACK_Y + 1), SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B), 1);
    CHECK_EQ_INT(is_rgb(at(s_pic, 281, SETTINGS_TRACK_Y + 1), SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B) ||
                 view == STREAM_VIEW_MAX, 1);

    /* Each stop's number under it: gold for the chosen one, grey for the
     * rest; the font's letters, centred on the stop. */
    for (v = STREAM_VIEW_MIN; v <= STREAM_VIEW_MAX; v++) {
        int x = settings_stop_x(v), px, py, wrong = 0, want_lit = 0;
        char digit[2] = { (char)('0' + v), 0 };
        int r = v == view ? SETTINGS_GOLD_R : 160, g = v == view ? SETTINGS_GOLD_G : 160,
            b = v == view ? SETTINGS_GOLD_B : 160;

        fb_fill_bgr8(s_ref, BW * BH, SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B);
        font_draw_bgr8(s_ref, BW, BH, x - font_text_width(digit, 2) / 2, SETTINGS_NUMBER_Y, digit, 2,
                       (unsigned char)r, (unsigned char)g, (unsigned char)b);
        for (py = SETTINGS_NUMBER_Y; py < SETTINGS_NUMBER_Y + FONT_H * 2; py++)
            for (px = x - 20; px < x + 20; px++) {
                want_lit += is_rgb(at(s_ref, px, py), r, g, b);
                wrong += memcmp(at(s_pic, px, py), at(s_ref, px, py), 3) != 0;
            }
        CHECK_EQ_INT(wrong, 0);
        CHECK_EQ_INT(want_lit >= 20, 1);
    }
}

static void test_draw(void)
{
    int v;

    for (v = STREAM_VIEW_MIN; v <= STREAM_VIEW_MAX; v++)
        check_drawn(v);
}

void test_settings(void)
{
    test_init();
    test_stops();
    test_touch_and_step();
    test_draw();
}
