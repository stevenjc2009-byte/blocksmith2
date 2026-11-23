#include <stdint.h>

#include "block.h"
#include "blocktex.h"
#include "framebuffer.h"
#include "hotbar.h"
#include "test.h"

/* Row 1-10: the nine-slot hotbar on the bottom screen, chosen by a tap. */

#define BW 320
#define BH 240
#define P HOTBAR_PITCH
#define X0 HOTBAR_X0
#define Y0 HOTBAR_Y0

static unsigned char s_bottom[BW * BH * 3];
static uint32_t s_atlas[BLOCKTEX_SIZE * BLOCKTEX_SIZE];

static void test_layout(void)
{
    int i;

    /* Centred along the bottom, inside the screen. */
    CHECK_EQ_INT(X0, 7);
    CHECK_EQ_INT(X0 + HOTBAR_SLOTS * P + X0, BW);
    CHECK_EQ_INT(Y0 + P + 6, BH);
    for (i = 0; i < HOTBAR_SLOTS; i++) {
        CHECK_EQ_INT(hotbar_slot_at(X0 + i * P + P / 2, Y0 + P / 2), i); /* middle */
        CHECK_EQ_INT(hotbar_slot_at(X0 + i * P, Y0), i);                 /* top-left */
        CHECK_EQ_INT(hotbar_slot_at(X0 + i * P + P - 1, Y0 + P - 1), i); /* bottom-right */
    }
    CHECK_EQ_INT(hotbar_slot_at(X0 - 1, Y0 + 5), -1);
    CHECK_EQ_INT(hotbar_slot_at(X0 + HOTBAR_SLOTS * P, Y0 + 5), -1);
    CHECK_EQ_INT(hotbar_slot_at(X0 + 5, Y0 - 1), -1);
    CHECK_EQ_INT(hotbar_slot_at(X0 + 5, Y0 + P), -1);
    CHECK_EQ_INT(hotbar_slot_at(160, 100), -1); /* the middle of the screen */
    CHECK_EQ_INT(hotbar_slot_at(-5, -5), -1);
}

static void test_choose(void)
{
    Hotbar hb;

    hotbar_init(&hb);
    CHECK_EQ_INT(hb.chosen, 0);
    CHECK_EQ_INT(hotbar_block(&hb), BLOCK_GRASS);
    CHECK_EQ_INT(hb.block[1], BLOCK_DIRT);
    CHECK_EQ_INT(hb.block[2], BLOCK_STONE);
    CHECK_EQ_INT(hb.block[3], BLOCK_TORCH); /* row 3-03 */
    CHECK_EQ_INT(hb.block[4], BLOCK_AIR);
    CHECK_EQ_INT(hb.block[8], BLOCK_AIR);

    CHECK_EQ_INT(hotbar_tap(&hb, X0 + 2 * P + 10, Y0 + 20), 1);
    CHECK_EQ_INT(hb.chosen, 2);
    CHECK_EQ_INT(hotbar_block(&hb), BLOCK_STONE);
    /* A tap anywhere else changes nothing. */
    CHECK_EQ_INT(hotbar_tap(&hb, 160, 100), 0);
    CHECK_EQ_INT(hb.chosen, 2);
    CHECK_EQ_INT(hotbar_tap(&hb, X0 + 1 * P + 3, Y0 - 1), 0);
    CHECK_EQ_INT(hb.chosen, 2);
    /* Dirt, then an empty slot: nothing in hand. */
    CHECK_EQ_INT(hotbar_tap(&hb, X0 + 1 * P, Y0 + P - 1), 1);
    CHECK_EQ_INT(hotbar_block(&hb), BLOCK_DIRT);
    CHECK_EQ_INT(hotbar_tap(&hb, X0 + 8 * P + P - 1, Y0), 1);
    CHECK_EQ_INT(hb.chosen, 8);
    CHECK_EQ_INT(hotbar_block(&hb), BLOCK_AIR);
}

/* What the picture should hold at (x, y), worked out pixel by pixel from
 * the description in hotbar.h; -1 means the pixel is left alone. Colour as
 * 0xRRGGBB. */
static long want(const Hotbar *hb, int x, int y)
{
    int cx = X0 + hb->chosen * P, i;

    if (x >= cx - 1 && x <= cx + P && y >= Y0 - 1 && y <= Y0 + P &&
        (x <= cx || x >= cx + P - 1 || y <= Y0 || y >= Y0 + P - 1))
        return 0xFFFFFF;
    for (i = 0; i < HOTBAR_SLOTS; i++) {
        int x0 = X0 + i * P;

        if (x < x0 || x >= x0 + P || y < Y0 || y >= Y0 + P)
            continue;
        if (x == x0 || x == x0 + P - 1 || y == Y0 || y == Y0 + P - 1)
            return 0x181818;
        if (hb->block[i] == BLOCK_AIR)
            return 0x3C3C3C;
        {
            int t = block_face_tile(hb->block[i], FACE_NEG_Z); /* any side */
            int tx = (t % BLOCKTEX_PER_ROW) * BLOCKTEX_TILE + (x - x0 - 1) / 2;
            int ty = (t / BLOCKTEX_PER_ROW) * BLOCKTEX_TILE + (y - Y0 - 1) / 2;

            return (long)(s_atlas[ty * BLOCKTEX_SIZE + tx] >> 8);
        }
    }
    return -1;
}

/* The colour seen at (x, y), as 0xRRGGBB. */
static long seen(int x, int y)
{
    int i = (x * BH + (BH - 1 - y)) * 3;

    return ((long)s_bottom[i + 2] << 16) | ((long)s_bottom[i + 1] << 8) | s_bottom[i];
}

static void test_draw(void)
{
    Hotbar hb;
    int c, x, y;

    blocktex_build(s_atlas);
    hotbar_init(&hb);
    hb.block[5] = BLOCK_STONE; /* a filled slot after an empty one */
    for (c = 0; c < HOTBAR_SLOTS; c++) {
        int wrong = 0, white = 0, left = 0;

        hb.chosen = c;
        fb_fill_bgr8(s_bottom, BW * BH, 80, 150, 60);
        hotbar_draw_bgr8(s_bottom, BW, BH, &hb, s_atlas);
        for (x = 0; x < BW; x++)
            for (y = 0; y < BH; y++) {
                long e = want(&hb, x, y);

                if (e < 0) {
                    wrong += seen(x, y) != 0x50963C; /* the green fill, untouched */
                    left++;
                } else {
                    wrong += seen(x, y) != e;
                    white += e == 0xFFFFFF;
                }
            }
        CHECK_EQ_INT(wrong, 0);
        CHECK_EQ_INT(white, (P + 2) * (P + 2) - (P - 2) * (P - 2)); /* the 2-pixel frame */
        CHECK_EQ_INT(left > BW * BH - (HOTBAR_SLOTS * P + 2) * (P + 2) - 1, 1);
    }

    /* The icons stand the right way up: grass is green along its top row and
     * dirt brown at its bottom; stone is grey. */
    hb.chosen = 8;
    fb_fill_bgr8(s_bottom, BW * BH, 0, 0, 0);
    hotbar_draw_bgr8(s_bottom, BW, BH, &hb, s_atlas);
    {
        long top = seen(X0 + 10, Y0 + 2), bottom = seen(X0 + 10, Y0 + P - 3);
        long stone = seen(X0 + 2 * P + 10, Y0 + 10);
        int tr = (int)(top >> 16), tg = (int)(top >> 8 & 255);
        int br = (int)(bottom >> 16), bg = (int)(bottom >> 8 & 255);
        int sr = (int)(stone >> 16), sg = (int)(stone >> 8 & 255), sb = (int)(stone & 255);

        CHECK_EQ_INT(tg > tr, 1);
        CHECK_EQ_INT(br > bg, 1);
        CHECK_EQ_INT(sr - sg < 16 && sg - sr < 16 && sg - sb < 16 && sb - sg < 16, 1);
    }
}

/* Row 3-03: a saved hotbar comes back as it was, with a torch put in the
 * first empty slot when it has none (a world from before torches). */
static void test_load(void)
{
    static const uint8_t old[HOTBAR_SLOTS] = { 1, 2, 3, 0, 3, 0, 0, 0, 0 };
    static const uint8_t has[HOTBAR_SLOTS] = { 0, 4, 1, 0, 0, 0, 0, 0, 2 };
    static const uint8_t full[HOTBAR_SLOTS] = { 1, 2, 3, 1, 2, 3, 1, 2, 3 };
    Hotbar hb;
    int i;

    hotbar_load(&hb, old, 4);
    CHECK_EQ_INT(hb.chosen, 4);
    CHECK_EQ_INT(hb.block[3], BLOCK_TORCH);
    CHECK_EQ_INT(hb.block[4], BLOCK_STONE);
    CHECK_EQ_INT(hb.block[5], BLOCK_AIR);
    hotbar_load(&hb, has, 8);
    for (i = 0; i < HOTBAR_SLOTS; i++)
        CHECK_EQ_INT(hb.block[i], has[i]);
    CHECK_EQ_INT(hb.chosen, 8);
    hotbar_load(&hb, full, 0);
    for (i = 0; i < HOTBAR_SLOTS; i++)
        CHECK_EQ_INT(hb.block[i], full[i]);
    CHECK_EQ_INT(BLOCK_TORCH, 4);
}

void test_hotbar(void)
{
    test_layout();
    test_choose();
    test_load();
    test_draw();
}
