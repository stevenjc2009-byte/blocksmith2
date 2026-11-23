#include <stdint.h>
#include <string.h>

#include "font.h"
#include "fps.h"
#include "framebuffer.h"
#include "hotbar.h"
#include "test.h"

/* Row 1-11: 30 frames a second, and the counter that shows it. */

#define R ((uint64_t)FPS_REFRESH_TICKS)
#define SEC ((uint64_t)FPS_TICKS_PER_SEC)
#define BW 320
#define BH 240

static unsigned char s_got[BW * BH * 3];
static unsigned char s_want[BW * BH * 3];

/* The game's wait, played against screens that refresh at R, 2R, 3R ...:
 * each wait for a refresh ends lat ticks after the next refresh. Returns
 * when the frame's wait ends, for a frame whose wait before ended at time
 * last and whose work took work ticks. */
static uint64_t frame_end(uint64_t last, uint64_t work, uint64_t lat)
{
    uint64_t t = last + work;

    do
        t = (t / R + 1) * R + lat; /* wake lat after the next refresh */
    while (!fps_pace_done(last, t));
    return t;
}

static void test_pace(void)
{
    static const uint64_t works2[] = { 0, R / 2, R - 1, R, R + R / 2, R + R * 3 / 4 };
    static const uint64_t works3[] = { 2 * R + R / 4, 2 * R + R * 3 / 4 };
    uint64_t lat, t;
    unsigned i;
    int k;

    CHECK_EQ_INT(fps_pace_done(1000, 1000), 0);
    CHECK_EQ_INT(fps_pace_done(1000, 1000 + R), 0);
    CHECK_EQ_INT(fps_pace_done(1000, 1000 + R * 3 / 2 - 1), 0);
    CHECK_EQ_INT(fps_pace_done(1000, 1000 + R * 3 / 2), 1);
    CHECK_EQ_INT(fps_pace_done(1000, 1000 + 2 * R), 1);
    /* Far into the 3DS's uptime as well. */
    CHECK_EQ_INT(fps_pace_done(SEC * 86400 * 30, SEC * 86400 * 30 + R), 0);
    CHECK_EQ_INT(fps_pace_done(SEC * 86400 * 30, SEC * 86400 * 30 + 2 * R), 1);

    /* Work under two refreshes: every frame takes exactly two refreshes,
     * whether the wait wakes at once or a fifth of a refresh late. */
    for (lat = 0; lat <= R / 5; lat += R / 5)
        for (i = 0; i < sizeof works2 / sizeof works2[0]; i++) {
            int bad = 0;

            t = 5 * R + lat;
            for (k = 0; k < 20; k++) {
                uint64_t next = frame_end(t, works2[i], lat);

                if (next - t != 2 * R)
                    bad++;
                t = next;
            }
            CHECK_EQ_INT(bad, 0);
        }
    /* Work over two refreshes: the frame ends on the third. */
    for (lat = 0; lat <= R / 5; lat += R / 5)
        for (i = 0; i < sizeof works3 / sizeof works3[0]; i++) {
            t = 5 * R + lat;
            CHECK_EQ_INT((int)(frame_end(t, works3[i], lat) - t == 3 * R), 1);
        }
}

/* Count frames step ticks apart, n of them, starting one step after t0. */
static void feed(FpsCounter *c, uint64_t *t, uint64_t step, int n)
{
    int k;

    for (k = 0; k < n; k++) {
        *t += step;
        fps_frame(c, *t);
    }
}

static void test_counter(void)
{
    FpsCounter c;
    uint64_t t = 7 * SEC, lat = 0;
    int k;

    /* 30 a second (two refreshes each): nothing until a full second. */
    fps_init(&c, t);
    CHECK_EQ_INT(c.shown, 0);
    feed(&c, &t, 2 * R, 29); /* 58 refreshes: just under a second */
    CHECK_EQ_INT(c.shown, 0);
    feed(&c, &t, 2 * R, 1); /* 60 refreshes: 1.003 s, 29.9 a second */
    CHECK_EQ_INT(c.shown, 30);
    CHECK_EQ_INT(c.frames, 0);
    feed(&c, &t, 2 * R, 30);
    CHECK_EQ_INT(c.shown, 30);

    /* 60 and 20 a second. */
    fps_init(&c, t);
    feed(&c, &t, R, 60);
    CHECK_EQ_INT(c.shown, 60);
    fps_init(&c, t);
    feed(&c, &t, 3 * R, 20);
    CHECK_EQ_INT(c.shown, 20);

    /* Rounding: 1 frame in 1 s is 1; 3 in 2 s is 1.5, shown 2; 5 in 2 s
     * is 2.5, shown 3; 7 in 2 s is 3.5, shown 4. */
    fps_init(&c, 0);
    fps_frame(&c, SEC);
    CHECK_EQ_INT(c.shown, 1);
    fps_init(&c, 0);
    fps_frame(&c, 1);
    fps_frame(&c, 2);
    fps_frame(&c, 2 * SEC);
    CHECK_EQ_INT(c.shown, 2);
    fps_init(&c, 0);
    for (k = 1; k <= 4; k++)
        fps_frame(&c, (uint64_t)k);
    fps_frame(&c, 2 * SEC);
    CHECK_EQ_INT(c.shown, 3);
    fps_init(&c, 0);
    for (k = 1; k <= 6; k++)
        fps_frame(&c, (uint64_t)k);
    fps_frame(&c, 2 * SEC);
    CHECK_EQ_INT(c.shown, 4);

    /* A long pause (the Home Menu): the count that holds it is low, the
     * next second is back to 30. */
    fps_init(&c, t);
    feed(&c, &t, 2 * R, 30);
    CHECK_EQ_INT(c.shown, 30);
    feed(&c, &t, 5 * SEC, 1);
    CHECK_EQ_INT(c.shown, 0);
    feed(&c, &t, 2 * R, 30);
    CHECK_EQ_INT(c.shown, 30);

    /* The game's own wait feeding the counter, work changing every frame
     * (a quarter to one and three quarter refreshes, waking a little
     * late): 30 every second. */
    t = 3 * R;
    fps_init(&c, t);
    for (k = 0; k < 300; k++) {
        lat = (uint64_t)(k % 4) * R / 20;
        t = frame_end(t, R / 4 + (uint64_t)(k * 7 % 12) * R / 8, lat);
        fps_frame(&c, t);
        if (k == 29)
            CHECK_EQ_INT(c.shown, 30);
    }
    CHECK_EQ_INT(c.shown, 30);
}

static void test_text(void)
{
    char s[FPS_TEXT_LEN];

    fps_text(30, s);
    CHECK_EQ_INT(strcmp(s, "30 FPS"), 0);
    fps_text(0, s);
    CHECK_EQ_INT(strcmp(s, "-- FPS"), 0);
    fps_text(-4, s);
    CHECK_EQ_INT(strcmp(s, "-- FPS"), 0);
    fps_text(7, s);
    CHECK_EQ_INT(strcmp(s, " 7 FPS"), 0);
    fps_text(10, s);
    CHECK_EQ_INT(strcmp(s, "10 FPS"), 0);
    fps_text(60, s);
    CHECK_EQ_INT(strcmp(s, "60 FPS"), 0);
    fps_text(99, s);
    CHECK_EQ_INT(strcmp(s, "99 FPS"), 0);
    fps_text(140, s);
    CHECK_EQ_INT(strcmp(s, "99 FPS"), 0);
}

/* The counter on the bottom screen: a 78 x 22 dark grey box at (4, 4), the
 * words in white at (8, 8), twice the font's size; nothing else changes. */
static void test_draw(void)
{
    static const int values[] = { 30, 0, 7 };
    unsigned i;
    int x, y;

    CHECK_EQ_INT(font_text_width("30 FPS", FPS_SCALE) + 2 * FPS_PAD, 78);
    CHECK_EQ_INT(FONT_H * FPS_SCALE + 2 * FPS_PAD, 22);
    /* Well clear of the hotbar. */
    CHECK_EQ_INT(FPS_BOX_Y + 22 < HOTBAR_Y0 - 2, 1);

    for (i = 0; i < sizeof values / sizeof values[0]; i++) {
        char text[FPS_TEXT_LEN];
        int wrong = 0, white = 0, grey = 0;

        fps_text(values[i], text);
        fb_fill_bgr8(s_got, BW * BH, 80, 150, 60);
        fb_fill_bgr8(s_want, BW * BH, 80, 150, 60);
        for (x = 4; x < 4 + 78; x++)
            for (y = 4; y < 4 + 22; y++)
                fb_put_bgr8(s_want, BW, BH, x, y, 24, 24, 24);
        font_draw_bgr8(s_want, BW, BH, 8, 8, text, 2, 255, 255, 255);

        fps_draw_bgr8(s_got, BW, BH, values[i]);
        for (x = 0; x < BW * BH * 3; x++)
            if (s_got[x] != s_want[x])
                wrong++;
        for (x = 0; x < BW * BH; x++) {
            if (s_got[x * 3] == 255 && s_got[x * 3 + 1] == 255 && s_got[x * 3 + 2] == 255)
                white++;
            if (s_got[x * 3] == 24 && s_got[x * 3 + 1] == 24 && s_got[x * 3 + 2] == 24)
                grey++;
        }
        CHECK_EQ_INT(wrong, 0);
        CHECK_EQ_INT(white > 100, 1);
        CHECK_EQ_INT(white + grey, 78 * 22);
    }
}

void test_fps(void)
{
    test_pace();
    test_counter();
    test_text();
    test_draw();
}
