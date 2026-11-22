#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

/* Fill a 3DS framebuffer stored as BGR8 (3 bytes per pixel, blue first),
 * the format gfxInitDefault() gives both screens. */
void fb_fill_bgr8(unsigned char *buf, int pixels,
                  unsigned char r, unsigned char g, unsigned char b);

#endif
