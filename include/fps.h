#ifndef FPS_H
#define FPS_H

#include <stdbool.h>
#include <stdint.h>

/* Frame pacing and the frame-rate counter (roadmap row 1-11). Pure C, no
 * 3DS calls, so the PC tests can run it.
 *
 * Times are 3DS system ticks (svcGetSystemTick(): SYSCLOCK_ARM11 a second).
 * The screens refresh about 59.83 times a second. The game hands over a
 * new picture every second refresh, so it runs at 30 frames a second, the
 * same on Old and New 3DS, as long as a frame's work takes under two
 * refreshes. */
#define FPS_TICKS_PER_SEC 268111856u /* SYSCLOCK_ARM11 */
#define FPS_REFRESH_TICKS 4481153u   /* one screen refresh: FPS_TICKS_PER_SEC / 59.831 */

/* After a frame's picture is handed over, the game waits for screen
 * refreshes until this is true: one and a half refreshes or more since the
 * last frame's wait ended (time last), so the wait ends on the second
 * refresh, not the first. A frame whose work ran past two refreshes ends
 * on the next one after that. */
bool fps_pace_done(uint64_t last, uint64_t now);

typedef struct {
    uint64_t start; /* when the current count began */
    int frames;     /* frames since then */
    int shown;      /* frames a second over the last count; 0 until one has finished */
} FpsCounter;

void fps_init(FpsCounter *c, uint64_t now);

/* A frame ended at time now. Once a second or more has passed since the
 * count began, shown becomes the frames a second over that time, rounded,
 * and a new count begins at now. */
void fps_frame(FpsCounter *c, uint64_t now);

/* The counter's words: "30 FPS"; " 7 FPS" under ten; "99 FPS" for 99 and
 * up; "-- FPS" before the first count has finished (shown 0). */
#define FPS_TEXT_LEN 7 /* 6 letters and the end mark */
void fps_text(int shown, char out[FPS_TEXT_LEN]);

/* The counter in the top-left corner of the bottom screen: a dark grey box
 * with the words in white at twice the font's size. Sizes as for
 * fb_put_bgr8() (the bottom screen is 320 wide, 240 tall). */
#define FPS_BOX_X 4
#define FPS_BOX_Y 4
#define FPS_SCALE 2
#define FPS_PAD 4
#define FPS_BOX_GREY 24
void fps_draw_bgr8(unsigned char *buf, int w, int h, int shown);

#endif
