#include <stdlib.h>
#include <string.h>

#include "test.h"
#include "texswizzle.h"

/* Every texel lands on its own slot, and every slot gets one texel. */
static void check_one_to_one(int w, int h)
{
    unsigned char *seen = calloc((size_t)w * h, 1);
    int x, y, bad = 0;

    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            size_t i = tex_tiled_index(x, y, w);
            if (i >= (size_t)w * h || seen[i])
                bad++;
            else
                seen[i] = 1;
        }
    CHECK_EQ_INT(bad, 0);
    free(seen);
}

void test_texswizzle(void)
{
    enum { W = 64, H = 64 };
    static uint32_t src[W * H], dst[W * H];
    int x, y, bad = 0;

    /* The top-left texel comes first; then Z order inside the 8x8 tile,
     * counting rows down from the top. */
    CHECK_EQ_INT((int)tex_tiled_index(0, 0, W), 0);
    CHECK_EQ_INT((int)tex_tiled_index(1, 0, W), 1);
    CHECK_EQ_INT((int)tex_tiled_index(0, 1, W), 2);
    CHECK_EQ_INT((int)tex_tiled_index(1, 1, W), 3);
    CHECK_EQ_INT((int)tex_tiled_index(2, 0, W), 4);
    CHECK_EQ_INT((int)tex_tiled_index(0, 2, W), 8);
    CHECK_EQ_INT((int)tex_tiled_index(4, 0, W), 16);
    CHECK_EQ_INT((int)tex_tiled_index(0, 4, W), 32);
    CHECK_EQ_INT((int)tex_tiled_index(7, 7, W), 63);
    /* Next tile to the right, then the next row of tiles down. */
    CHECK_EQ_INT((int)tex_tiled_index(8, 0, W), 64);
    CHECK_EQ_INT((int)tex_tiled_index(0, 8, W), 64 * (W / 8));
    /* The bottom-right texel is the last one. */
    CHECK_EQ_INT((int)tex_tiled_index(W - 1, H - 1, W), W * H - 1);
    /* A picture wider than tall. */
    CHECK_EQ_INT((int)tex_tiled_index(0, 0, 16), 0);
    CHECK_EQ_INT((int)tex_tiled_index(8, 0, 16), 64);
    CHECK_EQ_INT((int)tex_tiled_index(15, 7, 16), 127);

    check_one_to_one(W, H);
    check_one_to_one(16, 8);
    check_one_to_one(8, 32);

    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            src[y * W + x] = ((uint32_t)x << 16) | (uint32_t)y;
    memset(dst, 0xFF, sizeof dst);
    tex_swizzle(src, dst, W, H);
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            if (dst[tex_tiled_index(x, y, W)] != src[y * W + x])
                bad++;
    CHECK_EQ_INT(bad, 0);
}
