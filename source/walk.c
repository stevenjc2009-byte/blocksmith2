#include <math.h>

#include "walk.h"

void walk_velocity(int pad_x, int pad_y, float yaw, float *vx, float *vz)
{
    float len = sqrtf((float)(pad_x * pad_x + pad_y * pad_y));
    float push, fwd, right;

    *vx = 0.0f;
    *vz = 0.0f;
    if (len <= (float)WALK_PAD_DEAD)
        return;
    push = (len - WALK_PAD_DEAD) / (float)(WALK_PAD_FULL - WALK_PAD_DEAD);
    if (push > 1.0f)
        push = 1.0f;
    /* Split the push into forward and right, keeping its direction. */
    fwd = (float)pad_y / len * push * WALK_SPEED;
    right = (float)pad_x / len * push * WALK_SPEED;
    /* Facing yaw: forward is (sin yaw, -cos yaw), right is (cos yaw, sin yaw). */
    *vx = sinf(yaw) * fwd + cosf(yaw) * right;
    *vz = -cosf(yaw) * fwd + sinf(yaw) * right;
}
