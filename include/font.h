#ifndef FONT_H_INCLUDED
#define FONT_H_INCLUDED

/* A small 5 x 7 pixel font for on-screen numbers (roadmap row 1-11's frame
 * counter): the digits, '-', ' ' and the letters F, P and S. Pure C, no 3DS
 * calls, so the PC tests can run it. */

#define FONT_W 5
#define FONT_H 7

/* Width text takes when drawn at this scale: FONT_W + 1 font pixels a
 * letter, less the gap after the last one. */
int font_text_width(const char *text, int scale);

/* Draw text into a BGR8 framebuffer (sizes as for fb_put_bgr8()) with its
 * top-left corner at (x, y), each font pixel a scale x scale square in
 * colour (r, g, b). Only the letters' own pixels are drawn. A character the
 * font lacks draws nothing but still takes its space. */
void font_draw_bgr8(unsigned char *buf, int w, int h, int x, int y, const char *text, int scale,
                    unsigned char r, unsigned char g, unsigned char b);

#endif
