#include <math.h>

#include "block.h"
#include "held.h"

#define PI 3.14159265f

void held_init(Held *h)
{
    h->swing = 0.0f;
}

void held_swing(Held *h)
{
    h->swing = HELD_SWING_TIME;
}

void held_step(Held *h, float dt)
{
    h->swing -= dt;
    if (h->swing < 0.0f)
        h->swing = 0.0f;
}

float held_dip(const Held *h)
{
    if (h->swing <= 0.0f)
        return 0.0f;
    return sinf(PI * (1.0f - h->swing / HELD_SWING_TIME));
}

int held_build(int block, float t, MeshVertex out[MESH_BLOCK_VERTS])
{
    int n = block > BLOCK_AIR && block < BLOCK_COUNT ? mesh_block(out, -0.5f, -0.5f, -0.5f, block) : 0;
    int k;

    for (k = 0; k < n; k++)
        if (out[k].t < t)
            out[k].t = t;
    return n;
}

void held_place(Mat4 *out, const Held *h)
{
    float d = held_dip(h);
    Mat4 m, r;

    mat4_translate(out, HELD_X - 0.5f * HELD_SWING_DIP * d, HELD_Y - HELD_SWING_DIP * d,
                   HELD_Z - 0.5f * HELD_SWING_DIP * d);
    mat4_rotate_x(&r, -HELD_SWING_TIP * d);
    mat4_mul(out, out, &r);
    mat4_rotate_y(&r, HELD_TURN);
    mat4_mul(out, out, &r);
    mat4_identity(&m);
    m.m[0][0] = m.m[1][1] = m.m[2][2] = HELD_SIZE;
    mat4_mul(out, out, &m);
}
