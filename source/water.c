#include <math.h>

#include "water.h"

#include "block.h"

bool water_eye_under(const World *w, float x, float y, float z)
{
    int bx = (int)floorf(x), by = (int)floorf(y), bz = (int)floorf(z);

    if (world_get(w, bx, by, bz) != BLOCK_WATER)
        return false;
    if (world_get(w, bx, by + 1, bz) == BLOCK_WATER)
        return true; /* a full cell */
    return y - (float)by < WATER_TOP;
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
