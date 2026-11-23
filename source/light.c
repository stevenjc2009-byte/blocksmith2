#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "light.h"

/* light_build() works on the cells within LIGHT_REACH of the ring a mesh
 * samples (local x and z from -1 to CHUNK_SIZE): x and z from -BORDER to
 * CHUNK_SIZE - 1 + BORDER. That is all it needs. A torch that lights a
 * sampled cell is at most LIGHT_REACH steps from it, so inside this square,
 * and every cell on the light's way there is at most LIGHT_REACH from both
 * ends, so inside it too; a cell outside is more than LIGHT_REACH from every
 * sampled cell and can carry no light to one. The same holds for y with the
 * band [ylo - LIGHT_REACH, yhi + LIGHT_REACH]. The square lies inside the
 * chunk and its eight neighbours (BORDER < CHUNK_SIZE). */
#define BORDER (LIGHT_REACH + 1)
#define GRID (CHUNK_SIZE + 2 * BORDER)

_Static_assert(BORDER <= CHUNK_SIZE, "the light square must fit in the 3 x 3 chunks");
_Static_assert(GRID <= 64, "queue cells pack x and z in 6 bits each");

/* The breadth-first queue, a ring of packed cells (y << 12 | z << 6 | x,
 * grid coordinates). Every torch goes in first, all at LIGHT_TORCH, so a
 * cell's level is final the first time it is set and each cell goes in at
 * most once. A real world never has this many waiting at once (a whole
 * layer of torches in open air keeps about 4,000 waiting); if one does, the
 * cells that found it full keep their levels and settle() finishes the job
 * the slow way, so the levels are right whatever the world. */
#define QUEUE (1u << 15)

static uint8_t s_level[WORLD_HEIGHT][GRID][GRID];
static uint32_t s_queue[QUEUE];
static int s_y0, s_y1 = -1;           /* rows of s_level the last build filled */
static int s_ylo, s_yhi = -1;         /* rows it was asked for */
static const Chunk *s_at[3][3];       /* [dz + 1][dx + 1]: the chunk one over */
static bool s_full;                   /* this build's queue turned a cell away */

/* The cells the last build that found a torch set, so the next one clears
 * only those instead of every row: s_queue[0 .. s_dirty), each pushed once
 * when set; or, when that list is long, every cell of rows s_dirty_y0 ..
 * s_dirty_y1. Either way every other cell of s_level is 0. A queue that
 * filled or wrapped round (settle() and turned-away cells are not in the
 * list) always counts as long: see the assert. */
static unsigned s_dirty;
static int s_dirty_y0, s_dirty_y1 = -1;

_Static_assert((size_t)QUEUE * 8 > sizeof s_level,
               "a full or wrapped queue (tail >= QUEUE) is cleared row by row");

/* Per grid column (x or z): which chunk over (0, 1, 2 for -1, 0, +1), the
 * column in it, and its steps to the sampled ring (sample_distance());
 * and block_solid() of every byte. */
static uint8_t s_gc[GRID], s_gl[GRID], s_gd[GRID];
static uint8_t s_solid[256];
static bool s_tables;

/* The neighbour (NB_*) one chunk over by [dz + 1][dx + 1], as chunk_mesh.c. */
static const signed char k_nb_at[3][3] = {
    { NB_NEG_X_NEG_Z, NB_NEG_Z, NB_POS_X_NEG_Z },
    { NB_NEG_X, -1, NB_POS_X },
    { NB_NEG_X_POS_Z, NB_POS_Z, NB_POS_X_POS_Z },
};

static const signed char k_step[6][3] = {
    { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 },
};

/* Does the cell at local (x, y, z), x and z from -CHUNK_SIZE to
 * 2 * CHUNK_SIZE - 1 and y in the world, stop light? */
static bool solid_cell(int x, int y, int z)
{
    int dx = x < 0 ? -1 : x >= CHUNK_SIZE ? 1 : 0;
    int dz = z < 0 ? -1 : z >= CHUNK_SIZE ? 1 : 0;
    const Chunk *o = s_at[dz + 1][dx + 1];

    return o != NULL && block_solid(o->blocks[y][z - dz * CHUNK_SIZE][x - dx * CHUNK_SIZE]);
}

/* Steps from (x, y, z) to the nearest sampled cell, counting each axis
 * apart (the fewest steps any path could take). */
static int sample_distance(int x, int y, int z)
{
    int d = 0;

    d += x < -1 ? -1 - x : x > CHUNK_SIZE ? x - CHUNK_SIZE : 0;
    d += z < -1 ? -1 - z : z > CHUNK_SIZE ? z - CHUNK_SIZE : 0;
    d += y < s_ylo ? s_ylo - y : y > s_yhi ? y - s_yhi : 0;
    return d;
}

/* Is grid cell (gx, y, gz) solid? The same answer as solid_cell(). */
static bool solid_grid(int gx, int y, int gz)
{
    const Chunk *o = s_at[s_gc[gz]][s_gc[gx]];

    return o != NULL && s_solid[o->blocks[y][s_gl[gz]][s_gl[gx]]];
}

static void make_tables(void)
{
    int g;

    for (g = 0; g < GRID; g++) {
        int l = g - BORDER, d = l < 0 ? -1 : l >= CHUNK_SIZE ? 1 : 0;

        s_gc[g] = (uint8_t)(d + 1);
        s_gl[g] = (uint8_t)(l - d * CHUNK_SIZE);
        s_gd[g] = (uint8_t)(l < -1 ? -1 - l : l > CHUNK_SIZE ? l - CHUNK_SIZE : 0);
    }
    for (g = 0; g < 256; g++)
        s_solid[g] = block_solid(g);
    s_tables = true;
}

/* Zero the cells the last torch-finding build set (s_dirty above). */
static void clear_last(void)
{
    size_t row = sizeof s_level[0];

    if ((size_t)s_dirty * 8 > (size_t)(s_dirty_y1 - s_dirty_y0 + 1) * row) {
        if (s_dirty_y1 >= s_dirty_y0)
            memset(s_level[s_dirty_y0], 0, (size_t)(s_dirty_y1 - s_dirty_y0 + 1) * row);
    } else {
        unsigned i;

        for (i = 0; i < s_dirty; i++) {
            uint32_t q = s_queue[i];

            s_level[q >> 12][q >> 6 & 63][q & 63] = 0;
        }
    }
    s_dirty = 0;
    s_dirty_y1 = -1;
}

static void push(unsigned *tail, unsigned head, int gx, int y, int gz)
{
    if (*tail - head < QUEUE)
        s_queue[(*tail)++ & (QUEUE - 1)] = (uint32_t)y << 12 | (uint32_t)gz << 6 | (uint32_t)gx;
    else
        s_full = true;
}

/* After a build whose queue filled up: sweep the rows, raising each open
 * cell to one less than its brightest neighbour, until nothing rises. Every
 * level set so far is one some torch really gives (never too high), so this
 * ends on the same levels the breadth-first spread would have found. */
static void settle(void)
{
    bool rose = true;
    int gx, gz, y, f;

    while (rose) {
        rose = false;
        for (y = s_y0; y <= s_y1; y++)
            for (gz = 0; gz < GRID; gz++)
                for (gx = 0; gx < GRID; gx++) {
                    int best = s_level[y][gz][gx];

                    if (best >= LIGHT_TORCH - 1 || solid_cell(gx - BORDER, y, gz - BORDER))
                        continue;
                    for (f = 0; f < 6; f++) {
                        int nx = gx + k_step[f][0], ny = y + k_step[f][1], nz = gz + k_step[f][2];

                        if (nx >= 0 && nx < GRID && nz >= 0 && nz < GRID && ny >= s_y0 &&
                            ny <= s_y1 && s_level[ny][nz][nx] - 1 > best)
                            best = s_level[ny][nz][nx] - 1;
                    }
                    if (best > s_level[y][gz][gx]) {
                        s_level[y][gz][gx] = (uint8_t)best;
                        rose = true;
                    }
                }
    }
}

/* Seed every torch of chunk o (one over by dx, dz) that can light a sampled
 * cell. Clears the last build's cells on the first one. Returns whether
 * there was one. */
static bool seed_chunk(const Chunk *o, int dx, int dz, bool any, unsigned *tail)
{
    const uint8_t *base = &o->blocks[s_y0][0][0], *p = base;
    const uint8_t *end = base + (size_t)(s_y1 - s_y0 + 1) * CHUNK_SIZE * CHUNK_SIZE;

    while ((p = memchr(p, BLOCK_TORCH, (size_t)(end - p))) != NULL) {
        size_t off = (size_t)(p - base);
        int y = s_y0 + (int)(off / (CHUNK_SIZE * CHUNK_SIZE));
        int z = (int)(off / CHUNK_SIZE % CHUNK_SIZE) + dz * CHUNK_SIZE;
        int x = (int)(off % CHUNK_SIZE) + dx * CHUNK_SIZE;

        p++;
        if (sample_distance(x, y, z) > LIGHT_REACH)
            continue;
        if (!any) {
            clear_last(); /* before the first push: the cells it clears are in s_queue */
            any = true;
        }
        s_level[y][z + BORDER][x + BORDER] = LIGHT_TORCH;
        push(tail, 0, x + BORDER, y, z + BORDER);
    }
    return any;
}

bool light_build(const Chunk *c, const Chunk *const nb[NB_COUNT], int ylo, int yhi)
{
    unsigned head = 0, tail = 0;
    bool any = false;
    int i, j;

    s_full = false;
    s_y1 = -1; /* nothing valid until this build has cleared its rows */
    s_ylo = ylo < 0 ? 0 : ylo;
    s_yhi = yhi > WORLD_HEIGHT - 1 ? WORLD_HEIGHT - 1 : yhi;
    if (c == NULL || s_ylo > s_yhi)
        return false;
    if (!s_tables)
        make_tables();
    for (j = 0; j < 3; j++)
        for (i = 0; i < 3; i++)
            s_at[j][i] = (i == 1 && j == 1) ? c : nb ? nb[k_nb_at[j][i]] : NULL;
    s_y0 = s_ylo - LIGHT_REACH < 0 ? 0 : s_ylo - LIGHT_REACH;
    i = s_yhi + LIGHT_REACH > WORLD_HEIGHT - 1 ? WORLD_HEIGHT - 1 : s_yhi + LIGHT_REACH;
    s_y1 = i; /* seed_chunk() scans and clears s_y0 .. s_y1 */
    for (j = 0; j < 3; j++)
        for (i = 0; i < 3; i++)
            if (s_at[j][i] != NULL)
                any = seed_chunk(s_at[j][i], i - 1, j - 1, any, &tail);
    if (!any) {
        s_y1 = -1;
        return false;
    }
    while (head != tail) {
        uint32_t q = s_queue[head++ & (QUEUE - 1)];
        int gx = (int)(q & 63), gz = (int)(q >> 6 & 63), y = (int)(q >> 12);
        int l = s_level[y][gz][gx] - 1, f;

        if (l <= 0)
            continue;
        for (f = 0; f < 6; f++) {
            int nx = gx + k_step[f][0], ny = y + k_step[f][1], nz = gz + k_step[f][2];

            /* A cell farther than l from every sampled cell can pass on no
             * light that reaches one (each step costs a level), so it is
             * left dark: every cell on a lit path to a sampled cell is
             * nearer it than its level. */
            if (nx < 0 || nx >= GRID || nz < 0 || nz >= GRID || ny < s_y0 || ny > s_y1 ||
                s_level[ny][nz][nx] >= l ||
                l <= s_gd[nx] + s_gd[nz] + (ny < s_ylo ? s_ylo - ny : ny > s_yhi ? ny - s_yhi : 0) ||
                solid_grid(nx, ny, nz))
                continue;
            s_level[ny][nz][nx] = (uint8_t)l;
            push(&tail, head, nx, ny, nz);
        }
    }
    s_dirty = tail;
    s_dirty_y0 = s_y0;
    s_dirty_y1 = s_y1;
    if (s_full)
        settle();
    return true;
}

int light_level(int x, int y, int z)
{
    if (y < s_ylo || y > s_yhi || y < s_y0 || y > s_y1 || x < -1 || x > CHUNK_SIZE ||
        z < -1 || z > CHUNK_SIZE)
        return 0;
    return s_level[y][z + BORDER][x + BORDER];
}

float light_brightness(int level)
{
    if (level <= 0)
        return 0.0f;
    if (level >= LIGHT_TORCH)
        return 1.0f;
    return (float)level / (float)LIGHT_TORCH;
}
