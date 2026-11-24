/* Timing of chunk generation and chunk meshing, for the frame-budget check
 * of row 4-03 (trees and plants). Host tool, not part of the game:
 *
 *   gcc -O2 -Iinclude tools/treebench.c <the CORE_SOURCES of the Makefile> -o treebench
 *   treebench [reps]
 *
 * For each biome, finds the chunk in a search area where that biome covers
 * the most columns, then reports world_gen_chunk() time, the mesh's face
 * and vertex count and chunk_mesh_build() time (best of `reps` runs and the
 * spread, in milliseconds). Then the same for the worst chunk (most faces) of
 * the chunks wholly in that biome, from the same seed's search area, with
 * the median faces of those chunks (only with the argument "worst"). */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "biome.h"
#include "chunk_mesh.h"
#include "terrain.h"
#include "worldgen.h"

static double now_ms(void)
{
    struct timespec ts;

    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

typedef struct {
    const char *name;
    int biome;
} Want;

static Chunk s_nb[9];

static void report(uint32_t seed, int cx, int cz, const char *name, int reps)
{
    const Chunk *nb[NB_COUNT];
    static const int k_at[NB_COUNT][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
                                           { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 } };
    static Chunk c;
    double gen_best = 1e9, gen_max = 0, mesh_best = 1e9, mesh_max = 0, t;
    int faces = 0, verts = 0, i, r;
    MeshVertex *buf;

    for (i = 0; i < NB_COUNT; i++) {
        world_gen_chunk(&s_nb[i], seed, cx + k_at[i][0], cz + k_at[i][1]);
        nb[i] = &s_nb[i];
    }
    for (r = 0; r < reps; r++) {
        t = now_ms();
        world_gen_chunk(&c, seed, cx, cz);
        t = now_ms() - t;
        gen_best = t < gen_best ? t : gen_best;
        gen_max = t > gen_max ? t : gen_max;
    }
    faces = chunk_mesh_count(&c, nb);
    buf = malloc((size_t)(faces + 1) * MESH_FACE_VERTS * sizeof *buf);
    for (r = 0; r < reps; r++) {
        t = now_ms();
        verts = chunk_mesh_build(&c, nb, 0.0f, 0.0f, buf, faces * MESH_FACE_VERTS);
        t = now_ms() - t;
        mesh_best = t < mesh_best ? t : mesh_best;
        mesh_max = t > mesh_max ? t : mesh_max;
    }
    free(buf);
    printf("%-12s chunk (%4d,%4d) gen %.3f..%.3f ms  faces %5d verts %6d  mesh %.3f..%.3f ms\n", name, cx,
           cz, gen_best, gen_max, faces, verts, mesh_best, mesh_max);
}

/* The most faces (with its neighbours) among the chunks wholly in `biome` of one
 * seed's search area; the median faces too. Returns false if none. */
static bool worst_chunk(uint32_t seed, int biome, int *wcx, int *wcz, int *median)
{
    static Chunk c, nbc[NB_COUNT];
    static int faces[3600];
    static const int k_at[NB_COUNT][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
                                           { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 } };
    const Chunk *nb[NB_COUNT];
    int cx, cz, x, z, n = 0, best = -1, i, j;

    for (cz = -30; cz < 30; cz++)
        for (cx = -30; cx < 30; cx++) {
            TerrainColumn col[CHUNK_SIZE][CHUNK_SIZE];
            int ok = 1, f;

            terrain_chunk_columns(seed, cx, cz, col);
            for (z = 0; z < CHUNK_SIZE && ok; z++)
                for (x = 0; x < CHUNK_SIZE && ok; x++)
                    ok = col[z][x].biome == biome && col[z][x].height >= TERRAIN_SEA + 1 &&
                         col[z][x].kind != TERRAIN_MOUNTAIN;
            if (!ok)
                continue;
            world_gen_chunk(&c, seed, cx, cz);
            for (i = 0; i < NB_COUNT; i++) {
                world_gen_chunk(&nbc[i], seed, cx + k_at[i][0], cz + k_at[i][1]);
                nb[i] = &nbc[i];
            }
            f = chunk_mesh_count(&c, nb);
            faces[n++] = f;
            if (f > best) {
                best = f;
                *wcx = cx;
                *wcz = cz;
            }
        }
    if (n == 0)
        return false;
    for (i = 1; i < n; i++) /* insertion sort: n is a few hundred at most */
        for (j = i; j > 0 && faces[j - 1] > faces[j]; j--) {
            int t = faces[j];

            faces[j] = faces[j - 1];
            faces[j - 1] = t;
        }
    *median = faces[n / 2];
    return true;
}

int main(int argc, char **argv)
{
    static const Want k_want[] = {
        { "plains", BIOME_PLAINS }, { "forest", BIOME_FOREST }, { "birch", BIOME_BIRCH_FOREST },
        { "dark_forest", BIOME_DARK_FOREST }, { "taiga", BIOME_TAIGA }, { "jungle", BIOME_JUNGLE },
        { "savanna", BIOME_SAVANNA }, { "desert", BIOME_DESERT }, { "swamp", BIOME_SWAMP },
    };
    static const uint32_t k_seeds[] = { 2026u, 1u, 77u, 12345u, 999u, 31337u };
    int reps = argc > 1 ? atoi(argv[1]) : 20, w, s;

    for (w = 0; w < (int)(sizeof k_want / sizeof k_want[0]); w++) {
        int best = -1, bcx = 0, bcz = 0, cx, cz, x, z;
        uint32_t bseed = 0;

        for (s = 0; s < (int)(sizeof k_seeds / sizeof k_seeds[0]); s++)
            for (cz = -30; cz < 30; cz++)
                for (cx = -30; cx < 30; cx++) {
                    TerrainColumn col[CHUNK_SIZE][CHUNK_SIZE];
                    int n = 0, flat = 1;

                    terrain_chunk_columns(k_seeds[s], cx, cz, col);
                    for (z = 0; z < CHUNK_SIZE; z++)
                        for (x = 0; x < CHUNK_SIZE; x++) {
                            n += col[z][x].biome == k_want[w].biome &&
                                 col[z][x].height >= TERRAIN_SEA + 1;
                            flat &= col[z][x].kind != TERRAIN_MOUNTAIN;
                        }
                    if (flat && n > best) {
                        best = n;
                        bcx = cx;
                        bcz = cz;
                        bseed = k_seeds[s];
                    }
                }
        if (best < 0)
            continue;
        printf("[seed %lu, %d of 256 columns]\n", (unsigned long)bseed, best);
        report(bseed, bcx, bcz, k_want[w].name, reps);
        if (argc > 2 && strcmp(argv[2], "worst") == 0) {
            int wcx = 0, wcz = 0, median = 0;

            if (worst_chunk(bseed, k_want[w].biome, &wcx, &wcz, &median)) {
                printf("  worst of the whole-biome chunks (median %d faces):\n  ", median);
                report(bseed, wcx, wcz, k_want[w].name, reps);
            }
        }
    }
    return 0;
}
