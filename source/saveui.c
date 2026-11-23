#include "saveui.h"

#include "font.h"
#include "framebuffer.h"
#include "settings.h"

#define TEXT_SCALE 2

static void rect(unsigned char *buf, int w, int h, int x0, int y0, int rw, int rh,
                 unsigned char r, unsigned char g, unsigned char b)
{
    int x, y;

    for (y = y0; y < y0 + rh; y++)
        for (x = x0; x < x0 + rw; x++)
            fb_put_bgr8(buf, w, h, x, y, r, g, b);
}

bool saveui_hit(int x, int y)
{
    return x >= SAVEUI_X && x < SAVEUI_X + SAVEUI_WIDTH && y >= SAVEUI_Y && y < SAVEUI_Y + SAVEUI_HEIGHT;
}

void saveui_draw_bgr8(unsigned char *buf, int w, int h, SaveUiMessage msg)
{
    const char *text;
    int tw;

    rect(buf, w, h, SAVEUI_X, SAVEUI_Y, SAVEUI_WIDTH, SAVEUI_HEIGHT, SAVEUI_R, SAVEUI_G, SAVEUI_B);
    font_draw_bgr8(buf, w, h, SAVEUI_X + (SAVEUI_WIDTH - font_text_width("SAVE", TEXT_SCALE)) / 2,
                   SAVEUI_Y + (SAVEUI_HEIGHT - FONT_H * TEXT_SCALE) / 2, "SAVE", TEXT_SCALE, 255, 255,
                   255);
    if (msg == SAVEUI_NONE)
        return;
    text = msg == SAVEUI_SAVED ? "WORLD SAVED" : "SAVE FAILED";
    tw = font_text_width(text, TEXT_SCALE);
    rect(buf, w, h, (w - tw) / 2 - SAVEUI_MSG_PAD, SAVEUI_MSG_Y, tw + 2 * SAVEUI_MSG_PAD,
         SAVEUI_MSG_H, SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B);
    if (msg == SAVEUI_SAVED)
        font_draw_bgr8(buf, w, h, (w - tw) / 2, SAVEUI_MSG_Y + 8, text, TEXT_SCALE, 255, 255, 255);
    else
        font_draw_bgr8(buf, w, h, (w - tw) / 2, SAVEUI_MSG_Y + 8, text, TEXT_SCALE, SAVEUI_FAIL_R,
                       SAVEUI_FAIL_G, SAVEUI_FAIL_B);
}
