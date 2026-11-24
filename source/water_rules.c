#include <stdint.h>
#include <string.h>

#include "water_rules.h"

#include "block.h"
#include "water.h"
#include "water_flow.h"
#include "world_coords.h"

/* The four sides, in FACE_* order (+x, -x, +z, -z). */
static const int k_side[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };

/* Can water run into block b: air, or a torch or a plant (washed away, as
 * in Minecraft; plants: row 4-03)? Water never runs into water; a flowing
 * cell changes only on its own turn. */
static bool takes(int b)
{
    return b == BLOCK_AIR || b == BLOCK_TORCH || block_is_plant(b);
}

/* Can water pass block b on its way to a drop (Minecraft's
 * canPassThrough()): anything not solid and not a source. */
static bool passable(int b)
{
    return !block_solid(b) && b != BLOCK_WATER;
}

/* Is (x, y - 1, z) something water would run down into: anything not
 * solid (air, a torch, water)? The floor of the world is solid. */
static bool drop_under(const World *w, int x, int y, int z)
{
    return y > 0 && !block_solid(world_get(w, x, y - 1, z));
}

int water_rules_new(const World *w, int x, int y, int z)
{
    int sources = 0, most = 0, i, under;

    for (i = 0; i < 4; i++) {
        int s = world_get(w, x + k_side[i][0], y, z + k_side[i][1]);

        if (s == BLOCK_WATER)
            sources++;
        if (water_amount(s) > most)
            most = water_amount(s);
    }
    /* Two sources beside it, on a solid block or a source: a new source
     * (the floor of the world counts as solid). */
    under = y > 0 ? world_get(w, x, y - 1, z) : BLOCK_STONE;
    if (sources >= 2 && (block_solid(under) || under == BLOCK_WATER))
        return BLOCK_WATER;
    if (block_is_water(world_get(w, x, y + 1, z)))
        return BLOCK_FALL;
    return most - 1 <= 0 ? BLOCK_AIR : water_block(8 - (most - 1));
}

/* The cells round (ox, y, oz) the search for a drop reads, each read once
 * when first needed: 0 not read yet, 1 water cannot pass it, 2 it can, 3
 * it can and it is a drop. */
#define WIN (2 * WATER_FLOW_REACH + 1)

typedef struct {
    const World *w;
    int ox, y, oz;
    uint8_t cell[WIN][WIN];
} Near;

static int near_at(Near *k, int x, int z)
{
    uint8_t *c = &k->cell[z - k->oz + WATER_FLOW_REACH][x - k->ox + WATER_FLOW_REACH];

    if (*c == 0) {
        if (!passable(world_get(k->w, x, k->y, z)))
            *c = 1;
        else
            *c = drop_under(k->w, x, k->y, z) ? 3 : 2;
    }
    return *c;
}

/* Minecraft's getSlopeDistance(), as a search outward from side cell
 * (px, pz) (itself passable, not a drop): how many cells on the nearest
 * drop is, through passable cells and not back through the cell whose turn
 * it is, up to WATER_FLOW_SLOPE; 1000 if none is that near. */
static int slope(Near *k, int px, int pz)
{
    int qx[32], qz[32], qd[32], head = 0, tail = 0;
    uint8_t seen[WIN][WIN];

    memset(seen, 0, sizeof seen);
    seen[WATER_FLOW_REACH][WATER_FLOW_REACH] = 1; /* the cell itself */
    seen[pz - k->oz + WATER_FLOW_REACH][px - k->ox + WATER_FLOW_REACH] = 1;
    qx[tail] = px;
    qz[tail] = pz;
    qd[tail++] = 0;
    while (head < tail) {
        int cx = qx[head], cz = qz[head], d = qd[head++], i;

        for (i = 0; i < 4; i++) {
            int x = cx + k_side[i][0], z = cz + k_side[i][1], s;
            uint8_t *seen_it = &seen[z - k->oz + WATER_FLOW_REACH][x - k->ox + WATER_FLOW_REACH];

            if (*seen_it)
                continue;
            *seen_it = 1;
            s = near_at(k, x, z);
            if (s == 3)
                return d + 1;
            if (s == 2 && d + 1 < WATER_FLOW_SLOPE) { /* at most 1 + 4 + 8 + 12 cells */
                qx[tail] = x;
                qz[tail] = z;
                qd[tail++] = d + 1;
            }
        }
    }
    return 1000;
}

/* The sides water b at (x, y, z) runs into (Minecraft's spreadToSides()
 * and getSpread()). */
static int sides(const World *w, int x, int y, int z, int b, WaterWrite out[WATER_SPREAD_MAX])
{
    Near k;
    int cost[4], best = 1000, i, n = 0, any = 0;

    if ((b == BLOCK_FALL ? 7 : water_amount(b) - 1) <= 0)
        return 0; /* level 7 runs no further */
    for (i = 0; i < 4; i++)
        any |= takes(world_get(w, x + k_side[i][0], y, z + k_side[i][1]));
    if (!any)
        return 0; /* nowhere to run: the slopes cannot matter */
    k.w = w;
    k.ox = x;
    k.y = y;
    k.oz = z;
    memset(k.cell, 0, sizeof k.cell);
    k.cell[WATER_FLOW_REACH][WATER_FLOW_REACH] = 1;
    for (i = 0; i < 4; i++) {
        int px = x + k_side[i][0], pz = z + k_side[i][1], s = near_at(&k, px, pz);

        cost[i] = s == 1 ? -1 : s == 3 ? 0 : slope(&k, px, pz);
        if (cost[i] >= 0 && cost[i] < best)
            best = cost[i];
    }
    /* What each cell becomes is worked out before any is written. */
    for (i = 0; i < 4; i++) {
        int px = x + k_side[i][0], pz = z + k_side[i][1];

        if (cost[i] != best || !takes(world_get(w, px, y, pz)))
            continue;
        out[n].x = px;
        out[n].y = y;
        out[n].z = pz;
        out[n++].b = water_rules_new(w, px, y, pz);
    }
    return n;
}

int water_rules_spread(const World *w, int x, int y, int z, int b,
                       WaterWrite out[WATER_SPREAD_MAX])
{
    int under;

    if (!block_is_water(b))
        return 0;
    under = y > 0 ? world_get(w, x, y - 1, z) : BLOCK_STONE;
    if (takes(under)) { /* down first, as falling water */
        out[0].x = x;
        out[0].y = y - 1;
        out[0].z = z;
        out[0].b = water_rules_new(w, x, y - 1, z);
        return 1;
    }
    /* Flowing water on water stays put; a source, or water on something
     * solid, runs sideways. */
    if (b != BLOCK_WATER && block_is_water(under))
        return 0;
    return sides(w, x, y, z, b, out);
}

bool water_rules_ready(const World *w, int x, int z)
{
    int cx0 = block_to_chunk(x - WATER_FLOW_REACH), cx1 = block_to_chunk(x + WATER_FLOW_REACH);
    int cz0 = block_to_chunk(z - WATER_FLOW_REACH), cz1 = block_to_chunk(z + WATER_FLOW_REACH);
    int cx, cz;

    for (cz = cz0; cz <= cz1; cz++)
        for (cx = cx0; cx <= cx1; cx++)
            if (world_chunk(w, cx, cz) == NULL)
                return false;
    return true;
}

bool water_rules_would_change(const World *w, int x, int y, int z)
{
    WaterWrite out[WATER_SPREAD_MAX];
    int b = world_get(w, x, y, z);

    if (!block_is_water(b))
        return false;
    if (b != BLOCK_WATER && water_rules_new(w, x, y, z) != b)
        return true;
    return water_rules_spread(w, x, y, z, b, out) > 0;
}
