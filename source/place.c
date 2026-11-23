#include <math.h>
#include <stddef.h>

#include "place.h"

#include "block.h"

/* The same touching-is-not-overlapping margin as player.c. */
#define EPS 1e-4f

static int down(float v)
{
    return (int)floorf(v);
}

void place_spot(const RayHit *hit, int *x, int *y, int *z)
{
    *x = hit->x + (hit->face == FACE_POS_X) - (hit->face == FACE_NEG_X);
    *y = hit->y + (hit->face == FACE_POS_Y) - (hit->face == FACE_NEG_Y);
    *z = hit->z + (hit->face == FACE_POS_Z) - (hit->face == FACE_NEG_Z);
}

/* The blocks the body covers along one axis run from down(lo + EPS) to
 * down(hi - EPS), as in player.c's box_hits(). */
static bool covers(float lo, float hi, int b)
{
    return b >= down(lo + EPS) && b <= down(hi - EPS);
}

bool place_hits_player(const Player *p, int x, int y, int z)
{
    return covers(p->x - PLAYER_HALF, p->x + PLAYER_HALF, x) &&
           covers(p->y, p->y + PLAYER_HEIGHT, y) &&
           covers(p->z - PLAYER_HALF, p->z + PLAYER_HALF, z);
}

bool place_block(World *w, const Player *p, const RayHit *hit, int b, int *x, int *y, int *z)
{
    int here;

    place_spot(hit, x, y, z);
    if (b <= BLOCK_AIR || b >= BLOCK_COUNT || b == BLOCK_WATER)
        return false;
    if (*y < 0 || *y >= WORLD_HEIGHT || world_chunk(w, block_to_chunk(*x), block_to_chunk(*z)) == NULL)
        return false;
    here = world_get(w, *x, *y, *z);
    /* Row 4-01: a block takes water's place; a torch does not go under water. */
    if (here != BLOCK_AIR && (here != BLOCK_WATER || b == BLOCK_TORCH))
        return false;
    /* A torch stands on a solid block and does not get in the way, so it
     * may go where the player is (row 3-03). */
    if (b == BLOCK_TORCH ? !block_solid(world_get(w, *x, *y - 1, *z)) : place_hits_player(p, *x, *y, *z))
        return false;
    world_set(w, *x, *y, *z, b);
    return true;
}
