#ifndef WORLDLIST_H
#define WORLDLIST_H

#include <stdbool.h>

#include "worldname.h"

/* The world list (roadmap row 2-05, kickoff W-12), on the bottom screen
 * when the game starts with worlds saved: their names, WORLDLIST_ROWS at a
 * time, the chosen one blue; the D-pad's up and down or a tap on a row
 * chooses. OPEN (or A) plays the chosen world, NEW (or X) goes to the
 * new-world screen (newworld.h), RENAME (or Y) types it a new name with the
 * 3DS keyboard and DELETE (or L) deletes it, after asking: the delete
 * screen names the world and says it cannot be undone; DELETE (or A)
 * deletes it, KEEP (or B) goes back. The row from WORLDLIST_SPARE_Y is left
 * for world backups (roadmap row 2-07). Pure C, no 3DS calls, so the PC
 * tests can run it.
 *
 * Bottom-screen points are as the touch screen reports them: x 0..319 from
 * the left, y 0..239 from the top. */

#define WORLDLIST_ROWS 4

/* "WORLDS" at the top left, just right of the frame counter's box (fps.h,
 * 82 columns wide from the left edge), and "<n> OF <count>" (the chosen
 * world's place) at the top right, both twice the font's size like the
 * rest. */
#define WORLDLIST_TITLE_X 88
#define WORLDLIST_TITLE_Y 8
#define WORLDLIST_COUNT_RIGHT 310 /* the count's last column + 1 */

/* The rows: WORLDLIST_ROW_W x WORLDLIST_ROW_H from column WORLDLIST_ROW_X,
 * the first at WORLDLIST_ROW_Y, each next WORLDLIST_ROW_STEP lower; the
 * name sits WORLDLIST_NAME_INSET in from the row's left. */
#define WORLDLIST_ROW_X 20
#define WORLDLIST_ROW_W 280
#define WORLDLIST_ROW_H 26
#define WORLDLIST_ROW_Y 34
#define WORLDLIST_ROW_STEP 28
#define WORLDLIST_NAME_INSET 8

/* The buttons: two to a row, WORLDLIST_BUTTON_W x WORLDLIST_BUTTON_H. */
#define WORLDLIST_LEFT_X 10
#define WORLDLIST_RIGHT_X 165
#define WORLDLIST_BUTTON_W 145
#define WORLDLIST_BUTTON_H 26
#define WORLDLIST_SPARE_Y 150 /* nothing yet: backups (row 2-07) */
#define WORLDLIST_TOP_Y 180   /* "A  OPEN" (blue), "X  NEW" */
#define WORLDLIST_LOW_Y 210   /* "Y  RENAME", "L  DELETE" */

/* The delete screen: "DELETE WORLD" (3 times the font's size), the world's
 * name in gold, "IT CANNOT BE UNDONE" in grey, then "A  DELETE" (red) on
 * the left and "B  KEEP" on the right, at WORLDLIST_TOP_Y. */
#define WORLDLIST_ASK_TITLE_Y 40
#define WORLDLIST_ASK_NAME_Y 90
#define WORLDLIST_ASK_WARN_Y 120
#define WORLDLIST_RED_R 170
#define WORLDLIST_RED_G 50
#define WORLDLIST_RED_B 50

/* Which world is chosen and which shows in the first row. */
typedef struct {
    int count; /* worlds in the list */
    int sel;   /* the chosen one, 0 .. count - 1 (0 with none) */
    int top;   /* the one in the first row */
} WorldList;

typedef enum {
    WORLDLIST_NONE,   /* no button there */
    WORLDLIST_OPEN,   /* play the chosen world */
    WORLDLIST_NEW,    /* the new-world screen */
    WORLDLIST_RENAME, /* type the chosen world a new name */
    WORLDLIST_DELETE  /* delete the chosen world, after asking */
} WorldListButton;

typedef enum {
    WORLDLIST_ASK_NONE,   /* no button there */
    WORLDLIST_ASK_DELETE, /* delete it */
    WORLDLIST_ASK_KEEP    /* keep it */
} WorldListAsk;

/* count worlds, sel chosen (held to the list), shown in its rows. */
void worldlist_init(WorldList *l, int count, int sel);

/* Choose the world by places further down (up if negative), stopping at
 * either end; the rows follow it. */
void worldlist_move(WorldList *l, int by);

/* The world whose row is at (x, y), or -1. */
int worldlist_row_at(const WorldList *l, int x, int y);

/* Choose world i (held to the list); the rows follow it. */
void worldlist_choose(WorldList *l, int i);

/* The chosen world was deleted: the one after it is chosen, or the one
 * before when it was the last; the rows are filled from the top. */
void worldlist_remove(WorldList *l);

WorldListButton worldlist_button_at(int x, int y);
WorldListAsk worldlist_ask_at(int x, int y);

/* Draw the list over the whole of the bottom screen's picture: BGR8, w x h
 * seen (320 x 240), kept the way fb_put_bgr8() describes; the settings
 * screen's background (settings.h). names[i] is the name in row i, for the
 * rows that hold a world (top + i < count). */
void worldlist_draw_bgr8(unsigned char *buf, int w, int h, const WorldList *l,
                         const char names[WORLDLIST_ROWS][WORLDNAME_SIZE]);

/* Draw the delete screen for the world called name, the same way. */
void worldlist_ask_draw_bgr8(unsigned char *buf, int w, int h, const char *name);

#endif
