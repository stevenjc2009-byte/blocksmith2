#ifndef NEWWORLD_H
#define NEWWORLD_H

#include <stdbool.h>
#include <stdint.h>

#include "settings.h"

/* The new-world screen (roadmap row 2-03), on the bottom screen when the
 * game starts, before any ground is made: the new world's seed (seed.h),
 * random at first, and three buttons. RANDOM SEED (or X) rolls a new random
 * seed, TYPE A SEED (or Y) opens the 3DS keyboard to type one, and PLAY (or
 * A) makes the world from the seed shown and starts it. Pure C, no 3DS
 * calls, so the PC tests can run it.
 *
 * When a saved world can be carried on (roadmap row 2-04, save.h), a
 * CONTINUE button (or B) takes the place of the title and opens the world
 * played last, and PLAY reads "A  NEW WORLD". Without one the screen is as
 * row 2-03 made it.
 *
 * Bottom-screen points are as the touch screen reports them: x 0..319 from
 * the left, y 0..239 from the top. */

/* Where the words go (each line centred). */
#define NEWWORLD_TITLE_Y 30 /* "NEW WORLD", 3 times the font's size */
#define NEWWORLD_LABEL_Y 66 /* "SEED", twice the size, like the rest */
#define NEWWORLD_SEED_Y 86  /* the seed, gold */

/* The buttons: NEWWORLD_BUTTON_W x NEWWORLD_BUTTON_H from column
 * NEWWORLD_BUTTON_X, one above the other; each button's words sit in its
 * middle. A touch anywhere on a button presses it. */
#define NEWWORLD_BUTTON_X 50
#define NEWWORLD_BUTTON_W 220
#define NEWWORLD_BUTTON_H 30
#define NEWWORLD_RANDOM_Y 112 /* "X  RANDOM SEED" */
#define NEWWORLD_TYPE_Y 150   /* "Y  TYPE A SEED" */
#define NEWWORLD_PLAY_Y 188   /* "A  PLAY", "A  NEW WORLD" with a save */
#define NEWWORLD_CONTINUE_Y 30 /* "B  CONTINUE", only with a save */

/* The colours: the settings screen's background and gold (settings.h), grey
 * buttons and blue PLAY and CONTINUE buttons. */
#define NEWWORLD_BG_R SETTINGS_BG_R
#define NEWWORLD_BG_G SETTINGS_BG_G
#define NEWWORLD_BG_B SETTINGS_BG_B
#define NEWWORLD_BUTTON_R 64
#define NEWWORLD_BUTTON_G 72
#define NEWWORLD_BUTTON_B 92
#define NEWWORLD_PLAY_R 60
#define NEWWORLD_PLAY_G 110
#define NEWWORLD_PLAY_B 190

typedef enum {
    NEWWORLD_NONE,   /* no button there */
    NEWWORLD_RANDOM, /* roll a new random seed */
    NEWWORLD_TYPE,   /* type a seed */
    NEWWORLD_PLAY,   /* make the world and start */
    NEWWORLD_CONTINUE /* open the saved world played last */
} NewWorldButton;

/* The button at (x, y), or NEWWORLD_NONE; saved: CONTINUE is shown. */
NewWorldButton newworld_button_at(int x, int y, bool saved);

/* Draw the new-world screen over the whole of the bottom screen's picture:
 * BGR8, w x h seen (320 x 240), kept the way fb_put_bgr8() describes. The
 * settings screen's background; "NEW WORLD"; "SEED"; the seed written out
 * (seed_to_text()) in gold; the three buttons with their words in white.
 * saved: the CONTINUE button instead of "NEW WORLD", and "A  NEW WORLD" on
 * the PLAY button. */
void newworld_draw_bgr8(unsigned char *buf, int w, int h, uint32_t seed, bool saved);

#endif
