#ifndef TERRAIN_NOISE_H
#define TERRAIN_NOISE_H

#include <stdint.h>

/* Smooth noise for the ground (terrain.c). Pure C, whole numbers only.
 *
 * A layer is a square grid of random values, blended smoothly in between.
 * It is laid over the world turned and stretched: the layer reads world
 * column (x, z) at u = a * x - b * z, v = b * x + a * z, with a grid point
 * every 2^shift there. So its bumps are about 2^shift / sqrt(a*a + b*b)
 * blocks apart, and they do not line up with the blocks.
 *
 * The steepest a layer gets: next-door columns differ by at most
 * 1.5 * (a + b) / 2^shift of NOISE_ONE (the smooth blend climbs at most 1.5
 * times as fast as a straight line would). terrain.c adds these up to keep
 * the land walkable. */

#define NOISE_ONE 65536

typedef struct {
    uint32_t id;   /* which random values: each layer has its own */
    int a, b;      /* the turn and stretch, 0 or more, a + b at most 40 */
    int shift;     /* grid spacing 2^shift, 1 to 16 */
} NoiseLayer;

/* 0 to NOISE_ONE - 1 at world column (x, z); x and z up to about a million
 * blocks from the origin either way. */
int noise_value(uint32_t seed, const NoiseLayer *l, int x, int z);

/* Slope noise: a random slope at each grid point instead of a random
 * value, blended the same way. -NOISE_SLOPE_TOP..NOISE_SLOPE_TOP, 0 at
 * every grid point, and about as steep everywhere as anywhere, so the lines
 * where it is near a level (coasts, rivers) come out of an even width.
 * Steepest: next-door columns differ by at most 1.78 * sqrt(a*a + b*b) /
 * 2^shift of NOISE_ONE (found by trying every corner slope at 49 x 49
 * spots in a cell). */
#define NOISE_SLOPE_TOP 46341 /* NOISE_ONE / sqrt(2) */
int noise_slope(uint32_t seed, const NoiseLayer *l, int x, int z);

/* 3t^2 - 2t^3 for t from 0 to NOISE_ONE: an S-shaped step that starts and
 * ends flat. */
int noise_smooth(int t);

#endif
