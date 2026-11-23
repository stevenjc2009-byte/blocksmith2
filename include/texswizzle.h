#ifndef TEXSWIZZLE_H
#define TEXSWIZZLE_H

#include <stddef.h>
#include <stdint.h>

/* The PICA200 reads textures in 8x8 tiles, not row by row. Inside a tile
 * the texels go in Z order (x bit, y bit, x bit, ...); tiles go left to
 * right, one row of tiles after another.
 *
 * The GPU reads the first stored row at texture coordinate t = 1 and the
 * last at t = 0, so the top row of the picture is stored first and t = 1
 * is the top, as in OpenGL. (Storing the bottom row first showed the
 * picture upside down in Azahar, row 1-03.) */

/* Where texel (x, y) of a picture w wide goes, with y counted from the top
 * of the picture. Width and height must be multiples of 8. */
size_t tex_tiled_index(int x, int y, int w);

/* Copy a picture stored row by row from the top (one uint32_t a texel,
 * 0xRRGGBBAA) into the tiled order the GPU reads. dst must not overlap src. */
void tex_swizzle(const uint32_t *src, uint32_t *dst, int w, int h);

#endif
