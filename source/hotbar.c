#include "hotbar.h"

#include "block.h"
#include "blocktex.h"
#include "framebuffer.h"

#define FRAME_GREY 24 /* each slot's 1-pixel frame */
#define EMPTY_GREY 60 /* inside an empty slot */

void hotbar_init(Hotbar *hb)
{
    int i;

    for (i = 0; i < HOTBAR_SLOTS; i++)
        hb->block[i] = BLOCK_AIR;
    hb->block[0] = BLOCK_GRASS;
    hb->block[1] = BLOCK_DIRT;
    hb->block[2] = BLOCK_STONE;
    hb->block[3] = BLOCK_TORCH;
    hb->block[4] = BLOCK_SAND; /* row 4-01 */
    hb->chosen = 0;
}

/* Put block b in the first empty slot, unless a slot holds it already or
 * none is empty. */
static void add_if_missing(Hotbar *hb, int b)
{
    int i, empty = -1;

    for (i = 0; i < HOTBAR_SLOTS; i++) {
        if (hb->block[i] == b)
            return;
        if (hb->block[i] == BLOCK_AIR && empty < 0)
            empty = i;
    }
    if (empty >= 0)
        hb->block[empty] = b;
}

void hotbar_load(Hotbar *hb, const uint8_t blocks[HOTBAR_SLOTS], int chosen)
{
    int i;

    for (i = 0; i < HOTBAR_SLOTS; i++) /* water cannot be held (row 4-01) */
        hb->block[i] = blocks[i] == BLOCK_WATER ? BLOCK_AIR : blocks[i];
    add_if_missing(hb, BLOCK_TORCH); /* worlds from before row 3-03 */
    add_if_missing(hb, BLOCK_SAND);  /* and before row 4-01 */
    hb->chosen = chosen;
}

int hotbar_slot_at(int x, int y)
{
    if (y < HOTBAR_Y0 || y >= HOTBAR_Y0 + HOTBAR_PITCH || x < HOTBAR_X0 ||
        x >= HOTBAR_X0 + HOTBAR_SLOTS * HOTBAR_PITCH)
        return -1;
    return (x - HOTBAR_X0) / HOTBAR_PITCH;
}

bool hotbar_tap(Hotbar *hb, int x, int y)
{
    int s = hotbar_slot_at(x, y);

    if (s < 0)
        return false;
    hb->chosen = s;
    return true;
}

int hotbar_block(const Hotbar *hb)
{
    return hb->block[hb->chosen];
}

/* A square outline n pixels across from (x0, y0), t pixels thick. */
static void ring(unsigned char *buf, int w, int h, int x0, int y0, int n, int t, unsigned char v)
{
    int i, k;

    for (k = 0; k < t; k++)
        for (i = k; i < n - k; i++) {
            fb_put_bgr8(buf, w, h, x0 + i, y0 + k, v, v, v);
            fb_put_bgr8(buf, w, h, x0 + i, y0 + n - 1 - k, v, v, v);
            fb_put_bgr8(buf, w, h, x0 + k, y0 + i, v, v, v);
            fb_put_bgr8(buf, w, h, x0 + n - 1 - k, y0 + i, v, v, v);
        }
}

/* The inside of a slot from (x0, y0): the block's side tile at twice the
 * size, or plain grey for an empty slot (or anything with no tile). */
static void icon(unsigned char *buf, int w, int h, int x0, int y0, int block, const uint32_t *atlas)
{
    int tile = block_face_tile(block, FACE_POS_X);
    int u, v;

    for (v = 0; v < HOTBAR_ICON; v++)
        for (u = 0; u < HOTBAR_ICON; u++) {
            if (tile < 0) {
                fb_put_bgr8(buf, w, h, x0 + u, y0 + v, EMPTY_GREY, EMPTY_GREY, EMPTY_GREY);
            } else {
                int tx = (tile % BLOCKTEX_PER_ROW) * BLOCKTEX_TILE + u / 2;
                int ty = (tile / BLOCKTEX_PER_ROW) * BLOCKTEX_TILE + v / 2;
                uint32_t c = atlas[ty * BLOCKTEX_SIZE + tx]; /* 0xRRGGBBAA */

                fb_put_bgr8(buf, w, h, x0 + u, y0 + v, (unsigned char)(c >> 24),
                            (unsigned char)(c >> 16), (unsigned char)(c >> 8));
            }
        }
}

void hotbar_draw_bgr8(unsigned char *buf, int w, int h, const Hotbar *hb, const uint32_t *atlas)
{
    int i;

    for (i = 0; i < HOTBAR_SLOTS; i++) {
        int x0 = HOTBAR_X0 + i * HOTBAR_PITCH;

        ring(buf, w, h, x0, HOTBAR_Y0, HOTBAR_PITCH, 1, FRAME_GREY);
        icon(buf, w, h, x0 + 1, HOTBAR_Y0 + 1, hb->block[i], atlas);
    }
    /* Last, so it lies over the neighbours' frames. */
    ring(buf, w, h, HOTBAR_X0 + hb->chosen * HOTBAR_PITCH - 1, HOTBAR_Y0 - 1, HOTBAR_PITCH + 2, 2,
         255);
}
