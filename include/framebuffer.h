#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

/* Fill a 3DS framebuffer stored as BGR8 (3 bytes per pixel, blue first),
 * the format gfxInitDefault() gives both screens. */
void fb_fill_bgr8(unsigned char *buf, int pixels,
                  unsigned char r, unsigned char g, unsigned char b);

/* The screens are mounted turned a quarter turn, so a framebuffer holds the
 * picture a column at a time: for a screen w pixels wide and h tall, the
 * pixel seen at (x, y) from the top-left is at index x * h + (h - 1 - y).
 * Set that pixel (BGR8); spots off the screen are ignored. */
void fb_put_bgr8(unsigned char *buf, int w, int h, int x, int y,
                 unsigned char r, unsigned char g, unsigned char b);

/* The crosshair (roadmap row 1-08): a white plus 2 pixels thick and
 * FB_CROSS_LEN long each way, with a 1-pixel black edge so it shows on any
 * ground, centred on the middle of the screen (between pixels w/2 - 1 and
 * w/2, h/2 - 1 and h/2, where the camera's centre ray lands). */
#define FB_CROSS_LEN 14
void fb_crosshair_bgr8(unsigned char *buf, int w, int h);

#endif
