#include <string.h>

#include "font.h"
#include "framebuffer.h"
#include "test.h"

/* Rows 1-11 and 2-02: the small font the frame counter and the settings
 * screen are written in. The letters are written out again here as pictures, apart from the font's own bit
 * table, so a wrong bit in either one shows. */

#define SW 64
#define SH 40
#define GUARD 64

static const struct {
    char c;
    const char *row[FONT_H];
} s_want[] = {
    { '0', { ".XXX.", "X...X", "X..XX", "X.X.X", "XX..X", "X...X", ".XXX." } },
    { '1', { "..X..", ".XX..", "..X..", "..X..", "..X..", "..X..", ".XXX." } },
    { '2', { ".XXX.", "X...X", "....X", "...X.", "..X..", ".X...", "XXXXX" } },
    { '3', { "XXXXX", "...X.", "..X..", "...X.", "....X", "X...X", ".XXX." } },
    { '4', { "...X.", "..XX.", ".X.X.", "X..X.", "XXXXX", "...X.", "...X." } },
    { '5', { "XXXXX", "X....", "XXXX.", "....X", "....X", "X...X", ".XXX." } },
    { '6', { "..XX.", ".X...", "X....", "XXXX.", "X...X", "X...X", ".XXX." } },
    { '7', { "XXXXX", "....X", "...X.", "..X..", ".X...", ".X...", ".X..." } },
    { '8', { ".XXX.", "X...X", "X...X", ".XXX.", "X...X", "X...X", ".XXX." } },
    { '9', { ".XXX.", "X...X", "X...X", ".XXXX", "....X", "...X.", ".XX.." } },
    { '-', { ".....", ".....", ".....", ".XXX.", ".....", ".....", "....." } },
    { 'A', { ".XXX.", "X...X", "X...X", "XXXXX", "X...X", "X...X", "X...X" } },
    { 'B', { "XXXX.", "X...X", "X...X", "XXXX.", "X...X", "X...X", "XXXX." } },
    { 'C', { ".XXX.", "X...X", "X....", "X....", "X....", "X...X", ".XXX." } },
    { 'D', { "XXX..", "X..X.", "X...X", "X...X", "X...X", "X..X.", "XXX.." } },
    { 'E', { "XXXXX", "X....", "X....", "XXXX.", "X....", "X....", "XXXXX" } },
    { 'F', { "XXXXX", "X....", "X....", "XXXX.", "X....", "X....", "X...." } },
    { 'G', { ".XXX.", "X...X", "X....", "X.XXX", "X...X", "X...X", ".XXXX" } },
    { 'H', { "X...X", "X...X", "X...X", "XXXXX", "X...X", "X...X", "X...X" } },
    { 'I', { ".XXX.", "..X..", "..X..", "..X..", "..X..", "..X..", ".XXX." } },
    { 'J', { "..XXX", "...X.", "...X.", "...X.", "...X.", "X..X.", ".XX.." } },
    { 'K', { "X...X", "X..X.", "X.X..", "XX...", "X.X..", "X..X.", "X...X" } },
    { 'L', { "X....", "X....", "X....", "X....", "X....", "X....", "XXXXX" } },
    { 'M', { "X...X", "XX.XX", "X.X.X", "X.X.X", "X...X", "X...X", "X...X" } },
    { 'N', { "X...X", "X...X", "XX..X", "X.X.X", "X..XX", "X...X", "X...X" } },
    { 'O', { ".XXX.", "X...X", "X...X", "X...X", "X...X", "X...X", ".XXX." } },
    { 'P', { "XXXX.", "X...X", "X...X", "XXXX.", "X....", "X....", "X...." } },
    { 'Q', { ".XXX.", "X...X", "X...X", "X...X", "X.X.X", "X..X.", ".XX.X" } },
    { 'R', { "XXXX.", "X...X", "X...X", "XXXX.", "X.X..", "X..X.", "X...X" } },
    { 'S', { ".XXXX", "X....", "X....", ".XXX.", "....X", "....X", "XXXX." } },
    { 'T', { "XXXXX", "..X..", "..X..", "..X..", "..X..", "..X..", "..X.." } },
    { 'U', { "X...X", "X...X", "X...X", "X...X", "X...X", "X...X", ".XXX." } },
    { 'V', { "X...X", "X...X", "X...X", "X...X", "X...X", ".X.X.", "..X.." } },
    { 'W', { "X...X", "X...X", "X...X", "X.X.X", "X.X.X", "X.X.X", ".X.X." } },
    { 'X', { "X...X", "X...X", ".X.X.", "..X..", ".X.X.", "X...X", "X...X" } },
    { 'Y', { "X...X", "X...X", ".X.X.", "..X..", "..X..", "..X..", "..X.." } },
    { 'Z', { "XXXXX", "....X", "...X.", "..X..", ".X...", "X....", "XXXXX" } },
};

static unsigned char s_buf[SW * SH * 3 + GUARD];

static const char *const *want_rows(char c)
{
    unsigned i;

    for (i = 0; i < sizeof s_want / sizeof s_want[0]; i++)
        if (s_want[i].c == c)
            return s_want[i].row;
    return 0;
}

/* Is screen pixel (px, py) lit when text is drawn at (x, y) and scale s? */
static int want_lit(const char *text, int x, int y, int s, int px, int py)
{
    int cell = (FONT_W + 1) * s, i, fx, fy;
    const char *const *rows;

    if (px < x || py < y || py >= y + FONT_H * s)
        return 0;
    i = (px - x) / cell;
    if (i >= (int)strlen(text))
        return 0;
    fx = (px - x - i * cell) / s;
    fy = (py - y) / s;
    rows = want_rows(text[i]);
    return fx < FONT_W && rows && rows[fy][fx] == 'X';
}

/* Draw text in red on a blue screen and count the pixels that differ from
 * the pictures above; also counts the lit pixels so an empty drawing
 * cannot pass. */
static int draw_and_compare(const char *text, int x, int y, int s, int *lit)
{
    int px, py, wrong = 0;

    fb_fill_bgr8(s_buf, SW * SH, 0, 0, 200);
    font_draw_bgr8(s_buf, SW, SH, x, y, text, s, 255, 0, 0);
    *lit = 0;
    for (px = 0; px < SW; px++)
        for (py = 0; py < SH; py++) {
            const unsigned char *p = &s_buf[(px * SH + (SH - 1 - py)) * 3];
            int on = want_lit(text, x, y, s, px, py);

            if (on)
                (*lit)++;
            if (on ? !(p[0] == 0 && p[1] == 0 && p[2] == 255) : !(p[0] == 200 && p[1] == 0 && p[2] == 0))
                wrong++;
        }
    return wrong;
}

void test_font(void)
{
    unsigned i;
    int lit, s, n, guard_ok;
    char one[2] = { 0, 0 };

    /* Every letter, at its own size and at three times it. */
    for (i = 0; i < sizeof s_want / sizeof s_want[0]; i++)
        for (s = 1; s <= 3; s += 2) {
            one[0] = s_want[i].c;
            CHECK_EQ_INT(draw_and_compare(one, 9, 6, s, &lit), 0);
            CHECK_EQ_INT(lit >= 3 * s * s, 1);
        }

    /* Whole words: letters FONT_W + 1 apart, a space drawing nothing. */
    CHECK_EQ_INT(draw_and_compare("30 FPS", 1, 3, 1, &lit), 0);
    CHECK_EQ_INT(lit > 40, 1);
    CHECK_EQ_INT(draw_and_compare("-- 9", 2, 20, 2, &lit), 0);
    CHECK_EQ_INT(lit > 20, 1);
    CHECK_EQ_INT(draw_and_compare("VIEW 5", 0, 4, 1, &lit), 0);
    CHECK_EQ_INT(lit > 40, 1);
    CHECK_EQ_INT(draw_and_compare("JQXZ", 3, 12, 2, &lit), 0);
    CHECK_EQ_INT(lit > 60, 1);
    /* A character the font lacks draws nothing but keeps its space. */
    CHECK_EQ_INT(draw_and_compare("?1", 4, 4, 2, &lit), 0);
    n = lit;
    CHECK_EQ_INT(draw_and_compare(" 1", 4, 4, 2, &lit), 0);
    CHECK_EQ_INT(lit, n);
    CHECK_EQ_INT(n > 0, 1);

    CHECK_EQ_INT(font_text_width("", 2), 0);
    CHECK_EQ_INT(font_text_width("3", 1), 5);
    CHECK_EQ_INT(font_text_width("3", 2), 10);
    CHECK_EQ_INT(font_text_width("30 FPS", 2), 70);

    /* Off the edges: nothing is written outside the picture. */
    memset(s_buf + SW * SH * 3, 0xA5, GUARD);
    fb_fill_bgr8(s_buf, SW * SH, 0, 0, 200);
    font_draw_bgr8(s_buf, SW, SH, -7, -5, "88", 3, 255, 0, 0);
    font_draw_bgr8(s_buf, SW, SH, SW - 8, SH - 9, "88", 3, 255, 0, 0);
    guard_ok = 1;
    for (n = 0; n < GUARD; n++)
        if (s_buf[SW * SH * 3 + n] != 0xA5)
            guard_ok = 0;
    CHECK_EQ_INT(guard_ok, 1);
    CHECK_EQ_INT(draw_and_compare("88", -7, -5, 3, &lit), 0);
    CHECK_EQ_INT(lit > 0, 1);
    CHECK_EQ_INT(draw_and_compare("88", SW - 8, SH - 9, 3, &lit), 0);
    CHECK_EQ_INT(lit > 0, 1);
}
