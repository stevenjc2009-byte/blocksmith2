#ifndef HOTBAR_H
#define HOTBAR_H

#include <stdbool.h>
#include <stdint.h>

/* The hotbar (roadmap row 1-10): nine block slots in a row along the
 * bottom of the bottom screen (kickoff SC-2). Tapping a slot chooses it
 * (C-5); L places the chosen slot's block. Pure C, no 3DS calls, so the PC
 * tests can run it.
 *
 * Bottom-screen points are as the touch screen reports them: x 0..319 from
 * the left, y 0..239 from the top. */

#define HOTBAR_SLOTS 9
#define HOTBAR_PITCH 34 /* each slot's width and height, its frame included */
#define HOTBAR_ICON 32  /* a 16-pixel block tile drawn at twice the size */
#define HOTBAR_X0 ((320 - HOTBAR_SLOTS * HOTBAR_PITCH) / 2) /* 7: centred */
#define HOTBAR_Y0 (240 - HOTBAR_PITCH - 6)                  /* 200 */

typedef struct {
    int block[HOTBAR_SLOTS]; /* BLOCK_AIR: an empty slot */
    int chosen;              /* 0 .. HOTBAR_SLOTS - 1 */
} Hotbar;

/* Grass, dirt, stone, a torch (row 3-03) and sand (row 4-01) in the first
 * five slots, the rest empty, the first slot chosen. */
void hotbar_init(Hotbar *hb);

/* The hotbar a saved world kept: its slots' blocks and chosen slot (checked
 * by save.c). Water in a slot is taken out (row 4-01: it cannot be held).
 * A world saved before torches came (row 3-03) gets one in its first empty
 * slot, so every world can have them, and then the same for sand (row
 * 4-01); a hotbar that already holds one, or has no empty slot, is kept as
 * it was. */
void hotbar_load(Hotbar *hb, const uint8_t blocks[HOTBAR_SLOTS], int chosen);

/* The slot under bottom-screen point (x, y), or -1 if none. */
int hotbar_slot_at(int x, int y);

/* A tap at (x, y): chooses the slot there, if any. True if it hit one. */
bool hotbar_tap(Hotbar *hb, int x, int y);

/* The chosen slot's block (BLOCK_AIR if that slot is empty). */
int hotbar_block(const Hotbar *hb);

/* Draw the hotbar into the bottom screen's picture: BGR8, w x h seen
 * (320 x 240), kept the way fb_put_bgr8() describes. atlas is the block
 * picture from blocktex_build(). Each slot has a dark frame round its
 * block's side tile (an empty slot is plain grey inside); the chosen slot
 * gets a white frame 2 pixels thick round it. Nothing outside the slots and
 * that white frame is touched. */
void hotbar_draw_bgr8(unsigned char *buf, int w, int h, const Hotbar *hb, const uint32_t *atlas);

#endif
