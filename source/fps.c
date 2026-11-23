#include "fps.h"

#include "font.h"
#include "framebuffer.h"

bool fps_pace_done(uint64_t last, uint64_t now)
{
    return now - last >= (uint64_t)FPS_REFRESH_TICKS * 3 / 2;
}

void fps_init(FpsCounter *c, uint64_t now)
{
    c->start = now;
    c->frames = 0;
    c->shown = 0;
}

void fps_frame(FpsCounter *c, uint64_t now)
{
    uint64_t span = now - c->start;

    c->frames++;
    if (span < FPS_TICKS_PER_SEC)
        return;
    c->shown = (int)(((uint64_t)c->frames * FPS_TICKS_PER_SEC + span / 2) / span);
    c->start = now;
    c->frames = 0;
}

void fps_text(int shown, char out[FPS_TEXT_LEN])
{
    const char *tail = " FPS";
    int i;

    if (shown > 99)
        shown = 99;
    if (shown <= 0) {
        out[0] = '-';
        out[1] = '-';
    } else {
        out[0] = shown >= 10 ? (char)('0' + shown / 10) : ' ';
        out[1] = (char)('0' + shown % 10);
    }
    for (i = 0; tail[i]; i++)
        out[2 + i] = tail[i];
    out[2 + i] = '\0';
}

void fps_draw_bgr8(unsigned char *buf, int w, int h, int shown)
{
    char text[FPS_TEXT_LEN];
    int bw, bh, x, y;

    fps_text(shown, text);
    bw = font_text_width(text, FPS_SCALE) + 2 * FPS_PAD;
    bh = FONT_H * FPS_SCALE + 2 * FPS_PAD;
    for (x = 0; x < bw; x++)
        for (y = 0; y < bh; y++)
            fb_put_bgr8(buf, w, h, FPS_BOX_X + x, FPS_BOX_Y + y, FPS_BOX_GREY, FPS_BOX_GREY,
                        FPS_BOX_GREY);
    font_draw_bgr8(buf, w, h, FPS_BOX_X + FPS_PAD, FPS_BOX_Y + FPS_PAD, text, FPS_SCALE, 255, 255,
                   255);
}
