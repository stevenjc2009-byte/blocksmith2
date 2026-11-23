/* Pictures of villages and ruins for checking them by eye (roadmap row
 * 4-05). Host tool, not part of the game:
 *
 *   gcc -O2 -Iinclude -Itools tools/structpics.c source/terrain.c \
 *       source/terrain_ground.c source/terrain_fill.c source/biome.c \
 *       source/terrain_noise.c source/terrain_spawn.c source/structure.c \
 *       source/structure_templates.c source/village.c source/village_site.c \
 *       source/village_apply.c source/village_cache.c source/village_ctx.c \
 *       source/ruins.c source/block.c source/chunk.c -o structpics
 *   structpics <seed> <top.png> <side.png>
 *
 * Prints where the nearest village and the nearest land and sea ruins are
 * to the spawn. top.png: the village nearest the spawn from above, 5
 * pixels a block, each column the colour of its top block (white-edged
 * where it is a path). side.png: a cut through the middle of the village's
 * house on the steepest ground, from the side, 14 pixels a block. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "pngout.h"
#include "ruins.h"
#include "structure.h"
#include "structure_templates.h"
#include "terrain.h"
#include "village.h"

typedef struct {
    uint8_t *cells;
    StructCanvas cv;
} Area;

static void area_make(Area *a, uint32_t seed, int x0, int z0, int x1, int z1)
{
    int cx0 = structure_floor_div(x0, CHUNK_SIZE), cz0 = structure_floor_div(z0, CHUNK_SIZE);
    int nx = structure_floor_div(x1, CHUNK_SIZE) - cx0 + 1, nz = structure_floor_div(z1, CHUNK_SIZE) - cz0 + 1;
    int w = nx * CHUNK_SIZE, d = nz * CHUNK_SIZE, cx, cz, y, z;
    static Chunk c;

    a->cells = malloc((size_t)w * d * WORLD_HEIGHT);
    a->cv.cells = a->cells;
    a->cv.x0 = cx0 * CHUNK_SIZE;
    a->cv.z0 = cz0 * CHUNK_SIZE;
    a->cv.w = w;
    a->cv.d = d;
    for (cz = 0; cz < nz; cz++)
        for (cx = 0; cx < nx; cx++) {
            terrain_fill_chunk(&c, seed, cx0 + cx, cz0 + cz);
            for (y = 0; y < WORLD_HEIGHT; y++)
                for (z = 0; z < CHUNK_SIZE; z++)
                    memcpy(&a->cells[((size_t)y * d + cz * CHUNK_SIZE + z) * w + cx * CHUNK_SIZE],
                           c.blocks[y][z], CHUNK_SIZE);
        }
    structure_apply_canvas(&a->cv, seed);
}

static void block_colour(int b, int *r, int *g, int *bl)
{
    switch (b) {
    case BLOCK_GRASS: *r = 95; *g = 160; *bl = 60; break;
    case BLOCK_DIRT: *r = 130; *g = 95; *bl = 60; break;
    case BLOCK_STONE: *r = 120; *g = 120; *bl = 120; break;
    case BLOCK_SAND: *r = 225; *g = 205; *bl = 150; break;
    case BLOCK_WATER: *r = 60; *g = 120; *bl = 230; break;
    case BLOCK_TORCH: *r = 255; *g = 220; *bl = 60; break;
    case BLOCK_PLANKS: *r = 190; *g = 150; *bl = 85; break;
    case BLOCK_LOG: *r = 100; *g = 70; *bl = 40; break;
    case BLOCK_COBBLESTONE: *r = 140; *g = 140; *bl = 145; break;
    case BLOCK_MOSSY_COBBLESTONE: *r = 90; *g = 130; *bl = 90; break;
    case BLOCK_SNOWY_GRASS: case BLOCK_SNOW: *r = 240; *g = 244; *bl = 250; break;
    case BLOCK_ICE: *r = 150; *g = 196; *bl = 238; break;
    case BLOCK_SANDSTONE: *r = 212; *g = 195; *bl = 144; break;
    case BLOCK_RED_SAND: *r = 190; *g = 102; *bl = 44; break;
    case BLOCK_GRAVEL: *r = 125; *g = 118; *bl = 110; break;
    case BLOCK_TERRACOTTA: case BLOCK_TERRACOTTA_WHITE: case BLOCK_TERRACOTTA_YELLOW:
    case BLOCK_TERRACOTTA_RED: case BLOCK_TERRACOTTA_BROWN: *r = 152; *g = 94; *bl = 67; break;
    default: *r = 225; *g = 235; *bl = 250; break; /* air */
    }
}

static int top_y(const Area *a, int x, int z)
{
    int y;

    for (y = WORLD_HEIGHT - 1; y > 0 && structure_canvas_get(&a->cv, x, y, z) == BLOCK_AIR; y--)
        ;
    return y;
}

static void put_px(unsigned char *img, int w, int x, int y, int r, int g, int b)
{
    unsigned char *o = img + ((size_t)y * w + x) * 3;

    o[0] = (unsigned char)(r < 0 ? 0 : r > 255 ? 255 : r);
    o[1] = (unsigned char)(g < 0 ? 0 : g > 255 ? 255 : g);
    o[2] = (unsigned char)(b < 0 ? 0 : b > 255 ? 255 : b);
}

static void top_view(uint32_t seed, const VillagePlan *p, const char *path)
{
    const int S = 5;
    Area a;
    int x0 = p->x0 - 4, z0 = p->z0 - 4, W = p->x1 - p->x0 + 9, D = p->z1 - p->z0 + 9, x, z, k;
    unsigned char *img = calloc((size_t)W * S * D * S, 3);

    area_make(&a, seed, x0, z0, x0 + W - 1, z0 + D - 1);
    for (z = 0; z < D; z++)
        for (x = 0; x < W; x++) {
            int y = top_y(&a, x0 + x, z0 + z), b = structure_canvas_get(&a.cv, x0 + x, y, z0 + z);
            int r, g, bl, sh = (y - 64) * 4, px, pz;

            block_colour(b, &r, &g, &bl);
            for (pz = 0; pz < S; pz++)
                for (px = 0; px < S; px++)
                    put_px(img, W * S, x * S + px, z * S + pz, r + sh, g + sh, bl + sh);
        }
    for (k = 0; k < p->npath; k++) /* a pale dot on every path column */
        put_px(img, W * S, (p->path[k].x - x0) * S + 2, (p->path[k].z - z0) * S + 2, 255, 255, 255);
    write_png(path, img, W * S, D * S);
    printf("top view: %d x %d blocks from (%d, %d)\n", W, D, x0, z0);
    free(img);
    free(a.cells);
}

/* The house or hut of the plan on the steepest ground, and how steep. */
static const VillageBuilding *steepest(const VillagePlan *p, int *spread_out)
{
    const VillageBuilding *best = NULL;
    int i, spread = -1;

    for (i = 0; i < p->nbuild; i++) {
        const VillageBuilding *b = &p->b[i];
        int k, lo = 999, hi = 0;

        if (b->tmpl != TMPL_HOUSE_SMALL && b->tmpl != TMPL_HOUSE_LARGE && b->tmpl != TMPL_HUT)
            continue;
        for (k = 0; k < b->width * b->depth; k++) {
            lo = b->gh[k] < lo ? b->gh[k] : lo;
            hi = b->gh[k] > hi ? b->gh[k] : hi;
        }
        if (hi - lo > spread) {
            spread = hi - lo;
            best = b;
        }
    }
    *spread_out = spread;
    return best;
}

static void side_view(uint32_t seed, const VillagePlan *p, const char *path)
{
    const int S = 14;
    int spread;
    const VillageBuilding *best = steepest(p, &spread);

    if (best == NULL)
        return;
    {
        const Template *t = template_get(best->tmpl);
        Area a;
        int x0 = best->x0 - 6, W = best->width + 12, z = best->z0 + best->depth / 2;
        int y0 = best->fy - 6, H = t->h + 12, x, y;
        unsigned char *img = calloc((size_t)W * S * H * S, 3);

        area_make(&a, seed, x0, z - 1, x0 + W - 1, z + 1);
        for (y = 0; y < H; y++)
            for (x = 0; x < W; x++) {
                int b = structure_canvas_get(&a.cv, x0 + x, y0 + H - 1 - y, z), r, g, bl, px, pz;

                block_colour(b, &r, &g, &bl);
                for (pz = 0; pz < S; pz++)
                    for (px = 0; px < S; px++) {
                        int edge = (px == 0 || pz == 0) && b != BLOCK_AIR ? -40 : 0;
                        put_px(img, W * S, x * S + px, y * S + pz, r + edge, g + edge, bl + edge);
                    }
            }
        write_png(path, img, W * S, H * S);
        printf("side view: template %s, ground spread %d, row z %d, x %d .. %d, floor y %d\n",
               t->name, spread, z, x0, x0 + W - 1, best->fy);
        free(img);
        free(a.cells);
    }
}

int main(int argc, char **argv)
{
    uint32_t seed;
    int sx, sz, rx, rz, bestd = 1 << 30, kinds;
    VillagePlan best, p, steep;
    int steep_spread = -1, sp;

    if (argc < 4) {
        fprintf(stderr, "usage: structpics <seed> <top.png> <side.png>\n");
        return 2;
    }
    seed = (uint32_t)strtoul(argv[1], NULL, 0);
    if (!terrain_find_spawn(seed, &sx, &sz))
        return 1;
    printf("seed %lu spawn %d %d\n", (unsigned long)seed, sx, sz);
    memset(&best, 0, sizeof best);
    memset(&steep, 0, sizeof steep);
    for (rz = -4; rz < 4; rz++)
        for (rx = -4; rx < 4; rx++)
            if (village_plan(seed, rx, rz, &p)) {
                int d = abs(p.cx - sx) + abs(p.cz - sz);
                steepest(&p, &sp);
                if (sp > steep_spread) {
                    steep_spread = sp;
                    steep = p;
                }
                if (d < bestd) {
                    bestd = d;
                    best = p;
                }
            }
    for (kinds = 0; kinds < 3; kinds++) { /* 0: village, 1: land ruin, 2: sea ruin */
        int bd = 1 << 30, bx = 0, bz = 0, gh = 0, any = 0;

        if (kinds == 0 && best.valid) {
            bx = best.cx; bz = best.cz; gh = terrain_height(seed, bx, bz); any = 1;
        }
        for (rz = -8; rz < 8 && kinds; rz++)
            for (rx = -8; rx < 8; rx++) {
                RuinPlan r;
                if (ruin_plan(seed, rx, rz, &r) && r.sea == (kinds == 2)) {
                    int cx = r.x0 + r.width / 2, cz = r.z0 + r.depth / 2, d = abs(cx - sx) + abs(cz - sz);
                    if (d < bd) {
                        bd = d; bx = cx; bz = cz; gh = r.gh[r.depth / 2 * r.width + r.width / 2]; any = 1;
                    }
                }
            }
        if (any)
            printf("nearest %s: (%d, %d) ground %d, %d blocks from the spawn; to walk there from the "
                   "spawn go %s %d and %s %d\n",
                   kinds == 0 ? "village" : kinds == 1 ? "land ruin" : "sea ruin", bx, gh == 0 ? 0 : bz,
                   gh, abs(bx - sx) + abs(bz - sz), bx < sx ? "west (-x)" : "east (+x)", abs(bx - sx),
                   bz < sz ? "north (-z)" : "south (+z)", abs(bz - sz));
    }
    if (best.valid) {
        top_view(seed, &best, argv[2]);
        side_view(seed, steep.valid ? &steep : &best, argv[3]);
    }
    return 0;
}
