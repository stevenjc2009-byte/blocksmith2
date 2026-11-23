#ifndef SAVEUI_H
#define SAVEUI_H

#include <stdbool.h>

/* The SAVE button and the save message (roadmap row 2-04), on the bottom
 * screen while playing (not over the settings screen). Tapping SAVE saves
 * the world; a message in the middle of the screen then says how it went
 * for SAVEUI_MSG_MS: "WORLD SAVED", or "SAVE FAILED" in red. Autosaves show
 * the same message. Pure C, no 3DS calls, so the PC tests can run it.
 *
 * Bottom-screen points are as the touch screen reports them: x 0..319 from
 * the left, y 0..239 from the top. */

/* The button, in the top-right corner (the frame counter has the top-left,
 * the hotbar the foot). "SAVE" in white in its middle. */
#define SAVEUI_X 238
#define SAVEUI_Y 6
#define SAVEUI_WIDTH 76
#define SAVEUI_HEIGHT 28
#define SAVEUI_R 64 /* the new-world screen's grey buttons (newworld.h) */
#define SAVEUI_G 72
#define SAVEUI_B 92

/* The message: its words centred on row SAVEUI_MSG_Y + 8, in a box of the
 * settings screen's background colour SAVEUI_MSG_PAD wider than them each
 * side, rows SAVEUI_MSG_Y .. SAVEUI_MSG_Y + SAVEUI_MSG_H - 1. */
#define SAVEUI_MSG_Y 96
#define SAVEUI_MSG_H 30
#define SAVEUI_MSG_PAD 12
#define SAVEUI_MSG_MS 2000
#define SAVEUI_FAIL_R 255 /* "SAVE FAILED" */
#define SAVEUI_FAIL_G 110
#define SAVEUI_FAIL_B 90

typedef enum { SAVEUI_NONE, SAVEUI_SAVED, SAVEUI_FAILED } SaveUiMessage;

/* Is (x, y) on the SAVE button? */
bool saveui_hit(int x, int y);

/* Draw the button, and the message unless msg is SAVEUI_NONE, into the
 * bottom screen's picture: BGR8, w x h seen (320 x 240), kept the way
 * fb_put_bgr8() describes. Nothing else is touched. */
void saveui_draw_bgr8(unsigned char *buf, int w, int h, SaveUiMessage msg);

#endif
