#include "font.h"

#include "framebuffer.h"

/* Each letter is FONT_H rows of FONT_W bits, the leftmost pixel the
 * highest bit (0x10). */
static const struct {
    char c;
    unsigned char row[FONT_H];
} s_glyphs[] = {
    { '0', { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E } },
    { '1', { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E } },
    { '2', { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F } },
    { '3', { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E } },
    { '4', { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 } },
    { '5', { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E } },
    { '6', { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E } },
    { '7', { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } },
    { '8', { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E } },
    { '9', { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C } },
    { '-', { 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00 } },
    { 'F', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 } },
    { 'P', { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 } },
    { 'S', { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } },
};

static const unsigned char *glyph(char c)
{
    unsigned i;

    for (i = 0; i < sizeof s_glyphs / sizeof s_glyphs[0]; i++)
        if (s_glyphs[i].c == c)
            return s_glyphs[i].row;
    return 0; /* ' ' and anything else: nothing to draw */
}

int font_text_width(const char *text, int scale)
{
    int n = 0;

    while (text[n])
        n++;
    return n ? (n * (FONT_W + 1) - 1) * scale : 0;
}

void font_draw_bgr8(unsigned char *buf, int w, int h, int x, int y, const char *text, int scale,
                    unsigned char r, unsigned char g, unsigned char b)
{
    int i, fx, fy, sx, sy;

    for (i = 0; text[i]; i++) {
        const unsigned char *row = glyph(text[i]);
        int left = x + i * (FONT_W + 1) * scale;

        if (!row)
            continue;
        for (fy = 0; fy < FONT_H; fy++)
            for (fx = 0; fx < FONT_W; fx++) {
                if (!(row[fy] & (0x10 >> fx)))
                    continue;
                for (sy = 0; sy < scale; sy++)
                    for (sx = 0; sx < scale; sx++)
                        fb_put_bgr8(buf, w, h, left + fx * scale + sx, y + fy * scale + sy, r, g, b);
            }
    }
}
