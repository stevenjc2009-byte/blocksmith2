#include "framebuffer.h"
#include "test.h"

/* The top screen as the 3DS stores it: 400 x 240 seen, kept a column at a
 * time (row 1-08). */
#define TW 400
#define TH 240
static unsigned char s_top[TW * TH * 3];

/* Pixel seen at (x, y) from the top-left: the grey level if all three bytes
 * match, else -1. */
static int seen(int x, int y)
{
    int i = (x * TH + (TH - 1 - y)) * 3;

    if (s_top[i] != s_top[i + 1] || s_top[i] != s_top[i + 2])
        return -1;
    return s_top[i];
}

static void test_put_and_crosshair(void)
{
    int x, y, white = 0, black = 0, other = 0, lone = 0, sx = 0, sy = 0;
    int minx = TW, maxx = -1, miny = TH, maxy = -1;

    /* Corners land where the hardware shows them: the top-left pixel seen
     * is the last of the first column in memory. */
    fb_fill_bgr8(s_top, TW * TH, 0, 0, 0);
    fb_put_bgr8(s_top, TW, TH, 0, 0, 1, 2, 3);
    CHECK_EQ_INT(s_top[(TH - 1) * 3 + 0], 3);
    CHECK_EQ_INT(s_top[(TH - 1) * 3 + 1], 2);
    CHECK_EQ_INT(s_top[(TH - 1) * 3 + 2], 1);
    fb_put_bgr8(s_top, TW, TH, 0, TH - 1, 9, 9, 9); /* bottom-left: first byte */
    CHECK_EQ_INT(s_top[0], 9);
    fb_put_bgr8(s_top, TW, TH, TW - 1, 0, 7, 7, 7); /* top-right: last byte */
    CHECK_EQ_INT(s_top[TW * TH * 3 - 1], 7);
    fb_put_bgr8(s_top, TW, TH, 1, 0, 5, 5, 5);       /* one right: one column on */
    CHECK_EQ_INT(s_top[(TH + TH - 1) * 3], 5);
    /* Off the screen: nothing written anywhere. */
    fb_fill_bgr8(s_top, TW * TH, 0, 0, 0);
    fb_put_bgr8(s_top, TW, TH, -1, 5, 255, 255, 255);
    fb_put_bgr8(s_top, TW, TH, TW, 5, 255, 255, 255);
    fb_put_bgr8(s_top, TW, TH, 5, -1, 255, 255, 255);
    fb_put_bgr8(s_top, TW, TH, 5, TH, 255, 255, 255);
    for (x = 0; x < TW * TH * 3; x++)
        other += s_top[x] != 0;
    CHECK_EQ_INT(other, 0);

    /* The crosshair on grey ground. */
    fb_fill_bgr8(s_top, TW * TH, 128, 128, 128);
    fb_crosshair_bgr8(s_top, TW, TH);
    other = 0;
    for (x = 0; x < TW; x++)
        for (y = 0; y < TH; y++) {
            int v = seen(x, y);

            if (v == 255) {
                white++;
                sx += x;
                sy += y;
                if (x < minx) minx = x;
                if (x > maxx) maxx = x;
                if (y < miny) miny = y;
                if (y > maxy) maxy = y;
            } else if (v == 0) {
                int dx, dy, near = 0;

                black++;
                for (dx = -1; dx <= 1; dx++)
                    for (dy = -1; dy <= 1; dy++)
                        if (x + dx >= 0 && x + dx < TW && y + dy >= 0 && y + dy < TH &&
                            seen(x + dx, y + dy) == 255)
                            near = 1;
                lone += !near;
            } else if (v != 128) {
                other++;
            }
        }
    /* Two bars 14 x 2 crossing in a 2 x 2 middle; a 1-pixel black edge. */
    CHECK_EQ_INT(white, 2 * FB_CROSS_LEN * 2 - 4);
    CHECK_EQ_INT(black, 2 * (FB_CROSS_LEN + 2) * 4 - 16 - white);
    CHECK_EQ_INT(lone, 0);   /* every black pixel edges the white */
    CHECK_EQ_INT(other, 0);
    /* Centred on the middle of the screen, where the camera points:
     * (199.5, 119.5), so the sums are exactly 199.5 and 119.5 x white. */
    CHECK_EQ_INT(sx * 2, 399 * white);
    CHECK_EQ_INT(sy * 2, 239 * white);
    CHECK_EQ_INT(minx, 200 - FB_CROSS_LEN / 2);
    CHECK_EQ_INT(maxx, 199 + FB_CROSS_LEN / 2);
    CHECK_EQ_INT(miny, 120 - FB_CROSS_LEN / 2);
    CHECK_EQ_INT(maxy, 119 + FB_CROSS_LEN / 2);
}

void test_framebuffer(void)
{
    /* Four pixels plus three guard bytes that must stay untouched. */
    unsigned char buf[4 * 3 + 3];
    int i;

    for (i = 0; i < (int)sizeof buf; i++)
        buf[i] = 0xEE;

    fb_fill_bgr8(buf, 4, 10, 20, 30);

    for (i = 0; i < 4; i++) {
        CHECK_EQ_INT(buf[i * 3 + 0], 30); /* blue first */
        CHECK_EQ_INT(buf[i * 3 + 1], 20); /* then green */
        CHECK_EQ_INT(buf[i * 3 + 2], 10); /* red last */
    }
    for (i = 12; i < 15; i++)
        CHECK_EQ_INT(buf[i], 0xEE); /* nothing written past the end */

    fb_fill_bgr8(buf, 0, 1, 2, 3); /* zero pixels writes nothing */
    CHECK_EQ_INT(buf[0], 30);

    test_put_and_crosshair();
}
