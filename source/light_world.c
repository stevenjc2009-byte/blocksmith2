#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "light.h"

/* The world-side light calls (light.h): which chunks a block edit can
 * relight, and one cell's torch light read straight from the world.
 * Split from light.c, which keeps light_build() and its grid. */

/* The neighbour (NB_*) one chunk over by [dz + 1][dx + 1], as chunk_mesh.c and light.c. */
static const signed char k_nb_at[3][3] = {
    { NB_NEG_X_NEG_Z, NB_NEG_Z, NB_POS_X_NEG_Z },
    { NB_NEG_X, -1, NB_POS_X },
    { NB_NEG_X_POS_Z, NB_POS_Z, NB_POS_X_POS_Z },
};

/* One step out of each face, FACE_* order, as light.c. */
static const signed char k_step[6][3] = {
    { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 },
};

static void grow(int box[4], int x, int z)
{
    if (x < box[0]) box[0] = x;
    if (z < box[1]) box[1] = z;
    if (x > box[2]) box[2] = x;
    if (z > box[3]) box[3] = z;
}

bool light_touched_box(const World *w, int x, int y, int z, bool torch_edited, int box[4])
{
    const int r = LIGHT_REACH;
    int b[4] = { INT_MAX, INT_MAX, INT_MIN, INT_MIN };
    int dx, dy, dz;

    if (torch_edited)
        grow(b, x, z);
    for (dy = -r; dy <= r; dy++) {
        int ry = r - (dy < 0 ? -dy : dy);

        if (y + dy < 0 || y + dy >= WORLD_HEIGHT)
            continue;
        for (dz = -ry; dz <= ry; dz++) {
            int rx = ry - (dz < 0 ? -dz : dz);

            for (dx = -rx; dx <= rx; dx++)
                if (world_get(w, x + dx, y + dy, z + dz) == BLOCK_TORCH)
                    grow(b, x + dx, z + dz);
        }
    }
    if (b[0] > b[2])
        return false;
    /* A changed cell is within r of a torch; a mesh samples one cell past
     * its chunk on each side. */
    box[0] = block_to_chunk(b[0] - r - 1);
    box[1] = block_to_chunk(b[1] - r - 1);
    box[2] = block_to_chunk(b[2] + r + 1);
    box[3] = block_to_chunk(b[3] + r + 1);
    return true;
}

/* light_world_level() searches breadth first out from the cell itself, over
 * open cells, for the nearest torch: the first one found, d steps away,
 * gives LIGHT_TORCH - d, the most any torch can (the rule above, walked
 * backwards: a path from a torch is a path to it). Every cell it looks at
 * is within LIGHT_REACH of the cell, so in a cube NEAR across round it and
 * in its chunk or the eight round that (LIGHT_REACH < CHUNK_SIZE). s_seen
 * marks the cells already queued with this search's stamp, so it is
 * cleared only once every 255 searches. */
#define NEAR (2 * LIGHT_REACH + 1)
#define NEAR_QUEUE 4096 /* more than the 3,303 cells within LIGHT_REACH steps */

_Static_assert(LIGHT_REACH < CHUNK_SIZE, "the search must stay in the 3 x 3 chunks");
_Static_assert(NEAR <= 32, "queue cells pack x, y and z in 5 bits each");

static uint8_t s_seen[NEAR][NEAR][NEAR];
static uint8_t s_stamp;
static uint32_t s_near[NEAR_QUEUE];

/* Whether any torch of the 3 x 3 chunks at[][] is within LIGHT_REACH steps
 * of local cell (lx, y, lz), walls aside: a quick scan (as light_build()'s)
 * that spares the search in open ground, where no torch is near. A torch
 * farther than that can light nothing here, whatever lies between. */
static bool torch_near(const Chunk *at[3][3], int lx, int y, int lz)
{
    int y0 = y - LIGHT_REACH < 0 ? 0 : y - LIGHT_REACH;
    int y1 = y + LIGHT_REACH > WORLD_HEIGHT - 1 ? WORLD_HEIGHT - 1 : y + LIGHT_REACH;
    int i, j;

    for (j = 0; j < 3; j++)
        for (i = 0; i < 3; i++) {
            const uint8_t *base, *p, *end;

            if (at[j][i] == NULL)
                continue;
            base = p = &at[j][i]->blocks[y0][0][0];
            end = base + (size_t)(y1 - y0 + 1) * CHUNK_SIZE * CHUNK_SIZE;
            while ((p = memchr(p, BLOCK_TORCH, (size_t)(end - p))) != NULL) {
                size_t off = (size_t)(p - base);
                int ty = y0 + (int)(off / (CHUNK_SIZE * CHUNK_SIZE));
                int tz = (int)(off / CHUNK_SIZE % CHUNK_SIZE) + (j - 1) * CHUNK_SIZE;
                int tx = (int)(off % CHUNK_SIZE) + (i - 1) * CHUNK_SIZE;

                p++;
                if (abs(tx - lx) + abs(ty - y) + abs(tz - lz) <= LIGHT_REACH)
                    return true;
            }
        }
    return false;
}

int light_world_level(const World *w, int x, int y, int z)
{
    int cx = block_to_chunk(x), cz = block_to_chunk(z), i, j, b;
    const Chunk *c = world_chunk(w, cx, cz), *at[3][3];
    const Chunk *nb[NB_COUNT];
    int lx = x - cx * CHUNK_SIZE, lz = z - cz * CHUNK_SIZE;
    unsigned head = 0, tail = 0;

    if (c == NULL || y < 0 || y >= WORLD_HEIGHT)
        return 0;
    b = c->blocks[y][lz][lx];
    if (b == BLOCK_TORCH)
        return LIGHT_TORCH;
    if (block_solid(b))
        return 0;
    world_neighbours(w, cx, cz, nb);
    for (j = 0; j < 3; j++)
        for (i = 0; i < 3; i++)
            at[j][i] = (i == 1 && j == 1) ? c : nb[k_nb_at[j][i]];
    if (!torch_near(at, lx, y, lz))
        return 0;
    if (++s_stamp == 0) {
        memset(s_seen, 0, sizeof s_seen);
        s_stamp = 1;
    }
    s_seen[LIGHT_REACH][LIGHT_REACH][LIGHT_REACH] = s_stamp;
    s_near[tail++] = (uint32_t)LIGHT_REACH << 10 | (uint32_t)LIGHT_REACH << 5 | LIGHT_REACH;
    while (head != tail) {
        uint32_t q = s_near[head++];
        int gx = (int)(q & 31), gy = (int)(q >> 5 & 31), gz = (int)(q >> 10 & 31);
        int d = (int)(q >> 15) + 1, f; /* the steps to each neighbour */

        for (f = 0; f < 6; f++) {
            int nx = gx + k_step[f][0], ny = gy + k_step[f][1], nz = gz + k_step[f][2];
            int bx = lx + nx - LIGHT_REACH, by = y + ny - LIGHT_REACH, bz = lz + nz - LIGHT_REACH;
            int dx = bx < 0 ? -1 : bx >= CHUNK_SIZE ? 1 : 0;
            int dz = bz < 0 ? -1 : bz >= CHUNK_SIZE ? 1 : 0;
            const Chunk *o;

            if (by < 0 || by >= WORLD_HEIGHT || s_seen[ny][nz][nx] == s_stamp)
                continue;
            s_seen[ny][nz][nx] = s_stamp;
            o = at[dz + 1][dx + 1];
            b = o ? o->blocks[by][bz - dz * CHUNK_SIZE][bx - dx * CHUNK_SIZE] : BLOCK_AIR;
            if (b == BLOCK_TORCH)
                return LIGHT_TORCH - d;
            if (!block_solid(b) && d < LIGHT_REACH && tail < NEAR_QUEUE)
                s_near[tail++] = (uint32_t)d << 15 | (uint32_t)nz << 10 | (uint32_t)ny << 5 |
                                 (uint32_t)nx;
        }
    }
    return 0;
}
