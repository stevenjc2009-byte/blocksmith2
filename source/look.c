#include "look.h"

#define PI 3.14159265f

/* A C-stick axis as -1..1, with the dead zone taken out. */
static float stick_axis(int v)
{
    int mag = v < 0 ? -v : v;
    float f;

    if (mag <= LOOK_STICK_DEAD)
        return 0.0f;
    f = (float)(mag - LOOK_STICK_DEAD) / (LOOK_STICK_FULL - LOOK_STICK_DEAD);
    if (f > 1.0f)
        f = 1.0f;
    return v < 0 ? -f : f;
}

static float clamp1(float v)
{
    return v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
}

void look_rates(const LookInput *in, float *yaw_rate, float *pitch_rate)
{
    const float full = LOOK_DEG_PER_S * PI / 180.0f;
    float yaw = (float)in->right - (float)in->left + stick_axis(in->cx);
    float pitch = (float)in->up - (float)in->down + stick_axis(in->cy);

    *yaw_rate = clamp1(yaw) * full;
    *pitch_rate = clamp1(pitch) * full;
}
