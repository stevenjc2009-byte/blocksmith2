#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "chunk.h"
#include "terrain.h"
#include "test.h"

static const uint32_t k_seeds[] = { 0u, 1u, 2u, 77u, 2026u, 0xC0FFEEu, 0x80000000u, 0xFFFFFFFFu };
#define SEED_COUNT ((int)(sizeof k_seeds / sizeof k_seeds[0]))

/* A square of whole chunks, read with terrain_chunk_columns(). */
#define AREA_CHUNKS 32
#define AREA (AREA_CHUNKS * CHUNK_SIZE)
static TerrainColumn s_area[AREA][AREA]; /* [z][x] */

/* The wide look round for each kind: every WIDE_STEP blocks out to
 * WIDE_REACH either way. */
#define WIDE_REACH 4096
#define WIDE_STEP 32

/* The chunk holding world block x (rounding down for negative x too). */
static int chunk_of(int x)
{
    return x >= 0 ? x / CHUNK_SIZE : -((CHUNK_SIZE - 1 - x) / CHUNK_SIZE);
}

typedef struct {
    int out_of_range, steep, bad_surface, cliffs, tall_cliffs, stone;
    int land_pairs, level_pairs;
    int kinds[TERRAIN_KIND_COUNT];
} AreaCheck;

static void load_area(uint32_t seed, int cx0, int cz0, int chunks)
{
    TerrainColumn part[CHUNK_SIZE][CHUNK_SIZE];
    int cx, cz, z;

    for (cz = 0; cz < chunks; cz++)
        for (cx = 0; cx < chunks; cx++) {
            terrain_chunk_columns(seed, cx0 + cx, cz0 + cz, part);
            for (z = 0; z < CHUNK_SIZE; z++)
                memcpy(&s_area[cz * CHUNK_SIZE + z][cx * CHUNK_SIZE], part[z], sizeof part[z]);
        }
}

/* The surface block and height a column of its kind should have. */
static int surface_ok(const TerrainColumn *c)
{
    switch (c->kind) {
    case TERRAIN_OCEAN:
        return c->surface == BLOCK_SAND && c->height < TERRAIN_SEA;
    case TERRAIN_RIVER:
        return c->surface == BLOCK_SAND && c->height <= TERRAIN_SEA;
    case TERRAIN_BEACH:
        return c->surface == BLOCK_SAND && c->height >= TERRAIN_SEA && c->height <= TERRAIN_SEA + 3;
    case TERRAIN_PLAINS:
    case TERRAIN_HILLS:
        return c->surface == BLOCK_GRASS && c->height > TERRAIN_SEA;
    case TERRAIN_MOUNTAIN:
        /* Under the water (a river at its foot) it is a sand bed too. */
        if (c->height <= TERRAIN_SEA)
            return c->surface == BLOCK_SAND;
        return c->surface == BLOCK_GRASS || c->surface == BLOCK_STONE;
    default:
        return 0;
    }
}

/* Next-door columns a and b (on x or on z). */
static void check_pair(const TerrainColumn *a, const TerrainColumn *b, AreaCheck *r)
{
    int d = abs(a->height - b->height);

    if (a->kind != TERRAIN_MOUNTAIN && b->kind != TERRAIN_MOUNTAIN) {
        r->steep += d > 1;
        if (a->height > TERRAIN_SEA && b->height > TERRAIN_SEA) {
            r->land_pairs++;
            r->level_pairs += d == 0;
        }
    } else if (a->kind == TERRAIN_MOUNTAIN && b->kind == TERRAIN_MOUNTAIN) {
        r->cliffs += d >= 3;
        r->tall_cliffs += d >= 6;
    }
}

static void check_area(int size, AreaCheck *r)
{
    int x, z;

    memset(r, 0, sizeof *r);
    for (z = 0; z < size; z++)
        for (x = 0; x < size; x++) {
            const TerrainColumn *c = &s_area[z][x];

            r->out_of_range += c->height < TERRAIN_FLOOR || c->height > TERRAIN_PEAK;
            r->bad_surface += !surface_ok(c);
            r->stone += c->surface == BLOCK_STONE;
            if (c->kind >= 0 && c->kind < TERRAIN_KIND_COUNT)
                r->kinds[c->kind]++;
            if (x + 1 < size)
                check_pair(c, &s_area[z][x + 1], r);
            if (z + 1 < size)
                check_pair(c, &s_area[z + 1][x], r);
        }
}

/* Heights in range, the right surface for each kind, and one-block steps
 * everywhere but the mountains: round the origin and far out both ways. */
static void test_walkable(void)
{
    static const int k_far[][2] = { { 999000, -999100 }, { -999100, 999000 } };
    int i, k;

    for (i = 0; i < SEED_COUNT; i++) {
        AreaCheck r;

        load_area(k_seeds[i], -AREA_CHUNKS / 2, -AREA_CHUNKS / 2, AREA_CHUNKS);
        check_area(AREA, &r);
        CHECK_EQ_INT(r.out_of_range, 0);
        CHECK_EQ_INT(r.bad_surface, 0);
        CHECK_EQ_INT(r.steep, 0);
        /* Gentle: most next-door land columns are level. */
        CHECK_EQ_INT(r.level_pairs * 2 > r.land_pairs, 1);
        for (k = 0; k < 2; k++) {
            load_area(k_seeds[i], chunk_of(k_far[k][0]), chunk_of(k_far[k][1]), 8);
            check_area(8 * CHUNK_SIZE, &r);
            CHECK_EQ_INT(r.out_of_range, 0);
            CHECK_EQ_INT(r.bad_surface, 0);
            CHECK_EQ_INT(r.steep, 0);
        }
    }
}

/* A column of the given kind found by the wide look round from the origin,
 * at least min_height high; false if there is none. */
static bool find_kind(uint32_t seed, TerrainKind kind, int min_height, int *fx, int *fz)
{
    int x, z;

    for (z = -WIDE_REACH; z <= WIDE_REACH; z += WIDE_STEP)
        for (x = -WIDE_REACH; x <= WIDE_REACH; x += WIDE_STEP) {
            TerrainColumn c = terrain_column(seed, x, z);

            if (c.kind == kind && c.height >= min_height) {
                *fx = x;
                *fz = z;
                return true;
            }
        }
    return false;
}

/* Every seed has seas, beaches, rivers, plains, hills and mountains, and
 * the high mountains have cliffs (3 blocks or more straight up) of bare
 * stone. In at least half the seeds there are also many real cliffs, 6
 * blocks or more straight up: too tall to jump, dug or built up (steve's
 * answer for row 4-01). */
static void test_kinds(void)
{
    int i, k, tall = 0;

    for (i = 0; i < SEED_COUNT; i++) {
        int x = 0, z = 0;
        AreaCheck r;

        for (k = 0; k < TERRAIN_KIND_COUNT; k++)
            CHECK_EQ_INT(find_kind(k_seeds[i], (TerrainKind)k, TERRAIN_FLOOR, &x, &z), 1);
        CHECK_EQ_INT(find_kind(k_seeds[i], TERRAIN_MOUNTAIN, 100, &x, &z), 1);
        load_area(k_seeds[i], chunk_of(x) - 8, chunk_of(z) - 8, 16);
        check_area(16 * CHUNK_SIZE, &r);
        CHECK_EQ_INT(r.cliffs > 0, 1);
        CHECK_EQ_INT(r.stone > 0, 1);
        CHECK_EQ_INT(r.steep, 0);
        CHECK_EQ_INT(r.bad_surface, 0);
        tall += r.tall_cliffs >= 100;
    }
    CHECK_EQ_INT(tall * 2 >= SEED_COUNT, 1);
}

/* The same seed always makes the same ground, all three ways of asking
 * agree, and a new seed is a new place. */
static void test_seeds(void)
{
    TerrainColumn part[CHUNK_SIZE][CHUNK_SIZE];
    int i, x, z;

    for (i = 0; i < SEED_COUNT; i++) {
        int differ = 0, same_again = 1, agree = 1;

        for (z = -32; z < 32; z++)
            for (x = -32; x < 32; x++) {
                TerrainColumn c = terrain_column(k_seeds[i], x, z);
                TerrainColumn again = terrain_column(k_seeds[i], x, z);

                if (i + 1 < SEED_COUNT)
                    differ += c.height != terrain_height(k_seeds[i + 1], x, z);
                same_again &= c.height == again.height && c.surface == again.surface && c.kind == again.kind;
                agree &= c.height == terrain_height(k_seeds[i], x, z);
            }
        CHECK_EQ_INT(same_again, 1);
        CHECK_EQ_INT(agree, 1);
        if (i + 1 < SEED_COUNT)
            CHECK_EQ_INT(differ * 2 > 64 * 64, 1);

        /* A chunk at once is the same as one column at a time. */
        terrain_chunk_columns(k_seeds[i], -3 + i, 5 - 2 * i, part);
        agree = 1;
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++) {
                TerrainColumn c = terrain_column(k_seeds[i], (-3 + i) * CHUNK_SIZE + x, (5 - 2 * i) * CHUNK_SIZE + z);

                agree &= c.height == part[z][x].height && c.surface == part[z][x].surface &&
                         c.kind == part[z][x].kind;
            }
        CHECK_EQ_INT(agree, 1);
    }
}

/* Chunk (cx, cz) filled matches its columns block for block: stone, then
 * TERRAIN_DIRT of what goes under the surface, the surface, still water up
 * to TERRAIN_SEA, air above. Counts the water blocks into *water. */
static void check_fill(uint32_t seed, int cx, int cz, int *water)
{
    static Chunk c;
    int x, y, z, wrong = 0;

    memset(&c, BLOCK_STONE, sizeof c); /* old contents must go */
    terrain_fill_chunk(&c, seed, cx, cz);
    for (z = 0; z < CHUNK_SIZE; z++)
        for (x = 0; x < CHUNK_SIZE; x++) {
            TerrainColumn col = terrain_column(seed, cx * CHUNK_SIZE + x, cz * CHUNK_SIZE + z);
            int h = col.height, under = col.surface == BLOCK_GRASS ? BLOCK_DIRT : col.surface;

            for (y = 0; y < WORLD_HEIGHT; y++) {
                int b = chunk_get(&c, x, y, z);
                int want = y > TERRAIN_SEA && y > h ? BLOCK_AIR
                         : y > h ? BLOCK_WATER
                         : y == h ? col.surface
                         : y >= h - TERRAIN_DIRT ? under : BLOCK_STONE;

                wrong += b != want;
                *water += b == BLOCK_WATER;
            }
        }
    CHECK_EQ_INT(wrong, 0);
}

static void test_fill(void)
{
    static const int k_chunks[][2] = { { 0, 0 }, { -1, -1 }, { -2, 1 }, { 1, -2 }, { 7, -5 }, { -40, 33 } };
    static const TerrainKind k_wet[] = { TERRAIN_OCEAN, TERRAIN_RIVER };
    int i, k;

    for (i = 0; i < 3; i++) {
        int water = 0;

        for (k = 0; k < (int)(sizeof k_chunks / sizeof k_chunks[0]); k++)
            check_fill(k_seeds[i], k_chunks[k][0], k_chunks[k][1], &water);
        /* And chunks with sea and river in them. */
        for (k = 0; k < 2; k++) {
            int x = 0, z = 0, before = water;

            CHECK_EQ_INT(find_kind(k_seeds[i], k_wet[k], TERRAIN_FLOOR, &x, &z), 1);
            check_fill(k_seeds[i], chunk_of(x), chunk_of(z), &water);
            CHECK_EQ_INT(water > before, 1);
        }
    }
}

/* The start is dry land, off the mountains, with a one-block step at most
 * to each column round it, and the same every time. */
static void test_spawn(void)
{
    int i, dx, dz;

    for (i = 0; i < SEED_COUNT; i++) {
        int x = 1 << 30, z = 1 << 30, x2 = 0, z2 = 0, bad = 0;
        TerrainColumn c;

        CHECK_EQ_INT(terrain_find_spawn(k_seeds[i], &x, &z), 1);
        c = terrain_column(k_seeds[i], x, z);
        CHECK_EQ_INT(c.height > TERRAIN_SEA, 1);
        CHECK_EQ_INT(c.kind != TERRAIN_MOUNTAIN, 1);
        CHECK_EQ_INT(abs(x) <= TERRAIN_SPAWN_REACH && abs(z) <= TERRAIN_SPAWN_REACH, 1);
        for (dz = -1; dz <= 1; dz++)
            for (dx = -1; dx <= 1; dx++) {
                TerrainColumn n = terrain_column(k_seeds[i], x + dx, z + dz);

                bad += n.kind == TERRAIN_MOUNTAIN || abs(n.height - c.height) > 1;
            }
        CHECK_EQ_INT(bad, 0);
        CHECK_EQ_INT(terrain_find_spawn(k_seeds[i], &x2, &z2), 1);
        CHECK_EQ_INT(x2 == x && z2 == z, 1);
    }
}

void test_terrain(void)
{
    test_walkable();
    test_kinds();
    test_seeds();
    test_fill();
    test_spawn();
}
