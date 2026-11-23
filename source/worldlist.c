#include "worldlist.h"

#include <stdio.h>

#include "font.h"
#include "framebuffer.h"
#include "newworld.h"
#include "settings.h"

#define TITLE_SCALE 3
#define TEXT_SCALE 2
#define LABEL_GREY 200

static int clamp(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

/* sel held to the list, then the rows moved the least that shows it,
 * never leaving empty rows at the foot while worlds are above the top. */
static void settle(WorldList *l)
{
    int last_top = l->count > WORLDLIST_ROWS ? l->count - WORLDLIST_ROWS : 0;

    l->sel = l->count > 0 ? clamp(l->sel, 0, l->count - 1) : 0;
    if (l->sel < l->top)
        l->top = l->sel;
    if (l->sel >= l->top + WORLDLIST_ROWS)
        l->top = l->sel - WORLDLIST_ROWS + 1;
    l->top = clamp(l->top, 0, last_top);
}

void worldlist_init(WorldList *l, int count, int sel)
{
    l->count = count > 0 ? count : 0;
    l->sel = sel;
    l->top = 0;
    settle(l);
}

void worldlist_move(WorldList *l, int by)
{
    worldlist_choose(l, l->sel + by);
}

void worldlist_choose(WorldList *l, int i)
{
    l->sel = i;
    settle(l);
}

void worldlist_remove(WorldList *l)
{
    if (l->count > 0)
        l->count--;
    settle(l);
}

static bool inside(int x, int y, int x0, int y0, int w, int h)
{
    return x >= x0 && x < x0 + w && y >= y0 && y < y0 + h;
}

int worldlist_row_at(const WorldList *l, int x, int y)
{
    int i;

    for (i = 0; i < WORLDLIST_ROWS && l->top + i < l->count; i++)
        if (inside(x, y, WORLDLIST_ROW_X, WORLDLIST_ROW_Y + i * WORLDLIST_ROW_STEP,
                   WORLDLIST_ROW_W, WORLDLIST_ROW_H))
            return l->top + i;
    return -1;
}

static bool on_button(int x, int y, int left, int top)
{
    return inside(x, y, left, top, WORLDLIST_BUTTON_W, WORLDLIST_BUTTON_H);
}

WorldListButton worldlist_button_at(int x, int y)
{
    if (on_button(x, y, WORLDLIST_LEFT_X, WORLDLIST_TOP_Y))
        return WORLDLIST_OPEN;
    if (on_button(x, y, WORLDLIST_RIGHT_X, WORLDLIST_TOP_Y))
        return WORLDLIST_NEW;
    if (on_button(x, y, WORLDLIST_LEFT_X, WORLDLIST_LOW_Y))
        return WORLDLIST_RENAME;
    if (on_button(x, y, WORLDLIST_RIGHT_X, WORLDLIST_LOW_Y))
        return WORLDLIST_DELETE;
    return WORLDLIST_NONE;
}

WorldListAsk worldlist_ask_at(int x, int y)
{
    if (on_button(x, y, WORLDLIST_LEFT_X, WORLDLIST_TOP_Y))
        return WORLDLIST_ASK_DELETE;
    if (on_button(x, y, WORLDLIST_RIGHT_X, WORLDLIST_TOP_Y))
        return WORLDLIST_ASK_KEEP;
    return WORLDLIST_ASK_NONE;
}

static void rect(unsigned char *buf, int w, int h, int x0, int y0, int rw, int rh,
                 unsigned char r, unsigned char g, unsigned char b)
{
    int x, y;

    for (y = y0; y < y0 + rh; y++)
        for (x = x0; x < x0 + rw; x++)
            fb_put_bgr8(buf, w, h, x, y, r, g, b);
}

/* text with its middle column at x. */
static void centred(unsigned char *buf, int w, int h, int x, int y, const char *text, int scale,
                    unsigned char r, unsigned char g, unsigned char b)
{
    font_draw_bgr8(buf, w, h, x - font_text_width(text, scale) / 2, y, text, scale, r, g, b);
}

/* A button at (left, top), its words in white in the middle of it. */
static void button(unsigned char *buf, int w, int h, int left, int top, const char *text,
                   unsigned char r, unsigned char g, unsigned char b)
{
    rect(buf, w, h, left, top, WORLDLIST_BUTTON_W, WORLDLIST_BUTTON_H, r, g, b);
    centred(buf, w, h, left + WORLDLIST_BUTTON_W / 2,
            top + (WORLDLIST_BUTTON_H - FONT_H * TEXT_SCALE) / 2, text, TEXT_SCALE, 255, 255, 255);
}

static void grey_button(unsigned char *buf, int w, int h, int left, int top, const char *text)
{
    button(buf, w, h, left, top, text, NEWWORLD_BUTTON_R, NEWWORLD_BUTTON_G, NEWWORLD_BUTTON_B);
}

void worldlist_draw_bgr8(unsigned char *buf, int w, int h, const WorldList *l,
                         const char names[WORLDLIST_ROWS][WORLDNAME_SIZE])
{
    char count[32];
    int i;

    fb_fill_bgr8(buf, w * h, SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B);
    font_draw_bgr8(buf, w, h, WORLDLIST_TITLE_X, WORLDLIST_TITLE_Y, "WORLDS", TEXT_SCALE, 255,
                   255, 255);
    snprintf(count, sizeof count, "%d OF %d", l->count > 0 ? l->sel + 1 : 0, l->count);
    font_draw_bgr8(buf, w, h, WORLDLIST_COUNT_RIGHT - font_text_width(count, TEXT_SCALE),
                   WORLDLIST_TITLE_Y, count, TEXT_SCALE, LABEL_GREY, LABEL_GREY, LABEL_GREY);
    for (i = 0; i < WORLDLIST_ROWS && l->top + i < l->count; i++) {
        int y = WORLDLIST_ROW_Y + i * WORLDLIST_ROW_STEP;
        bool chosen = l->top + i == l->sel;

        rect(buf, w, h, WORLDLIST_ROW_X, y, WORLDLIST_ROW_W, WORLDLIST_ROW_H,
             chosen ? NEWWORLD_PLAY_R : NEWWORLD_BUTTON_R,
             chosen ? NEWWORLD_PLAY_G : NEWWORLD_BUTTON_G,
             chosen ? NEWWORLD_PLAY_B : NEWWORLD_BUTTON_B);
        font_draw_bgr8(buf, w, h, WORLDLIST_ROW_X + WORLDLIST_NAME_INSET,
                       y + (WORLDLIST_ROW_H - FONT_H * TEXT_SCALE) / 2, names[i], TEXT_SCALE,
                       255, 255, 255);
    }
    button(buf, w, h, WORLDLIST_LEFT_X, WORLDLIST_TOP_Y, "A  OPEN", NEWWORLD_PLAY_R,
           NEWWORLD_PLAY_G, NEWWORLD_PLAY_B);
    grey_button(buf, w, h, WORLDLIST_RIGHT_X, WORLDLIST_TOP_Y, "X  NEW");
    grey_button(buf, w, h, WORLDLIST_LEFT_X, WORLDLIST_LOW_Y, "Y  RENAME");
    grey_button(buf, w, h, WORLDLIST_RIGHT_X, WORLDLIST_LOW_Y, "L  DELETE");
}

void worldlist_ask_draw_bgr8(unsigned char *buf, int w, int h, const char *name)
{
    fb_fill_bgr8(buf, w * h, SETTINGS_BG_R, SETTINGS_BG_G, SETTINGS_BG_B);
    centred(buf, w, h, w / 2, WORLDLIST_ASK_TITLE_Y, "DELETE WORLD", TITLE_SCALE, 255, 255, 255);
    centred(buf, w, h, w / 2, WORLDLIST_ASK_NAME_Y, name, TEXT_SCALE, SETTINGS_GOLD_R,
            SETTINGS_GOLD_G, SETTINGS_GOLD_B);
    centred(buf, w, h, w / 2, WORLDLIST_ASK_WARN_Y, "IT CANNOT BE UNDONE", TEXT_SCALE, LABEL_GREY,
            LABEL_GREY, LABEL_GREY);
    button(buf, w, h, WORLDLIST_LEFT_X, WORLDLIST_TOP_Y, "A  DELETE", WORLDLIST_RED_R,
           WORLDLIST_RED_G, WORLDLIST_RED_B);
    grey_button(buf, w, h, WORLDLIST_RIGHT_X, WORLDLIST_TOP_Y, "B  KEEP");
}
