#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>

#include "stream.h"

/* The settings screen (roadmap row 2-02), for now with one slider: the
 * view distance (kickoff W-3, SC-5), how many chunks out from the player's
 * chunk are drawn. SELECT opens and closes it on the bottom screen; while
 * it is open the player stands still but the world keeps being made and
 * drawn, so a change shows on the top screen at once. The choice is not
 * kept when the game quits yet (saving is roadmap row 2-04). Pure C, no 3DS
 * calls, so the PC tests can run it.
 *
 * Bottom-screen points are as the touch screen reports them: x 0..319 from
 * the left, y 0..239 from the top. The view distances run from
 * STREAM_VIEW_MIN to STREAM_VIEW_MAX (stream.h). */

/* The slider: one stop for each view distance, left to right, on a track
 * across the middle of the screen. */
#define SETTINGS_STOP_X0 40   /* the first stop's middle (view STREAM_VIEW_MIN) */
#define SETTINGS_STOP_STEP 80 /* from one stop to the next */
#define SETTINGS_TRACK_Y 112  /* the track's top row; it is 4 rows tall */
/* A touch anywhere in these rows moves the knob to the stop nearest it:
 * the knob's rows and the numbers under the stops. */
#define SETTINGS_TOUCH_Y0 92
#define SETTINGS_TOUCH_Y1 148 /* the first row below */

/* The knob: KNOB_W x KNOB_H, its middle column on the chosen stop. */
#define SETTINGS_KNOB_W 16
#define SETTINGS_KNOB_H 28
#define SETTINGS_KNOB_Y 100

/* Where the words go (each line centred), and the colours. */
#define SETTINGS_TITLE_Y 36   /* "SETTINGS", 3 times the font's size */
#define SETTINGS_LABEL_Y 76   /* "VIEW DISTANCE", twice the size, like the rest */
#define SETTINGS_NUMBER_Y 134 /* the view distance under each stop */
#define SETTINGS_VALUE_Y 160  /* "3 CHUNKS" */
#define SETTINGS_HINT_Y 212   /* "SELECT TO CLOSE" */
#define SETTINGS_BG_R 36
#define SETTINGS_BG_G 40
#define SETTINGS_BG_B 52
#define SETTINGS_GOLD_R 240 /* the knob and the chosen stop's number */
#define SETTINGS_GOLD_G 200
#define SETTINGS_GOLD_B 60

typedef struct {
    bool open; /* the settings screen is up */
    int view;  /* STREAM_VIEW_MIN .. STREAM_VIEW_MAX */
} Settings;

/* Closed, view distance STREAM_VIEW_DEFAULT. */
void settings_init(Settings *s);

/* The middle column of view distance v's stop. */
int settings_stop_x(int v);

/* The view distance whose stop is nearest column x (half way between two
 * stops goes to the right one); anything left of the first stop or right
 * of the last is that stop. */
int settings_view_at(int x);

/* A touch at (x, y): in rows SETTINGS_TOUCH_Y0 .. SETTINGS_TOUCH_Y1 - 1 it
 * chooses the stop nearest x. True if the view distance changed. */
bool settings_touch(Settings *s, int x, int y);

/* The D-pad: one stop left (by -1) or right (by 1), never past the ends.
 * True if the view distance changed. */
bool settings_step(Settings *s, int by);

/* Draw the settings screen over the whole of the bottom screen's picture:
 * BGR8, w x h seen (320 x 240), kept the way fb_put_bgr8() describes. A
 * dark blue-grey background; "SETTINGS"; "VIEW DISTANCE"; the slider's
 * track with a mark at each stop and the gold knob on the chosen one; each
 * stop's number under it (the chosen one gold, the rest grey); the choice
 * in words ("3 CHUNKS"); and "SELECT TO CLOSE". */
void settings_draw_bgr8(unsigned char *buf, int w, int h, const Settings *s);

#endif
