#include "texswizzle.h"

/* Z order inside an 8x8 tile: x bit 0, y bit 0, x bit 1, y bit 1, ... */
static unsigned morton8(unsigned x, unsigned y)
{
    return (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) |
           ((x & 4) << 2) | ((y & 4) << 3);
}

size_t tex_tiled_index(int x, int y, int w)
{
    unsigned ux = (unsigned)x;
    unsigned uy = (unsigned)y; /* rows counted down from the top */
    size_t tile = (size_t)(uy >> 3) * (unsigned)(w >> 3) + (ux >> 3);

    return tile * 64 + morton8(ux & 7, uy & 7);
}

void tex_swizzle(const uint32_t *src, uint32_t *dst, int w, int h)
{
    int x, y;

    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            dst[tex_tiled_index(x, y, w)] = src[y * w + x];
}
