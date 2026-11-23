/* Pictures of the ground for checking terrain.c by eye (roadmap rows 4-01
 * and 4-02). Host tool, not part of the game:
 *
 *   gcc -O2 -Iinclude -Itools tools/terrainmap.c source/terrain.c \n *       source/terrain_ground.c source/terrain_fill.c source/biome.c \n *       source/terrain_noise.c source/terrain_spawn.c source/structure.c \n *       source/structure_templates.c source/village.c source/village_site.c \n *       source/village_apply.c source/village_cache.c source/village_ctx.c \n *       source/ruins.c source/block.c source/chunk.c -o terrainmap
 *   terrainmap <seed> <map.png> [<side.png> [<blocks per pixel>]]
 *
 * map.png: 512 x 512 blocks seen from above, centred on the spawn (red
 * cross), each column coloured by its biome and lit from the north-west so
 * slopes show. Legend (land biomes are lit by height; mountains show bare
 * stone grey and snow white; seas get darker as they deepen):
 *   plains yellow-green, forest green, birch forest pale green, dark
 *   forest very dark green, taiga blue-green, snowy plains white-blue,
 *   snowy taiga grey-blue, swamp olive, savanna yellow-brown, jungle
 *   bright green, desert pale yellow, badlands orange-red;
 *   ocean blue, warm ocean turquoise, cold ocean dark blue, frozen ocean
 *   pale ice blue, beach cream, snowy beach white, river light blue,
 *   frozen river very pale blue.
 * Villages are white-edged orange squares, ruins on land purple and ruins
 * in the sea yellow (row 4-05). Black dots mark where a column is 2 or
 * more blocks above a neighbour (cliffs).
 * side.png: the row through the highest point, seen from the side, 2
 * pixels per block up. With more than one block per pixel the map shows a
 * wider area (cliff dots then mean nothing). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "biome.h"
#include "block.h"
#include "pngout.h"
#include "ruins.h"
#include "structure.h"
#include "terrain.h"
#include "village.h"

#define SIZE 512
#define CHUNKS (SIZE / CHUNK_SIZE)

static TerrainColumn s_col[SIZE][SIZE];
static unsigned char s_rgb[SIZE * SIZE * 3];

static int clampc(int v)
{
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

/* The colour of each biome (red, green, blue), in BIOME_* order. */
static const int k_biome_rgb[BIOME_COUNT][3] = {
    { 140, 190, 80 },  /* plains */
    { 70, 150, 50 },   /* forest */
    { 130, 190, 100 }, /* birch forest */
    { 35, 95, 40 },    /* dark forest */
    { 70, 140, 120 },  /* taiga */
    { 225, 235, 245 }, /* snowy plains */
    { 150, 170, 185 }, /* snowy taiga */
    { 100, 110, 50 },  /* swamp */
    { 190, 170, 80 },  /* savanna */
    { 40, 200, 40 },   /* jungle */
    { 235, 220, 150 }, /* desert */
    { 200, 100, 50 },  /* badlands */
    { 40, 110, 220 },  /* ocean */
    { 30, 190, 200 },  /* warm ocean */
    { 30, 70, 160 },   /* cold ocean */
    { 170, 210, 235 }, /* frozen ocean */
    { 235, 215, 160 }, /* beach */
    { 250, 250, 250 }, /* snowy beach */
    { 70, 160, 240 },  /* river */
    { 200, 225, 245 }, /* frozen river */
};

static void colour(const TerrainColumn *c, int *r, int *g, int *b)
{
    int h = c->height, d = TERRAIN_SEA - h, lift = h - 64;

    *r = k_biome_rgb[c->biome][0];
    *g = k_biome_rgb[c->biome][1];
    *b = k_biome_rgb[c->biome][2];
    if (h < TERRAIN_SEA) { /* under water: deeper is darker */
        *r -= d;
        *g -= 3 * d;
        *b -= 4 * d;
        return;
    }
    if (c->kind == TERRAIN_MOUNTAIN && h > TERRAIN_SEA) {
        if (c->surface == BLOCK_SNOW)
            *r = *g = *b = 245;
        else if (c->surface == BLOCK_STONE)
            *r = *g = *b = 70 + 2 * lift;
        else
            *r += 2 * lift, *g += 2 * lift, *b += 2 * lift;
        return;
    }
    if (c->kind == TERRAIN_HILLS || c->kind == TERRAIN_PLAINS) {
        *r -= lift > 0 ? lift : 0;
        *g -= lift > 0 ? lift : 0;
        *b -= lift > 0 ? lift : 0;
    }
}

/* A square of side 2 * r + 1 pixels with a white edge, at world (wx, wz). */
static void dot(int wx, int wz, int sx, int sz, int x0, int z0, int step, int r,
                int cr, int cg, int cb)
{
    int px = step == 1 ? wx - x0 : (wx - sx) / step + SIZE / 2;
    int pz = step == 1 ? wz - z0 : (wz - sz) / step + SIZE / 2;
    int x, z;

    for (z = -r; z <= r; z++)
        for (x = -r; x <= r; x++) {
            int qx = px + x, qz = pz + z;
            unsigned char *o;

            if (qx < 0 || qx >= SIZE || qz < 0 || qz >= SIZE)
                continue;
            o = s_rgb + ((size_t)qz * SIZE + (size_t)qx) * 3;
            if (abs(x) == r || abs(z) == r) {
                o[0] = o[1] = o[2] = 255;
            } else {
                o[0] = (unsigned char)cr;
                o[1] = (unsigned char)cg;
                o[2] = (unsigned char)cb;
            }
        }
}

/* Mark every village and ruin the map covers; returns how many of each. */
static void mark_structures(uint32_t seed, int sx, int sz, int x0, int z0, int step, int n[3])
{
    const int vsize = VILLAGE_REGION_CHUNKS * CHUNK_SIZE, rsize = RUIN_REGION_CHUNKS * CHUNK_SIZE;
    int wx0 = step == 1 ? x0 : sx - SIZE / 2 * step, wz0 = step == 1 ? z0 : sz - SIZE / 2 * step;
    int rx, rz;

    for (rz = structure_floor_div(wz0, vsize); rz <= structure_floor_div(wz0 + SIZE * step, vsize); rz++)
        for (rx = structure_floor_div(wx0, vsize); rx <= structure_floor_div(wx0 + SIZE * step, vsize); rx++) {
            VillagePlan p;
            if (village_plan(seed, rx, rz, &p)) {
                dot(p.cx, p.cz, sx, sz, x0, z0, step, 4, 255, 140, 0);
                n[0]++;
            }
        }
    for (rz = structure_floor_div(wz0, rsize); rz <= structure_floor_div(wz0 + SIZE * step, rsize); rz++)
        for (rx = structure_floor_div(wx0, rsize); rx <= structure_floor_div(wx0 + SIZE * step, rsize); rx++) {
            RuinPlan p;
            if (ruin_plan(seed, rx, rz, &p)) {
                dot(p.x0 + p.width / 2, p.z0 + p.depth / 2, sx, sz, x0, z0, step, 2,
                    p.sea ? 255 : 160, p.sea ? 230 : 40, p.sea ? 0 : 220);
                n[p.sea ? 2 : 1]++;
            }
        }
}

int main(int argc, char **argv)
{
    uint32_t seed;
    int sx, sz, x0, z0, x, z, cx, cz, top = -1, top_z = 0, n_struct[3] = { 0, 0, 0 }, n_kind[TERRAIN_KIND_COUNT] = { 0 }, step;
    static TerrainColumn chunk[CHUNK_SIZE][CHUNK_SIZE];

    if (argc < 3) {
        fprintf(stderr, "usage: terrainmap <seed> <map.png> [<side.png>]\n");
        return 2;
    }
    seed = (uint32_t)strtoul(argv[1], NULL, 0);
    step = argc > 4 ? atoi(argv[4]) : 1;
    if (step < 1)
        step = 1;
    if (!terrain_find_spawn(seed, &sx, &sz))
        printf("no spawn found\n");
    printf("seed %lu spawn %d %d (height %d)\n", (unsigned long)seed, sx, sz, terrain_height(seed, sx, sz));
    /* Whole chunks, so the map lines up with them. */
    x0 = (sx - SIZE / 2) - ((sx - SIZE / 2) % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    z0 = (sz - SIZE / 2) - ((sz - SIZE / 2) % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    if (step > 1) {
        for (z = 0; z < SIZE; z++)
            for (x = 0; x < SIZE; x++)
                s_col[z][x] = terrain_column(seed, sx + (x - SIZE / 2) * step, sz + (z - SIZE / 2) * step);
        x0 = sx - SIZE / 2;
        z0 = sz - SIZE / 2;
    }
    for (cz = 0; cz < CHUNKS && step == 1; cz++)
        for (cx = 0; cx < CHUNKS; cx++) {
            terrain_chunk_columns(seed, x0 / CHUNK_SIZE + cx, z0 / CHUNK_SIZE + cz, chunk);
            for (z = 0; z < CHUNK_SIZE; z++)
                for (x = 0; x < CHUNK_SIZE; x++)
                    s_col[cz * CHUNK_SIZE + z][cx * CHUNK_SIZE + x] = chunk[z][x];
        }
    for (z = 0; z < SIZE; z++)
        for (x = 0; x < SIZE; x++) {
            const TerrainColumn *c = &s_col[z][x];
            int r, g, b, lit = 0, cliff = 0;
            unsigned char *o = s_rgb + ((size_t)z * SIZE + (size_t)x) * 3;

            colour(c, &r, &g, &b);
            n_kind[c->kind]++;
            if (c->height > top) {
                top = c->height;
                top_z = z;
            }
            if (x > 0 && z > 0 && c->height > TERRAIN_SEA)
                lit = 12 * (c->height - s_col[z - 1][x - 1].height);
            if (x > 0 && c->height - s_col[z][x - 1].height >= 2)
                cliff = 1;
            if (z > 0 && c->height - s_col[z - 1][x].height >= 2)
                cliff = 1;
            if (x + 1 < SIZE && c->height - s_col[z][x + 1].height >= 2)
                cliff = 1;
            if (z + 1 < SIZE && c->height - s_col[z + 1][x].height >= 2)
                cliff = 1;
            if (step > 1) {
                lit /= step;
                cliff = 0;
            }
            if (cliff) {
                r = g = b = 20;
                lit = 0;
            }
            o[0] = (unsigned char)clampc(r + lit);
            o[1] = (unsigned char)clampc(g + lit);
            o[2] = (unsigned char)clampc(b + lit);
        }
    for (x = -6; x <= 6; x++) {
        int px = sx - x0 + x, pz = sz - z0;

        if (px >= 0 && px < SIZE && pz >= 0 && pz < SIZE) {
            unsigned char *o = s_rgb + ((size_t)pz * SIZE + (size_t)px) * 3;
            o[0] = 255; o[1] = 0; o[2] = 0;
        }
        px = sx - x0;
        pz = sz - z0 + x;
        if (px >= 0 && px < SIZE && pz >= 0 && pz < SIZE) {
            unsigned char *o = s_rgb + ((size_t)pz * SIZE + (size_t)px) * 3;
            o[0] = 255; o[1] = 0; o[2] = 0;
        }
    }
    mark_structures(seed, sx, sz, x0, z0, step, n_struct);
    printf("villages %d, land ruins %d, sea ruins %d\n", n_struct[0], n_struct[1], n_struct[2]);
    printf("ocean %d beach %d river %d plains %d hills %d mountain %d (of %d), top %d\n",
           n_kind[0], n_kind[1], n_kind[2], n_kind[3], n_kind[4], n_kind[5], SIZE * SIZE, top);
    if (!write_png(argv[2], s_rgb, SIZE, SIZE))
        return 1;
    if (argc > 3) {
        static unsigned char side[SIZE * 256 * 3];
        int y;

        printf("side view: row z %d\n", z0 + top_z);
        for (x = 0; x < SIZE; x++) {
            const TerrainColumn *c = &s_col[top_z][x];

            for (y = 0; y < 256; y++) {
                int by = (255 - y) / 2, r = 235, g = 240, b = 250;
                unsigned char *o = side + ((size_t)y * SIZE + (size_t)x) * 3;

                if (by == c->height) {
                    colour(c, &r, &g, &b);
                    if (c->kind == TERRAIN_OCEAN || c->kind == TERRAIN_RIVER)
                        r = 225, g = 205, b = 150; /* the sea floor, not the water */
                } else if (by < c->height) {
                    r = g = b = by >= c->height - c->depth && c->under == BLOCK_DIRT ? 0 : 120;
                    if (r == 0)
                        r = 130, g = 95, b = 60;
                } else if (by <= TERRAIN_SEA) {
                    r = 60, g = 120, b = 230;
                }
                if (by % 10 == 0 && by > c->height && by > TERRAIN_SEA)
                    r = g = b = 215;
                o[0] = (unsigned char)r;
                o[1] = (unsigned char)g;
                o[2] = (unsigned char)b;
            }
        }
        if (!write_png(argv[3], side, SIZE, 256))
            return 1;
    }
    return 0;
}
