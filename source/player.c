#include <math.h>
#include <stddef.h>

#include "player.h"

#include "block.h"

/* Touching is not overlapping: a box face exactly on a block face, or a
 * float rounding hair past it, does not count as inside the block. */
#define EPS 1e-4f
/* Longest move in one piece: under half a block, so no piece can carry the
 * box from one side of a block to the other. */
#define PIECE 0.45f

static int down(float v)
{
    return (int)floorf(v);
}

bool player_solid(const World *w, int x, int y, int z)
{
    if (y < 0)
        return true;
    if (y >= WORLD_HEIGHT)
        return false;
    if (world_chunk(w, block_to_chunk(x), block_to_chunk(z)) == NULL)
        return true; /* not loaded: a wall */
    return world_get(w, x, y, z) != BLOCK_AIR;
}

/* Does the box from (x0, y0, z0) to (x1, y1, z1) overlap a solid block? */
static bool box_hits(const World *w, float x0, float y0, float z0, float x1, float y1, float z1)
{
    int i, j, k;

    for (j = down(y0 + EPS); j <= down(y1 - EPS); j++)
        for (k = down(z0 + EPS); k <= down(z1 - EPS); k++)
            for (i = down(x0 + EPS); i <= down(x1 - EPS); i++)
                if (player_solid(w, i, j, k))
                    return true;
    return false;
}

bool player_fits(const World *w, float x, float y, float z)
{
    return !box_hits(w, x - PLAYER_HALF, y, z - PLAYER_HALF, x + PLAYER_HALF, y + PLAYER_HEIGHT, z + PLAYER_HALF);
}

void player_spawn(Player *p, const World *w, int x, int z)
{
    int y = WORLD_HEIGHT - 1;

    while (y >= 0 && world_get(w, x, y, z) == BLOCK_AIR)
        y--;
    p->x = (float)x + 0.5f;
    p->y = (float)(y + 1);
    p->z = (float)z + 0.5f;
    p->vy = 0.0f;
    p->on_ground = false;
}

/* Each move is less than a block, from a spot where the box fits, so a
 * block it now hits is in the layer its leading face just entered: stop
 * the box flush against that layer. */
static void move_y(Player *p, const World *w, float d)
{
    float y = p->y + d;

    if (player_fits(w, p->x, y, p->z)) {
        p->y = y;
        return;
    }
    if (d < 0.0f) {
        p->y = (float)(down(y + EPS) + 1);
        p->on_ground = true;
    } else {
        p->y = (float)down(y + PLAYER_HEIGHT - EPS) - PLAYER_HEIGHT;
    }
    p->vy = 0.0f;
}

/* True if a block stopped the move. */
static bool move_x(Player *p, const World *w, float d)
{
    float x = p->x + d;

    if (player_fits(w, x, p->y, p->z)) {
        p->x = x;
        return false;
    }
    p->x = d > 0.0f ? (float)down(x + PLAYER_HALF - EPS) - PLAYER_HALF : (float)(down(x - PLAYER_HALF + EPS) + 1) + PLAYER_HALF;
    return true;
}

static bool move_z(Player *p, const World *w, float d)
{
    float z = p->z + d;

    if (player_fits(w, p->x, p->y, z)) {
        p->z = z;
        return false;
    }
    p->z = d > 0.0f ? (float)down(z + PLAYER_HALF - EPS) - PLAYER_HALF : (float)(down(z - PLAYER_HALF + EPS) + 1) + PLAYER_HALF;
    return true;
}

static float bigger(float a, float b)
{
    return a > b ? a : b;
}

void player_step(Player *p, const World *w, float vx, float vz, float dt)
{
    float dx = vx * dt, dz = vz * dt, dy;
    bool hit_x = false, hit_z = false;
    int n, i;

    p->vy -= PLAYER_GRAVITY * dt;
    if (p->vy < -PLAYER_FALL_MAX)
        p->vy = -PLAYER_FALL_MAX;
    dy = p->vy * dt;
    n = (int)(bigger(fabsf(dy), bigger(fabsf(dx), fabsf(dz))) / PIECE) + 1;
    p->on_ground = false;
    for (i = 0; i < n; i++) {
        move_y(p, w, dy / n);
        hit_x |= move_x(p, w, dx / n);
        hit_z |= move_z(p, w, dz / n);
    }

    /* Walked into something while standing: if it is one block high (the
     * spot ahead fits one block up) and there is room to rise here, hop. */
    if (p->on_ground && (hit_x || hit_z) &&
        player_fits(w, p->x, p->y + 1.0f, p->z) &&
        player_fits(w, p->x + (hit_x ? dx : 0.0f), p->y + 1.0f, p->z + (hit_z ? dz : 0.0f)))
        p->vy = PLAYER_HOP_SPEED;
}
