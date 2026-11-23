#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "light.h"
#include "ruins.h"
#include "structure.h"
#include "structure_templates.h"
#include "terrain.h"
#include "test.h"
#include "village.h"
#include "worldgen.h"

/* Row 4-05: villages and ruins as blocks. Each check builds the ground of an
 * area (terrain_fill_chunk()), puts the structures on it, and looks at the
 * blocks like a player would: doors you can walk through, nothing floating,
 * nothing buried, no air under the sea. */

static const uint32_t k_seeds[] = { 2026u, 1u, 77u, 12345u, 999u, 31337u };
#define N_SEEDS ((int)(sizeof k_seeds / sizeof k_seeds[0]))

typedef struct {
    uint8_t *cells;
    StructCanvas cv;
} Area;

static int fdiv(int a, int b) { return structure_floor_div(a, b); }

/* The ground of every chunk touching blocks x0..x1, z0..z1 (structures on
 * top if asked). Free with area_free(). */
static void area_make(Area *a, uint32_t seed, int x0, int z0, int x1, int z1, bool structures)
{
    int cx0 = fdiv(x0, CHUNK_SIZE), cz0 = fdiv(z0, CHUNK_SIZE);
    int nx = fdiv(x1, CHUNK_SIZE) - cx0 + 1, nz = fdiv(z1, CHUNK_SIZE) - cz0 + 1;
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
    if (structures) {
        structure_cache_clear();
        structure_apply_canvas(&a->cv, seed);
    }
}

static void area_free(Area *a) { free(a->cells); }

static int at(const Area *a, int x, int y, int z) { return structure_canvas_get(&a->cv, x, y, z); }

static bool dry_land(uint32_t seed, int x, int z)
{
    TerrainKind k = terrain_column(seed, x, z).kind;
    return k == TERRAIN_PLAINS || k == TERRAIN_HILLS;
}

/* The nth (from 0) village found in regions -2 .. 1 round the origin. */
static bool find_village(uint32_t seed, int n, VillagePlan *out, int *rx_out, int *rz_out)
{
    int rx, rz;

    for (rz = -2; rz < 2; rz++)
        for (rx = -2; rx < 2; rx++)
            if (village_plan(seed, rx, rz, out) && n-- == 0) {
                *rx_out = rx;
                *rz_out = rz;
                return true;
            }
    return false;
}

static int is_stone_block(int b)
{
    return b == BLOCK_COBBLESTONE || b == BLOCK_MOSSY_COBBLESTONE;
}

void test_check_village(uint32_t seed, const VillagePlan *p)
{
    Area a;
    int i, k, y, bad_air = 0, doors = 0, houses_air = 0, bad = 0;

    area_make(&a, seed, p->x0 - 2, p->z0 - 2, p->x1 + 2, p->z1 + 2, true);
    /* paths: at ground height, the path block, air above, on dry land */
    for (k = 0; k < p->npath; k++) {
        const VillagePathCol *c = &p->path[k];
        bad += at(&a, c->x, c->y, c->z) != style_path_block(p->style);
        bad += c->y != terrain_height(seed, c->x, c->z);
        bad += at(&a, c->x, c->y + 1, c->z) != BLOCK_AIR || at(&a, c->x, c->y + 2, c->z) != BLOCK_AIR;
        bad += !dry_land(seed, c->x, c->z);
    }
    CHECK_EQ_INT(bad, 0);
    bad = 0;
    for (i = 0; i < p->nbuild; i++) {
        const VillageBuilding *b = &p->b[i];
        const Template *t = template_get(b->tmpl);
        int cx[4] = { b->x0, b->x0 + b->width - 1, b->x0, b->x0 + b->width - 1 };
        int cz[4] = { b->z0, b->z0, b->z0 + b->depth - 1, b->z0 + b->depth - 1 };
        int x, z, W, D;

        struct_footprint(t->w, t->d, b->rot, &W, &D);
        CHECK_EQ_INT(W == b->width && D == b->depth, 1);
        /* on dry land, ground not far from the floor, nothing below the floor open */
        for (z = 0; z < D; z++)
            for (x = 0; x < W; x++) {
                int gh = terrain_height(seed, b->x0 + x, b->z0 + z);
                bad += !dry_land(seed, b->x0 + x, b->z0 + z);
                bad += b->gh[z * W + x] != gh;
                bad += abs(gh - b->fy) > VILLAGE_FIT;
                for (y = gh < b->fy ? gh : b->fy; y <= b->fy; y++)
                    bad += !block_solid(at(&a, b->x0 + x, y, b->z0 + z)) &&
                           at(&a, b->x0 + x, y, b->z0 + z) != BLOCK_WATER;
            }
        /* above the floor, every cell the template leaves open (or does not
         * reach, up to the cleared height) is air: no hillside through a room or roof */
        for (z = 0; z < D; z++)
            for (x = 0; x < W; x++) {
                int ax, az, yy;
                struct_unmap(t->w, t->d, b->rot, b->mirror, x, z, &ax, &az);
                for (yy = b->fy + 1; yy <= b->clear_top; yy++) {
                    StructPart part = yy - b->fy < t->h ? template_part(t, ax, yy - b->fy, az) : PART_AIR;
                    if (part == PART_AIR || part == PART_SKIP)
                        bad += at(&a, b->x0 + x, yy, b->z0 + z) != BLOCK_AIR;
                }
            }
        /* corners never float: solid from the ground to the floor */
        for (k = 0; k < 4; k++) {
            int gh = terrain_height(seed, cx[k], cz[k]);
            for (y = gh < b->fy ? gh : b->fy; y <= b->fy; y++)
                bad += !block_solid(at(&a, cx[k], y, cz[k]));
        }
        if (t->door_x >= 0) { /* a doorway you can walk through, from its path */
            int lx, lz, ox, oz, onpath = 0;

            struct_map(t->w, t->d, b->rot, b->mirror, t->door_x, t->d - 1, &lx, &lz);
            CHECK_EQ_INT(village_door_outside(b, &ox, &oz), 1);
            for (k = 0; k < p->npath; k++)
                onpath |= p->path[k].x == ox && p->path[k].z == oz && p->path[k].y == b->fy;
            CHECK_EQ_INT(onpath, 1);
            CHECK_EQ_INT(at(&a, ox, b->fy, oz), style_path_block(p->style));
            CHECK_EQ_INT(at(&a, b->x0 + lx, b->fy, b->z0 + lz) != BLOCK_AIR, 1); /* floor */
            CHECK_EQ_INT(at(&a, b->x0 + lx, b->fy + 1, b->z0 + lz), BLOCK_AIR);
            CHECK_EQ_INT(at(&a, b->x0 + lx, b->fy + 2, b->z0 + lz), BLOCK_AIR);
            CHECK_EQ_INT(at(&a, ox, b->fy + 1, oz), BLOCK_AIR);
            CHECK_EQ_INT(at(&a, ox, b->fy + 2, oz), BLOCK_AIR);
            doors++;
        }
        if (b->tmpl == TMPL_HOUSE_SMALL || b->tmpl == TMPL_HOUSE_LARGE || b->tmpl == TMPL_HUT) {
            int air = 0;
            for (z = 1; z < D - 1; z++)
                for (x = 1; x < W - 1; x++)
                    air += at(&a, b->x0 + x, b->fy + 1, b->z0 + z) == BLOCK_AIR;
            houses_air += air >= 3;
            bad_air += air < 3;
        }
    }
    CHECK_EQ_INT(bad, 0);
    CHECK_EQ_INT(bad_air, 0);
    CHECK_EQ_INT(doors >= 3, 1);
    CHECK_EQ_INT(houses_air >= 3, 1);
    area_free(&a);
}

static void check_villages(void)
{
    int s, n, total = 0;

    for (s = 0; s < N_SEEDS; s++)
        for (n = 0; n < 2; n++) {
            VillagePlan p;
            int rx, rz;
            if (find_village(k_seeds[s], n, &p, &rx, &rz)) {
                test_check_village(k_seeds[s], &p);
                total++;
            }
        }
    CHECK_EQ_INT(total >= 8, 1);
}

static void check_ruin(uint32_t seed, const RuinPlan *p)
{
    Area a;
    int x, z, y, stones = 0, bad = 0, air_under_sea = 0, floating = 0;

    area_make(&a, seed, p->x0 - 8, p->z0 - 8, p->x0 + p->width + 8, p->z0 + p->depth + 8, true);
    for (z = 0; z < p->depth; z++)
        for (x = 0; x < p->width; x++) {
            TerrainColumn col = terrain_column(seed, p->x0 + x, p->z0 + z);
            bad += p->sea ? col.kind != TERRAIN_OCEAN : !dry_land(seed, p->x0 + x, p->z0 + z);
            bad += p->gh[z * p->width + x] != col.height;
        }
    for (z = a.cv.z0; z < a.cv.z0 + a.cv.d; z++)
        for (x = a.cv.x0; x < a.cv.x0 + a.cv.w; x++) {
            int h = terrain_height(seed, x, z);
            for (y = h + 1; h < TERRAIN_SEA && y <= TERRAIN_SEA; y++)
                air_under_sea += at(&a, x, y, z) == BLOCK_AIR;
            for (y = 1; y < WORLD_HEIGHT; y++)
                if (is_stone_block(at(&a, x, y, z))) {
                    stones++;
                    floating += !block_solid(at(&a, x, y - 1, z));
                }
        }
    CHECK_EQ_INT(bad, 0);
    CHECK_EQ_INT(air_under_sea, 0);
    if (p->sea)
        for (x = 0; x < p->width * p->depth; x++)
            CHECK_EQ_INT(p->gh[x] <= 54 /* written out on purpose: the limit itself must not move */, 1);
    CHECK_EQ_INT(floating, 0);
    CHECK_EQ_INT(stones >= 6, 1);
    area_free(&a);
}

static void check_ruins(void)
{
    int s, rx, rz, land = 0, sea = 0;

    for (s = 0; s < N_SEEDS; s++)
        for (rz = -3; rz < 3; rz++)
            for (rx = -3; rx < 3; rx++) {
                RuinPlan p;
                if (!ruin_plan(k_seeds[s], rx, rz, &p) || (p.sea ? sea >= 6 : land >= 8))
                    continue;
                check_ruin(k_seeds[s], &p);
                sea += p.sea;
                land += !p.sea;
            }
    CHECK_EQ_INT(land >= 8 && sea >= 4, 1);
}

/* Chunk by chunk in two orders, and the whole area at once, give the same
 * blocks; and the structures really are in there. */
static void check_seamless(uint32_t seed)
{
    VillagePlan p;
    Area whole, bare;
    int rx, rz, order, cx, cz, cx0, cz0, nx, nz, diff = 0, changed = 0, i;
    static Chunk c;

    if (!find_village(seed, 0, &p, &rx, &rz))
        return;
    area_make(&whole, seed, p.x0 - 20, p.z0 - 20, p.x1 + 20, p.z1 + 20, true);
    area_make(&bare, seed, p.x0 - 20, p.z0 - 20, p.x1 + 20, p.z1 + 20, false);
    for (i = 0; i < whole.cv.w * whole.cv.d * WORLD_HEIGHT; i++)
        changed += whole.cells[i] != bare.cells[i];
    CHECK_EQ_INT(changed > 500, 1);
    cx0 = whole.cv.x0 / CHUNK_SIZE;
    cz0 = whole.cv.z0 / CHUNK_SIZE;
    nx = whole.cv.w / CHUNK_SIZE;
    nz = whole.cv.d / CHUNK_SIZE;
    for (order = 0; order < 2; order++) {
        structure_cache_clear();
        for (i = 0; i < nx * nz; i++) {
            int n = order ? nx * nz - 1 - i : i, y, z;
            cx = n % nx;
            cz = n / nx;
            if (i == nx * nz / 2)
                structure_cache_clear(); /* a cold cache half way */
            world_gen_chunk(&c, seed, cx0 + cx, cz0 + cz);
            for (y = 0; y < WORLD_HEIGHT; y++)
                for (z = 0; z < CHUNK_SIZE; z++)
                    diff += memcmp(c.blocks[y][z],
                                   &whole.cells[((size_t)y * whole.cv.d + cz * CHUNK_SIZE + z) *
                                                    whole.cv.w + cx * CHUNK_SIZE], CHUNK_SIZE) != 0;
        }
    }
    CHECK_EQ_INT(diff, 0);
    area_free(&whole);
    area_free(&bare);
}

/* The spawn column is never inside a wall or under a roof: its chunk's
 * blocks there are the bare ground's. And a village or ruin planned with the
 * spawn on top of it gives way. */
static void check_spawn(void)
{
    int s, rx, rz, moved = 0, tried = 0;
    static Chunk with, bare;

    for (s = 0; s < N_SEEDS + 20; s++) {
        uint32_t seed = s < N_SEEDS ? k_seeds[s] : (uint32_t)(5000 + s);
        int sx, sz, y;

        if (!structure_spawn(seed, &sx, &sz))
            continue;
        world_gen_chunk(&with, seed, fdiv(sx, CHUNK_SIZE), fdiv(sz, CHUNK_SIZE));
        terrain_fill_chunk(&bare, seed, fdiv(sx, CHUNK_SIZE), fdiv(sz, CHUNK_SIZE));
        for (y = 0; y < WORLD_HEIGHT; y++)
            CHECK_EQ_INT(with.blocks[y][sz & 15][sx & 15], bare.blocks[y][sz & 15][sx & 15]);
    }
    for (s = 0; s < N_SEEDS; s++) {
        VillagePlan p, q;
        int i;
        if (!find_village(k_seeds[s], 0, &p, &rx, &rz))
            continue;
        for (i = 0; i < p.nbuild; i++) {
            const VillageBuilding *b = &p.b[i];
            int sx = b->x0 + b->width / 2, sz = b->z0 + b->depth / 2, j, covered = 0;

            village_plan_spawn(k_seeds[s], rx, rz, true, sx, sz, &q);
            for (j = 0; q.valid && j < q.nbuild; j++)
                covered |= sx >= q.b[j].x0 - 2 && sx < q.b[j].x0 + q.b[j].width + 2 &&
                           sz >= q.b[j].z0 - 2 && sz < q.b[j].z0 + q.b[j].depth + 2;
            CHECK_EQ_INT(covered, 0);
            moved += memcmp(&p, &q, sizeof p) != 0;
            tried++;
        }
        {   /* the spawn right at the middle: no village there, or a different one */
            village_plan_spawn(k_seeds[s], rx, rz, true, p.cx, p.cz, &q);
            CHECK_EQ_INT(q.valid && abs(q.cx - p.cx) < 7 && abs(q.cz - p.cz) < 7, 0);
        }
    }
    CHECK_EQ_INT(moved, tried);
    {
        int found = 0, covered = 0;
        for (s = 0; s < N_SEEDS; s++)
            for (rz = -3; rz < 3; rz++)
                for (rx = -3; rx < 3; rx++) {
                    RuinPlan r, q;
                    if (!ruin_plan(k_seeds[s], rx, rz, &r))
                        continue;
                    ruin_plan_spawn(k_seeds[s], rx, rz, true, r.x0 + r.width / 2,
                                    r.z0 + r.depth / 2, &q);
                    found++;
                    covered += q.valid && memcmp(&q, &r, sizeof q) == 0;
                }
        CHECK_EQ_INT(found > 10, 1);
        CHECK_EQ_INT(covered, 0);
    }
}

/* A lamp post on a chunk edge lights the cells on both sides of it. */
static World s_world;

static int check_lamp(uint32_t seed, const VillageBuilding *b)
{
    int x = b->x0, z = b->z0, y = b->fy + 3, cx = fdiv(x, CHUNK_SIZE), cz = fdiv(z, CHUNK_SIZE);
    int dx = (x & 15) == 0 ? -1 : (x & 15) == 15 ? 1 : 0;
    int dz = (z & 15) == 0 ? -1 : (z & 15) == 15 ? 1 : 0;
    int i, j;

    if (!dx && !dz)
        return 0;
    world_init(&s_world, seed);
    for (j = -1; j <= 1; j++)
        for (i = -1; i <= 1; i++)
            world_load(&s_world, cx + i, cz + j);
    CHECK_EQ_INT(world_get(&s_world, x, y, z), BLOCK_TORCH);
    CHECK_EQ_INT(light_world_level(&s_world, x, y, z), LIGHT_TORCH);
    if (dx) {
        CHECK_EQ_INT(world_get(&s_world, x + dx, y, z), BLOCK_AIR);
        CHECK_EQ_INT(light_world_level(&s_world, x + dx, y, z), LIGHT_TORCH - 1);
    }
    if (dz) {
        CHECK_EQ_INT(world_get(&s_world, x, y, z + dz), BLOCK_AIR);
        CHECK_EQ_INT(light_world_level(&s_world, x, y, z + dz), LIGHT_TORCH - 1);
    }
    return 1;
}

static void check_lamps(void)
{
    int s, rx, rz, i, n = 0;

    for (s = 1; s < 60 && n < 4; s++) {
        VillagePlan p;
        if (!find_village((uint32_t)s, 0, &p, &rx, &rz))
            continue;
        for (i = 0; i < p.nbuild && n < 4; i++)
            if (p.b[i].tmpl == TMPL_LAMP)
                n += check_lamp((uint32_t)s, &p.b[i]);
    }
    CHECK_EQ_INT(n >= 3, 1);
}

void test_structure_world(void)
{
    check_villages();
    check_ruins();
    check_seamless(2026u);
    check_seamless(77u);
    check_spawn();
    check_lamps();
}
