/* Pictures of the trees and plants for checking them by eye (roadmap row
 * 4-03). Host tool, not part of the game; build it with the core sources:
 *
 *   treepics <seed> <out-prefix>
 *
 * For forest, jungle, taiga, savanna, desert and a village edge it finds the
 * woodiest 64 x 64 stretch (the real world_gen_chunk() blocks) and writes
 * <prefix>-<name>-top.png (from above, 8 pixels a block, shaded by height)
 * and <prefix>-<name>-side.png (a cut along x through a tree, 10 pixels a
 * block; blocks up to 8 columns behind the cut show darker). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "biome.h"
#include "block.h"
#include "pngout.h"
#include "structure.h"
#include "terrain.h"
#include "trees.h"
#include "village.h"
#include "worldgen.h"

#define SPAN 64
#define PLANES (SPAN / CHUNK_SIZE + 2)

typedef struct {
    int x0, z0; /* south-west corner of the SPAN x SPAN square */
    uint8_t cells[WORLD_HEIGHT][SPAN][SPAN];
} Patch;

static Patch s_patch;

static void patch_make(uint32_t seed, int x0, int z0)
{
    static Chunk c;
    int cx, cz, y, z;

    s_patch.x0 = x0;
    s_patch.z0 = z0;
    for (cz = 0; cz < SPAN / CHUNK_SIZE; cz++)
        for (cx = 0; cx < SPAN / CHUNK_SIZE; cx++) {
            world_gen_chunk(&c, seed, x0 / CHUNK_SIZE + cx, z0 / CHUNK_SIZE + cz);
            for (y = 0; y < WORLD_HEIGHT; y++)
                for (z = 0; z < CHUNK_SIZE; z++)
                    memcpy(&s_patch.cells[y][cz * CHUNK_SIZE + z][cx * CHUNK_SIZE], c.blocks[y][z], CHUNK_SIZE);
        }
}

static void colour(int b, int *r, int *g, int *bl)
{
    static const int k_rgb[][4] = {
        { BLOCK_GRASS, 95, 160, 60 },        { BLOCK_DIRT, 130, 95, 60 },       { BLOCK_STONE, 120, 120, 120 },
        { BLOCK_SAND, 225, 205, 150 },       { BLOCK_WATER, 60, 120, 230 },     { BLOCK_PLANKS, 190, 150, 85 },
        { BLOCK_LOG, 100, 70, 40 },          { BLOCK_COBBLESTONE, 140, 140, 145 }, { BLOCK_SNOWY_GRASS, 240, 244, 250 },
        { BLOCK_SNOW, 240, 244, 250 },       { BLOCK_ICE, 150, 196, 238 },      { BLOCK_SANDSTONE, 212, 195, 144 },
        { BLOCK_BIRCH_LOG, 225, 220, 205 },  { BLOCK_SPRUCE_LOG, 70, 50, 30 },  { BLOCK_DARK_OAK_LOG, 50, 35, 20 },
        { BLOCK_JUNGLE_LOG, 110, 85, 55 },   { BLOCK_ACACIA_LOG, 150, 100, 80 }, { BLOCK_OAK_LEAVES, 70, 130, 40 },
        { BLOCK_BIRCH_LEAVES, 130, 170, 90 }, { BLOCK_SPRUCE_LEAVES, 40, 85, 60 }, { BLOCK_DARK_OAK_LEAVES, 35, 85, 35 },
        { BLOCK_JUNGLE_LEAVES, 40, 150, 25 }, { BLOCK_ACACIA_LEAVES, 110, 150, 40 }, { BLOCK_CACTUS, 30, 120, 40 },
        { BLOCK_SHORT_GRASS, 120, 190, 70 }, { BLOCK_FERN, 50, 140, 70 },       { BLOCK_DANDELION, 250, 225, 40 },
        { BLOCK_POPPY, 225, 30, 30 },        { BLOCK_OXEYE_DAISY, 255, 255, 255 }, { BLOCK_BLUE_ORCHID, 40, 190, 230 },
        { BLOCK_DEAD_BUSH, 140, 95, 50 },
    };
    size_t i;

    for (i = 0; i < sizeof k_rgb / sizeof k_rgb[0]; i++)
        if (k_rgb[i][0] == b) {
            *r = k_rgb[i][1];
            *g = k_rgb[i][2];
            *bl = k_rgb[i][3];
            return;
        }
    if (b >= BLOCK_TERRACOTTA && b <= BLOCK_TERRACOTTA_BROWN) {
        *r = 152; *g = 94; *bl = 67;
        return;
    }
    *r = 225; *g = 235; *bl = 250; /* air: sky */
}

static void px(unsigned char *img, int w, int x, int y, int r, int g, int b)
{
    unsigned char *o = img + ((size_t)y * w + x) * 3;

    o[0] = (unsigned char)(r < 0 ? 0 : r > 255 ? 255 : r);
    o[1] = (unsigned char)(g < 0 ? 0 : g > 255 ? 255 : g);
    o[2] = (unsigned char)(b < 0 ? 0 : b > 255 ? 255 : b);
}

static void fill(unsigned char *img, int w, int x, int y, int s, int r, int g, int b)
{
    int i, j;

    for (j = 0; j < s; j++)
        for (i = 0; i < s; i++)
            px(img, w, x * s + i, y * s + j, r, g, b);
}

static void top_view(const char *path)
{
    const int S = 8;
    unsigned char *img = calloc((size_t)SPAN * S * SPAN * S, 3);
    int x, z, y;

    for (z = 0; z < SPAN; z++)
        for (x = 0; x < SPAN; x++) {
            int r, g, b, sh;

            for (y = WORLD_HEIGHT - 1; y > 0 && s_patch.cells[y][z][x] == BLOCK_AIR; y--)
                ;
            colour(s_patch.cells[y][z][x], &r, &g, &b);
            sh = (y - 70) * 3;
            fill(img, SPAN * S, x, z, S, r + sh, g + sh, b + sh);
            if (block_is_plant(s_patch.cells[y][z][x])) { /* a plant: ground colour round a small dot */
                int gr, gg, gb;

                colour(s_patch.cells[y - 1][z][x], &gr, &gg, &gb);
                fill(img, SPAN * S, x, z, S, gr + sh, gg + sh, gb + sh);
                px(img, SPAN * S, x * S + 3, z * S + 3, r, g, b);
                px(img, SPAN * S, x * S + 4, z * S + 3, r, g, b);
                px(img, SPAN * S, x * S + 3, z * S + 4, r, g, b);
                px(img, SPAN * S, x * S + 4, z * S + 4, r, g, b);
            }
        }
    write_png(path, img, SPAN * S, SPAN * S);
    free(img);
}

/* Cut along x at column z_cut, looking towards +z; y from y_lo to y_hi. */
static void side_view(const char *path, int z_cut, int y_lo, int y_hi)
{
    const int S = 10;
    int H = y_hi - y_lo + 1, x, y, d;
    unsigned char *img = calloc((size_t)SPAN * S * H * S, 3);

    for (y = y_hi; y >= y_lo; y--)
        for (x = 0; x < SPAN; x++) {
            int r = 225, g = 235, b = 250, depth = 0;

            for (d = 0; d < 8 && z_cut + d < SPAN; d++)
                if (s_patch.cells[y][z_cut + d][x] != BLOCK_AIR) {
                    colour(s_patch.cells[y][z_cut + d][x], &r, &g, &b);
                    depth = d;
                    break;
                }
            fill(img, SPAN * S, x, y_hi - y, S, r - depth * 14, g - depth * 14, b - depth * 14);
        }
    write_png(path, img, SPAN * S, H * S);
    free(img);
}

typedef int (*Want)(int biome);

static int is_forest(int b) { return b == BIOME_FOREST || b == BIOME_BIRCH_FOREST || b == BIOME_DARK_FOREST; }
static int is_jungle(int b) { return b == BIOME_JUNGLE; }
static int is_taiga(int b) { return b == BIOME_TAIGA || b == BIOME_SNOWY_TAIGA; }
static int is_savanna(int b) { return b == BIOME_SAVANNA; }
static int is_desert(int b) { return b == BIOME_DESERT; }

/* The SPAN x SPAN square (corners on multiples of 16 over -1024 to 1024)
 * with the most trees whose trunk biome fits, and whose centre fits. */
static int best_square(uint32_t seed, Want want, int *bx, int *bz)
{
    static Tree t[4096];
    int x, z, best = -1, i;

    for (z = -1024; z < 1024; z += 32)
        for (x = -1024; x < 1024; x += 32) {
            int n = trees_in_area(seed, x, z, SPAN, SPAN, t, 4096), fit = 0;

            if (!want(terrain_column(seed, x + SPAN / 2, z + SPAN / 2).biome))
                continue;
            for (i = 0; i < n; i++)
                fit += want(terrain_column(seed, t[i].x, t[i].z).biome);
            if (fit > best) {
                best = fit;
                *bx = x;
                *bz = z;
            }
        }
    return best;
}

/* The highest ground and the first tree trunk row (a log) in the patch. */
static void side_choice(int *z_cut, int *y_lo, int *y_hi)
{
    int x, y, z, top = 0, best_z = SPAN / 2, best_n = -1;

    for (z = 0; z < SPAN - 8; z++) {
        int logs = 0;

        for (x = 0; x < SPAN; x++)
            for (y = 0; y < WORLD_HEIGHT; y++) {
                int b = s_patch.cells[y][z][x];

                logs += (b >= BLOCK_BIRCH_LOG && b <= BLOCK_ACACIA_LOG) || b == BLOCK_LOG || b == BLOCK_CACTUS;
            }
        if (logs > best_n) {
            best_n = logs;
            best_z = z;
        }
    }
    for (z = 0; z < SPAN; z++)
        for (x = 0; x < SPAN; x++)
            for (y = 0; y < WORLD_HEIGHT; y++) {
                if (s_patch.cells[y][z][x] != BLOCK_AIR && y > top)
                    top = y;
            }
    *z_cut = best_z;
    *y_hi = top;
    *y_lo = TERRAIN_SEA - 6 < top - 40 ? top - 40 : TERRAIN_SEA - 6;
}

static void make(uint32_t seed, const char *prefix, const char *name, int x0, int z0)
{
    char path[512];
    int z_cut, y_lo, y_hi;

    patch_make(seed, x0, z0);
    snprintf(path, sizeof path, "%s-%s-top.png", prefix, name);
    top_view(path);
    side_choice(&z_cut, &y_lo, &y_hi);
    snprintf(path, sizeof path, "%s-%s-side.png", prefix, name);
    side_view(path, z_cut, y_lo, y_hi);
    printf("%-8s from (%d, %d), side cut at z +%d, y %d..%d\n", name, x0, z0, z_cut, y_lo, y_hi);
}

int main(int argc, char **argv)
{
    static const struct { const char *name; Want want; } k_sites[] = {
        { "forest", is_forest }, { "jungle", is_jungle }, { "taiga", is_taiga },
        { "savanna", is_savanna }, { "desert", is_desert },
    };
    uint32_t seed;
    size_t i;
    int rx, rz, vx = 0, vz = 0, vbest = -1;
    static Tree t[4096];

    if (argc < 3) {
        fprintf(stderr, "usage: treepics <seed> <out-prefix>\n");
        return 1;
    }
    seed = (uint32_t)strtoul(argv[1], NULL, 10);
    for (i = 0; i < sizeof k_sites / sizeof k_sites[0]; i++) {
        int bx = 0, bz = 0, n = best_square(seed, k_sites[i].want, &bx, &bz);

        if (n < 0) {
            printf("%-8s none found for seed %lu\n", k_sites[i].name, (unsigned long)seed);
            continue;
        }
        printf("(%d trees) ", n);
        make(seed, argv[2], k_sites[i].name, bx, bz);
    }
    /* A village edge: the village with most trees round it; the square's
     * west edge sits 24 blocks inside the village's east side. */
    for (rz = -6; rz <= 6; rz++)
        for (rx = -6; rx <= 6; rx++) {
            const VillagePlan *v = village_get(seed, rx, rz);
            int n;

            if (v == NULL)
                continue;
            n = trees_in_area(seed, v->x1 - 24, v->z0, SPAN, SPAN, t, 4096);
            if (n > vbest) {
                vbest = n;
                vx = v->x1 - 24;
                vz = v->z0;
            }
        }
    if (vbest >= 0) {
        printf("(%d trees) ", vbest);
        make(seed, argv[2], "village", structure_floor_div(vx, CHUNK_SIZE) * CHUNK_SIZE, structure_floor_div(vz, CHUNK_SIZE) * CHUNK_SIZE);
    } else {
        printf("village  none found\n");
    }
    return 0;
}
