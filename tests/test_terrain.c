#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "chunk.h"
#include "terrain.h"
#include "test.h"

static const uint32_t k_seeds[] = { 0u, 1u, 2u, 77u, 12345u, 0xC0FFEEu, 0x80000000u, 0xFFFFFFFFu };
#define SEED_COUNT ((int)(sizeof k_seeds / sizeof k_seeds[0]))

/* Heights stay in range, next-door columns differ by at most one block
 * (walkable with a one-block step), and there are real hills, over a
 * square of x0..x0+size, z0..z0+size. Returns the height spread. */
static int check_area(uint32_t seed, int x0, int z0, int size, int *flat_pairs, int *pairs)
{
    int x, z, lo = 1 << 30, hi = -(1 << 30), out = 0, steep = 0;

    for (z = z0; z <= z0 + size; z++)
        for (x = x0; x <= x0 + size; x++) {
            int h = terrain_height(seed, x, z);
            int e = terrain_height(seed, x + 1, z);
            int s = terrain_height(seed, x, z + 1);

            out += h < TERRAIN_BASE || h > TERRAIN_TOP;
            steep += abs(e - h) > 1;
            steep += abs(s - h) > 1;
            *flat_pairs += (e == h) + (s == h);
            *pairs += 2;
            if (h < lo)
                lo = h;
            if (h > hi)
                hi = h;
        }
    CHECK_EQ_INT(out, 0);
    CHECK_EQ_INT(steep, 0);
    return hi - lo;
}

static void test_gentle_hills(void)
{
    int i;

    for (i = 0; i < SEED_COUNT; i++) {
        int flat = 0, pairs = 0;

        /* Around the origin, negative coordinates included. */
        CHECK_EQ_INT(check_area(k_seeds[i], -200, -200, 400, &flat, &pairs) >= 6, 1);
        /* Far away, both signs. */
        CHECK_EQ_INT(check_area(k_seeds[i], 999000, -999100, 100, &flat, &pairs) >= 3, 1);
        CHECK_EQ_INT(check_area(k_seeds[i], -999100, 999000, 100, &flat, &pairs) >= 3, 1);
        /* Gentle: most next-door columns are level. */
        CHECK_EQ_INT(flat * 2 > pairs, 1);
    }
}

static void test_seeds(void)
{
    int i, x, z;

    for (i = 0; i + 1 < SEED_COUNT; i++) {
        int differ = 0, same_again = 1;

        for (z = -32; z < 32; z++)
            for (x = -32; x < 32; x++) {
                int h = terrain_height(k_seeds[i], x, z);

                differ += h != terrain_height(k_seeds[i + 1], x, z);
                same_again &= h == terrain_height(k_seeds[i], x, z);
            }
        CHECK_EQ_INT(same_again, 1);
        CHECK_EQ_INT(differ * 2 > 64 * 64, 1); /* a new seed is a new place */
    }
}

static void test_fill(void)
{
    static Chunk c;
    static const int k_chunks[][2] = { { 0, 0 }, { -1, -1 }, { -2, 1 }, { 1, -2 }, { 7, -5 }, { -40, 33 } };
    int i, k, x, y, z;

    for (i = 0; i < 3; i++)
        for (k = 0; k < (int)(sizeof k_chunks / sizeof k_chunks[0]); k++) {
            int cx = k_chunks[k][0], cz = k_chunks[k][1];
            int wrong = 0, bad_column = 0;

            memset(&c, BLOCK_STONE, sizeof c); /* old contents must go */
            terrain_fill_chunk(&c, k_seeds[i], cx, cz);
            for (z = 0; z < CHUNK_SIZE; z++)
                for (x = 0; x < CHUNK_SIZE; x++) {
                    int h = terrain_height(k_seeds[i], cx * CHUNK_SIZE + x, cz * CHUNK_SIZE + z);
                    int grass = 0, dirt = 0, top = -1;

                    for (y = 0; y < WORLD_HEIGHT; y++) {
                        int b = chunk_get(&c, x, y, z);
                        int want = y > h ? BLOCK_AIR : y == h ? BLOCK_GRASS
                                 : y >= h - TERRAIN_DIRT ? BLOCK_DIRT : BLOCK_STONE;

                        wrong += b != want;
                        grass += b == BLOCK_GRASS;
                        dirt += b == BLOCK_DIRT;
                        if (b != BLOCK_AIR)
                            top = y;
                    }
                    bad_column += grass != 1 || dirt != TERRAIN_DIRT || top != h ||
                                  chunk_get(&c, x, 0, z) != BLOCK_STONE;
                }
            CHECK_EQ_INT(wrong, 0);
            CHECK_EQ_INT(bad_column, 0);
        }
}

void test_terrain(void)
{
    test_gentle_hills();
    test_seeds();
    test_fill();
}
