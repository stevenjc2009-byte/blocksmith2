#include "controls.h"

#include "walk.h"

void read_walk(const Camera *cam, float *vx, float *vz)
{
    circlePosition pad = { 0, 0 };

    hidCircleRead(&pad);
    walk_velocity(pad.dx, pad.dy, cam->yaw, vx, vz);
}

void read_look(LookInput *in, u32 ignore)
{
    u32 held = hidKeysHeld() & ~ignore;
    circlePosition cs = { 0, 0 };

    hidCstickRead(&cs);
    in->up = (held & KEY_X) != 0;
    in->down = (held & KEY_B) != 0;
    in->left = (held & KEY_Y) != 0;
    in->right = (held & KEY_A) != 0;
    in->cx = cs.dx;
    in->cy = cs.dy;
}
