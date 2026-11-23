#include "settings.h"

#include <string.h>

#include "font.h"
#include "framebuffer.h"

#define TEXT_SCALE 2
#define TITLE_SCALE 3
#define MARK_GREY 120 /* the track and the stop marks */
#define NUMBER_GREY 160
#define HINT_GREY 150

/* The view distances are written as one digit (snprintf() would pull about
 * 50 KB of the C library's printf into the game for this). */
#if STREAM_VIEW_MIN < 0 || STREAM_VIEW_MAX > 9
#error "settings.c writes a view distance as one digit"
#endif

void settings_init(Settings *s)
{
    s->open = false;
    s->view = STREAM_VIEW_DEFAULT;
}

int settings_stop_x(int v)
{
    return SETTINGS_STOP_X0 + (v - STREAM_VIEW_MIN) * SETTINGS_STOP_STEP;
}

int settings_view_at(int x)
{
    int from = x - SETTINGS_STOP_X0 + SETTINGS_STOP_STEP / 2; /* half a step on: rounds to nearest */

    if (from < 0)
        return STREAM_VIEW_MIN;
    return stream_view(STREAM_VIEW_MIN + from / SETTINGS_STOP_STEP);
}

static bool set_view(Settings *s, int v)
{
    v = stream_view(v);
    if (v == s->view)
        return false;
    s->view = v;
    return true;
}

bool settings_touch(Settings *s, int x, int y)
{
    if (y < SETTINGS_TOUCH_Y0 || y >= SETTINGS_TOUCH_Y1)
        return false;
    return set_view(s, settings_view_at(x));
}

bool settings_step(Settings *s, int by)
{
    return set_view(s, s->view + by);
}

static void rect(unsigned char *buf, int w, int h, int x0, int y0, int rw, int rh,
                 unsigned char r, unsigned char g, unsigned char b)
{
    int x, y;

    for (y = y0; y < y0 + rh; y++)
        for (x = x0; x < x0 + rw; x++)
            fb_put_bgr8(buf, w, h, x, y, r, g, b);
}

/* View distance v (0..9) as its digit, then the words after ("" for none),
 * into text, which holds 16 characters. */
static void view_text(char *text, int v, const char *after)
{
    text[0] = (char)('0' + v);
    text[1] = '\0';
    strncat(text, after, 14);
}

/* text with its middle column at x (whole screen width: x = w / 2). */
static void centred(unsigned char *buf, int w, int h, int x, int y, const char *text, int scale,
                    unsigned char r, unsigned char g, unsigned char b)
{
    font_draw_bgr8(buf, w, h, x - font_text_width(text, scale) / 2, y, text, scale, r, g, b);
}

void settings_draw_bgr8(unsigned char *buf, int w, int h, const Settings *s)
{
    char text[16];
    int v;

    fb_fill_bgr8(buf, w * h, SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B);
    centred(buf, w, h, w / 2, SETTINGS_TITLE_Y, "SETTINGS", TITLE_SCALE, 255, 255, 255);
    centred(buf, w, h, w / 2, SETTINGS_LABEL_Y, "VIEW DISTANCE", TEXT_SCALE, 200, 200, 200);

    /* The track from the first stop to the last, a mark 2 wide and 16 tall
     * at each stop, then the knob over the chosen one. */
    rect(buf, w, h, settings_stop_x(STREAM_VIEW_MIN), SETTINGS_TRACK_Y,
         settings_stop_x(STREAM_VIEW_MAX) - settings_stop_x(STREAM_VIEW_MIN) + 1, 4,
         MARK_GREY, MARK_GREY, MARK_GREY);
    for (v = STREAM_VIEW_MIN; v <= STREAM_VIEW_MAX; v++) {
        rect(buf, w, h, settings_stop_x(v) - 1, SETTINGS_TRACK_Y - 6, 2, 16,
             MARK_GREY, MARK_GREY, MARK_GREY);
        view_text(text, v, "");
        if (v == s->view)
            centred(buf, w, h, settings_stop_x(v), SETTINGS_NUMBER_Y, text, TEXT_SCALE,
                    SETTINGS_GOLD_R, SETTINGS_GOLD_G, SETTINGS_GOLD_B);
        else
            centred(buf, w, h, settings_stop_x(v), SETTINGS_NUMBER_Y, text, TEXT_SCALE,
                    NUMBER_GREY, NUMBER_GREY, NUMBER_GREY);
    }
    rect(buf, w, h, settings_stop_x(s->view) - SETTINGS_KNOB_W / 2, SETTINGS_KNOB_Y,
         SETTINGS_KNOB_W, SETTINGS_KNOB_H, SETTINGS_GOLD_R, SETTINGS_GOLD_G, SETTINGS_GOLD_B);

    view_text(text, s->view, " CHUNKS");
    centred(buf, w, h, w / 2, SETTINGS_VALUE_Y, text, TEXT_SCALE, 255, 255, 255);
    centred(buf, w, h, w / 2, SETTINGS_HINT_Y, "SELECT TO CLOSE", TEXT_SCALE,
            HINT_GREY, HINT_GREY, HINT_GREY);
}
