#include "framebuffer.h"

void fb_fill_bgr8(unsigned char *buf, int pixels,
                  unsigned char r, unsigned char g, unsigned char b)
{
    int i;
    for (i = 0; i < pixels; i++) {
        buf[i * 3 + 0] = b;
        buf[i * 3 + 1] = g;
        buf[i * 3 + 2] = r;
    }
}

void fb_put_bgr8(unsigned char *buf, int w, int h, int x, int y,
                 unsigned char r, unsigned char g, unsigned char b)
{
    int i;

    if (x < 0 || x >= w || y < 0 || y >= h)
        return;
    i = (x * h + (h - 1 - y)) * 3;
    buf[i + 0] = b;
    buf[i + 1] = g;
    buf[i + 2] = r;
}

/* Fill the box from (x0, y0) to (x1, y1), both corners included. */
static void box(unsigned char *buf, int w, int h, int x0, int y0, int x1, int y1, unsigned char v)
{
    int x, y;

    for (x = x0; x <= x1; x++)
        for (y = y0; y <= y1; y++)
            fb_put_bgr8(buf, w, h, x, y, v, v, v);
}

void fb_crosshair_bgr8(unsigned char *buf, int w, int h)
{
    int cx = w / 2, cy = h / 2, half = FB_CROSS_LEN / 2;

    /* Black first, one pixel bigger all round, then the white plus on top. */
    box(buf, w, h, cx - half - 1, cy - 2, cx + half, cy + 1, 0);
    box(buf, w, h, cx - 2, cy - half - 1, cx + 1, cy + half, 0);
    box(buf, w, h, cx - half, cy - 1, cx + half - 1, cy, 255);
    box(buf, w, h, cx - 1, cy - half, cx, cy + half - 1, 255);
}
