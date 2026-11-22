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
