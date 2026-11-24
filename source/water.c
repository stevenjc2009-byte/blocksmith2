#include <math.h>
#include <string.h>

#include "water.h"

#include "block.h"

/* The row tests below read four blocks at a time and need the flowing and
 * falling ids side by side, in level order, under 128. */
_Static_assert(BLOCK_FALL == BLOCK_FLOW_1 + WATER_LEVEL_MAX, "flowing ids then falling, in order");
_Static_assert(BLOCK_FLOW_1 >= 1 && BLOCK_FALL + 1 <= 128 && BLOCK_WATER < 128,
               "water ids fit the four-at-a-time tests");

int water_level(int b)
{
    if (b == BLOCK_WATER)
        return 0;
    if (b >= BLOCK_FLOW_1 && b <= BLOCK_FALL)
        return b - BLOCK_FLOW_1 + 1;
    return -1;
}

int water_block(int level)
{
    if (level == 0)
        return BLOCK_WATER;
    if (level >= 1 && level <= WATER_FALLING)
        return BLOCK_FLOW_1 + level - 1;
    return BLOCK_AIR;
}

int water_amount(int b)
{
    int n = water_level(b);

    if (n < 0)
        return 0;
    return n == 0 || n == WATER_FALLING ? 8 : 8 - n;
}

float water_height(int b)
{
    int n = water_level(b);

    if (n < 0)
        return 0.0f;
    if (n == 0)
        return WATER_TOP;
    if (n == WATER_FALLING)
        return 1.0f;
    return (float)(8 - n) / 9.0f;
}

#define ONES 0x01010101u

/* Does any of the four bytes of x lie strictly between m and n (0 <= m <=
 * 127, n <= 128)? "hasbetween" from Sean Anderson's Bit Twiddling Hacks:
 * each byte's sums stay inside that byte (no carry or borrow crosses into
 * the next), so the answer is exact for every byte value. */
static bool any_between(uint32_t x, uint32_t m, uint32_t n)
{
    uint32_t low = x & (ONES * 127u);

    return ((ONES * (127u + n) - low) & ~x & (low + ONES * (127u - m)) & (ONES * 128u)) != 0;
}

/* Does any of the four bytes of x equal b? ("haszero" of x ^ b: a borrow
 * can only mark a byte above one that really is zero, so the yes or no is
 * exact.) */
static bool any_equal(uint32_t x, uint32_t b)
{
    uint32_t v = x ^ (ONES * b);

    return ((v - ONES) & ~v & (ONES * 128u)) != 0;
}

bool water_row_flowing(const uint8_t *row, int n)
{
    int i;

    for (i = 0; i < n; i += 4) {
        uint32_t x;

        memcpy(&x, row + i, sizeof x);
        if (any_between(x, BLOCK_FLOW_1 - 1, BLOCK_FALL + 1))
            return true;
    }
    return false;
}

bool water_row_has(const uint8_t *row, int n)
{
    int i;

    for (i = 0; i < n; i += 4) {
        uint32_t x;

        memcpy(&x, row + i, sizeof x);
        if (any_equal(x, BLOCK_WATER) || any_between(x, BLOCK_FLOW_1 - 1, BLOCK_FALL + 1))
            return true;
    }
    return false;
}

bool water_eye_under(const World *w, float x, float y, float z)
{
    int bx = (int)floorf(x), by = (int)floorf(y), bz = (int)floorf(z);

    int b = world_get(w, bx, by, bz);

    if (!block_is_water(b))
        return false;
    if (block_is_water(world_get(w, bx, by + 1, bz)))
        return true; /* a full cell */
    return y - (float)by < water_height(b);
}

/* Minecraft's FlowingFluid.getFlow(), flat (water.h). */
void water_current(const World *w, int x, int y, int z, float *fx, float *fz)
{
    static const int k_side[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    int b = world_get(w, x, y, z), n = water_level(b), i;
    float own, sx = 0.0f, sz = 0.0f, len;

    *fx = *fz = 0.0f;
    if (n <= 0 || n == WATER_FALLING)
        return; /* not water, still, or falling */
    own = (float)water_amount(b) / 9.0f;
    for (i = 0; i < 4; i++) {
        int nx = x + k_side[i][0], nz = z + k_side[i][1];
        int s = world_get(w, nx, y, nz), under;
        float d = 0.0f;

        if (block_is_water(s)) {
            d = own - (float)water_amount(s) / 9.0f;
        } else if (!block_solid(s)) {
            /* An open side over water: the water a cell lower, its top
             * 8/9 under ours, pulls hard. */
            under = world_get(w, nx, y - 1, nz);
            if (block_is_water(under))
                d = own - ((float)water_amount(under) / 9.0f - 8.0f / 9.0f);
        }
        sx += (float)k_side[i][0] * d;
        sz += (float)k_side[i][1] * d;
    }
    len = sqrtf(sx * sx + sz * sz);
    if (len < 1e-6f)
        return;
    *fx = sx / len;
    *fz = sz / len;
}

/* Deep blue, times the light, each part rounded. */
uint32_t water_fog_color(float light)
{
    static const float k_blue[3] = { 30.0f, 72.0f, 150.0f };
    uint32_t c = 0xFF;
    int i;

    if (light < 0.0f)
        light = 0.0f;
    if (light > 1.0f)
        light = 1.0f;
    for (i = 0; i < 3; i++)
        c |= (uint32_t)(k_blue[i] * light + 0.5f) << (24 - 8 * i);
    return c;
}

