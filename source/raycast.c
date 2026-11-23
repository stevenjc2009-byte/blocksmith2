#include <math.h>

#include "block.h"
#include "raycast.h"

#define FAR 1e30f /* "never": an axis the ray does not move along */

/* One axis of the walk: which way it steps, how far along the ray the next
 * block boundary is, and how far apart the boundaries are. */
static void axis(float o, float d, int cell, int *step, float *next, float *gap)
{
    if (d > 0.0f) {
        *step = 1;
        *next = ((float)(cell + 1) - o) / d;
        *gap = 1.0f / d;
    } else if (d < 0.0f) {
        *step = -1;
        *next = (o - (float)cell) / -d;
        *gap = -1.0f / d;
    } else {
        *step = 0;
        *next = FAR;
        *gap = FAR;
    }
}

bool raycast(const World *w, float ox, float oy, float oz,
             float dx, float dy, float dz, float reach, RayHit *hit)
{
    float len = sqrtf(dx * dx + dy * dy + dz * dz);
    int x, y, z, sx, sy, sz, face, b;
    float nx, ny, nz, gx, gy, gz, t;

    if (!(len > 0.0f))
        return false;
    dx /= len;
    dy /= len;
    dz /= len;
    x = (int)floorf(ox);
    y = (int)floorf(oy);
    z = (int)floorf(oz);
    axis(ox, dx, x, &sx, &nx, &gx);
    axis(oy, dy, y, &sy, &ny, &gy);
    axis(oz, dz, z, &sz, &nz, &gz);

    /* Step into whichever neighbour block the ray reaches first (Amanatides
     * and Woo). Every step moves a boundary at least one block further on,
     * so the walk ends. */
    for (;;) {
        if (nx <= ny && nx <= nz) {
            t = nx;
            x += sx;
            nx += gx;
            face = sx > 0 ? FACE_NEG_X : FACE_POS_X;
        } else if (ny <= nz) {
            t = ny;
            y += sy;
            ny += gy;
            face = sy > 0 ? FACE_NEG_Y : FACE_POS_Y;
        } else {
            t = nz;
            z += sz;
            nz += gz;
            face = sz > 0 ? FACE_NEG_Z : FACE_POS_Z;
        }
        if (t > reach)
            return false;
        b = world_get(w, x, y, z);
        if (b != BLOCK_AIR && b != BLOCK_WATER) { /* row 4-01: through water */
            hit->x = x;
            hit->y = y;
            hit->z = z;
            hit->face = face;
            hit->dist = t;
            return true;
        }
    }
}
