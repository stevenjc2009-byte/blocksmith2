#include <string.h>

#include "font.h"
#include "framebuffer.h"
#include "hotbar.h"
#include "saveui.h"
#include "settings.h"
#include "test.h"

/* Row 2-04: the SAVE button and the save message. */

#define BW 320
#define BH 240
#define GUARD 64
#define TEXT_SCALE 2

static unsigned char s_pic[BW * BH * 3 + GUARD];
static unsigned char s_ref[BW * BH * 3];

/* Screen pixel (x, y), kept the way fb_put_bgr8() describes. */
static const unsigned char *at(const unsigned char *buf, int x, int y)
{
    return &buf[(x * BH + (BH - 1 - y)) * 3];
}

static int is_rgb(const unsigned char *p, int r, int g, int b)
{
    return p[0] == b && p[1] == g && p[2] == r;
}

static void test_hit(void)
{
    int x, y, in_ok = 0, in_n = 0, out_ok = 0, out_n = 0;
    int l = SAVEUI_X, r = SAVEUI_X + SAVEUI_WIDTH - 1, t = SAVEUI_Y, b = SAVEUI_Y + SAVEUI_HEIGHT - 1;

    CHECK_EQ_INT(saveui_hit(l, t), 1);
    CHECK_EQ_INT(saveui_hit(r, t), 1);
    CHECK_EQ_INT(saveui_hit(l, b), 1);
    CHECK_EQ_INT(saveui_hit(r, b), 1);
    CHECK_EQ_INT(saveui_hit(l - 1, t + 5), 0);
    CHECK_EQ_INT(saveui_hit(r + 1, t + 5), 0);
    CHECK_EQ_INT(saveui_hit(l + 5, t - 1), 0);
    CHECK_EQ_INT(saveui_hit(l + 5, b + 1), 0);
    for (y = -5; y < BH + 5; y++)
        for (x = -5; x < BW + 5; x++) {
            int want = x >= l && x <= r && y >= t && y <= b;

            if (want) {
                in_n++;
                in_ok += saveui_hit(x, y);
            } else {
                out_n++;
                out_ok += !saveui_hit(x, y);
            }
            /* No hotbar slot under the button. */
            if (want && hotbar_slot_at(x, y) >= 0)
                in_ok = -1000000;
        }
    CHECK_EQ_INT(in_ok, in_n);
    CHECK_EQ_INT(in_n, SAVEUI_WIDTH * SAVEUI_HEIGHT);
    CHECK_EQ_INT(out_ok, out_n);
    /* On the screen, clear of the frame counter's corner (x < 86, y < 28 at
     * most) and above the hotbar. */
    CHECK_EQ_INT(SAVEUI_X + SAVEUI_WIDTH <= BW && SAVEUI_X >= 86, 1);
    CHECK_EQ_INT(SAVEUI_Y + SAVEUI_HEIGHT < HOTBAR_Y0, 1);
}

/* The message box for text: its columns, and the box against the button
 * and the hotbar. */
static void msg_box(const char *text, int *x0, int *x1)
{
    int tw = font_text_width(text, TEXT_SCALE);

    *x0 = (BW - tw) / 2 - SAVEUI_MSG_PAD;
    *x1 = *x0 + tw + 2 * SAVEUI_MSG_PAD;
}

/* Draw msg over a grass-green screen and compare with what it should be:
 * the green, the button with "SAVE" centred in white, and the message box
 * with its words. Returns the pixels that differ. */
static int draw_wrong(SaveUiMessage msg, int *lit_save, int *lit_msg)
{
    const char *text = msg == SAVEUI_SAVED ? "WORLD SAVED" : "SAVE FAILED";
    int x, y, n, wrong = 0, guard_ok = 1, x0, x1;

    fb_fill_bgr8(s_pic, BW * BH, 80, 150, 60);
    memset(s_pic + BW * BH * 3, 0xA5, GUARD);
    saveui_draw_bgr8(s_pic, BW, BH, msg);
    for (n = 0; n < GUARD; n++)
        guard_ok &= s_pic[BW * BH * 3 + n] == 0xA5;
    CHECK_EQ_INT(guard_ok, 1);

    fb_fill_bgr8(s_ref, BW * BH, 80, 150, 60);
    for (y = SAVEUI_Y; y < SAVEUI_Y + SAVEUI_HEIGHT; y++)
        for (x = SAVEUI_X; x < SAVEUI_X + SAVEUI_WIDTH; x++)
            fb_put_bgr8(s_ref, BW, BH, x, y, SAVEUI_R, SAVEUI_G, SAVEUI_B);
    font_draw_bgr8(s_ref, BW, BH, SAVEUI_X + (SAVEUI_WIDTH - font_text_width("SAVE", TEXT_SCALE)) / 2,
                   SAVEUI_Y + (SAVEUI_HEIGHT - FONT_H * TEXT_SCALE) / 2, "SAVE", TEXT_SCALE, 255, 255,
                   255);
    if (msg != SAVEUI_NONE) {
        msg_box(text, &x0, &x1);
        for (y = SAVEUI_MSG_Y; y < SAVEUI_MSG_Y + SAVEUI_MSG_H; y++)
            for (x = x0; x < x1; x++)
                fb_put_bgr8(s_ref, BW, BH, x, y, SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B);
        if (msg == SAVEUI_SAVED)
            font_draw_bgr8(s_ref, BW, BH, x0 + SAVEUI_MSG_PAD, SAVEUI_MSG_Y + 8, text, TEXT_SCALE,
                           255, 255, 255);
        else
            font_draw_bgr8(s_ref, BW, BH, x0 + SAVEUI_MSG_PAD, SAVEUI_MSG_Y + 8, text, TEXT_SCALE,
                           SAVEUI_FAIL_R, SAVEUI_FAIL_G, SAVEUI_FAIL_B);
    }
    *lit_save = *lit_msg = 0;
    for (y = 0; y < BH; y++)
        for (x = 0; x < BW; x++) {
            const unsigned char *p = at(s_pic, x, y);

            wrong += memcmp(p, at(s_ref, x, y), 3) != 0;
            if (y < SAVEUI_MSG_Y)
                *lit_save += is_rgb(p, 255, 255, 255);
            else if (msg == SAVEUI_SAVED)
                *lit_msg += is_rgb(p, 255, 255, 255);
            else
                *lit_msg += is_rgb(p, SAVEUI_FAIL_R, SAVEUI_FAIL_G, SAVEUI_FAIL_B);
        }
    return wrong;
}

static void test_draw(void)
{
    int lit_save, lit_msg, x0, x1;
    static unsigned char saved[BW * BH * 3];

    CHECK_EQ_INT(draw_wrong(SAVEUI_NONE, &lit_save, &lit_msg), 0);
    CHECK_EQ_INT(lit_save > 40, 1);
    CHECK_EQ_INT(lit_msg, 0); /* no message */
    CHECK_EQ_INT(draw_wrong(SAVEUI_SAVED, &lit_save, &lit_msg), 0);
    CHECK_EQ_INT(lit_save > 40 && lit_msg > 100, 1);
    memcpy(saved, s_pic, sizeof saved);
    CHECK_EQ_INT(draw_wrong(SAVEUI_FAILED, &lit_save, &lit_msg), 0);
    CHECK_EQ_INT(lit_save > 40 && lit_msg > 100, 1);
    CHECK_EQ_INT(memcmp(saved, s_pic, sizeof saved) != 0, 1);
    /* The message box: on the screen, under the button, over the hotbar's
     * top, words in its middle rows. */
    msg_box("WORLD SAVED", &x0, &x1);
    CHECK_EQ_INT(x0 >= 0 && x1 <= BW, 1);
    msg_box("SAVE FAILED", &x0, &x1);
    CHECK_EQ_INT(x0 >= 0 && x1 <= BW, 1);
    CHECK_EQ_INT(SAVEUI_MSG_Y > SAVEUI_Y + SAVEUI_HEIGHT && SAVEUI_MSG_Y + SAVEUI_MSG_H < HOTBAR_Y0, 1);
    CHECK_EQ_INT(8 + FONT_H * TEXT_SCALE <= SAVEUI_MSG_H - 4, 1);
    CHECK_EQ_INT(SAVEUI_MSG_MS == 2000, 1);
}

void test_saveui(void)
{
    test_hit();
    test_draw();
}
