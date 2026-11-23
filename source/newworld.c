#include "newworld.h"

#include "font.h"
#include "framebuffer.h"
#include "seed.h"

#define TITLE_SCALE 3
#define TEXT_SCALE 2
#define LABEL_GREY 200

static int on_button(int x, int y, int top)
{
    return x >= NEWWORLD_BUTTON_X && x < NEWWORLD_BUTTON_X + NEWWORLD_BUTTON_W && y >= top &&
           y < top + NEWWORLD_BUTTON_H;
}

static int on_back(int x, int y)
{
    return x >= NEWWORLD_BACK_X && x < NEWWORLD_BACK_X + NEWWORLD_BACK_W && y >= NEWWORLD_BACK_Y &&
           y < NEWWORLD_BACK_Y + NEWWORLD_BACK_H;
}

NewWorldButton newworld_button_at(int x, int y, bool back)
{
    if (back && on_back(x, y))
        return NEWWORLD_BACK;
    if (on_button(x, y, NEWWORLD_RANDOM_Y))
        return NEWWORLD_RANDOM;
    if (on_button(x, y, NEWWORLD_TYPE_Y))
        return NEWWORLD_TYPE;
    if (on_button(x, y, NEWWORLD_PLAY_Y))
        return NEWWORLD_PLAY;
    return NEWWORLD_NONE;
}

static void rect(unsigned char *buf, int w, int h, int x0, int y0, int rw, int rh,
                 unsigned char r, unsigned char g, unsigned char b)
{
    int x, y;

    for (y = y0; y < y0 + rh; y++)
        for (x = x0; x < x0 + rw; x++)
            fb_put_bgr8(buf, w, h, x, y, r, g, b);
}

/* text with its middle column at x (whole screen width: x = w / 2). */
static void centred(unsigned char *buf, int w, int h, int x, int y, const char *text, int scale,
                    unsigned char r, unsigned char g, unsigned char b)
{
    font_draw_bgr8(buf, w, h, x - font_text_width(text, scale) / 2, y, text, scale, r, g, b);
}

/* A box at (x0, y0), its words in white in the middle of it. */
static void box(unsigned char *buf, int w, int h, int x0, int y0, int bw, int bh, const char *text,
                unsigned char r, unsigned char g, unsigned char b)
{
    rect(buf, w, h, x0, y0, bw, bh, r, g, b);
    centred(buf, w, h, x0 + bw / 2, y0 + (bh - FONT_H * TEXT_SCALE) / 2, text, TEXT_SCALE, 255,
            255, 255);
}

/* A button from row top, its words in white in the middle of it. */
static void button(unsigned char *buf, int w, int h, int top, const char *text,
                   unsigned char r, unsigned char g, unsigned char b)
{
    box(buf, w, h, NEWWORLD_BUTTON_X, top, NEWWORLD_BUTTON_W, NEWWORLD_BUTTON_H, text, r, g, b);
}

void newworld_draw_bgr8(unsigned char *buf, int w, int h, uint32_t seed, bool back)
{
    char text[SEED_TEXT_SIZE];

    fb_fill_bgr8(buf, w * h, NEWWORLD_BG_R, NEWWORLD_BG_G, NEWWORLD_BG_B);
    if (back)
        box(buf, w, h, NEWWORLD_BACK_X, NEWWORLD_BACK_Y, NEWWORLD_BACK_W, NEWWORLD_BACK_H,
            "B  BACK", NEWWORLD_BUTTON_R, NEWWORLD_BUTTON_G, NEWWORLD_BUTTON_B);
    centred(buf, w, h, w / 2, NEWWORLD_TITLE_Y, "NEW WORLD", TITLE_SCALE, 255, 255, 255);
    centred(buf, w, h, w / 2, NEWWORLD_LABEL_Y, "SEED", TEXT_SCALE, LABEL_GREY, LABEL_GREY,
            LABEL_GREY);
    seed_to_text(seed, text);
    centred(buf, w, h, w / 2, NEWWORLD_SEED_Y, text, TEXT_SCALE, SETTINGS_GOLD_R, SETTINGS_GOLD_G,
            SETTINGS_GOLD_B);
    button(buf, w, h, NEWWORLD_RANDOM_Y, "X  RANDOM SEED", NEWWORLD_BUTTON_R, NEWWORLD_BUTTON_G,
           NEWWORLD_BUTTON_B);
    button(buf, w, h, NEWWORLD_TYPE_Y, "Y  TYPE A SEED", NEWWORLD_BUTTON_R, NEWWORLD_BUTTON_G,
           NEWWORLD_BUTTON_B);
    button(buf, w, h, NEWWORLD_PLAY_Y, "A  PLAY", NEWWORLD_PLAY_R, NEWWORLD_PLAY_G,
           NEWWORLD_PLAY_B);
}
