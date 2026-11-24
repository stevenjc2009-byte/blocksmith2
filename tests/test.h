#ifndef TEST_H
#define TEST_H

/* Tiny PC test helpers. No libraries: every check is counted, and the run
 * fails if any check fails or if no checks ran at all. */

#include <stdint.h>
#include <stdio.h>

#include "chunk_mesh.h"
#include "village.h"
#include "world.h"

extern int g_checks;
extern int g_fails;

#define CHECK_EQ_INT(actual, expected)                                              \
    do {                                                                            \
        int a_ = (actual), e_ = (expected);                                         \
        g_checks++;                                                                 \
        if (a_ != e_) {                                                             \
            g_fails++;                                                              \
            printf("FAIL %s:%d: %s is %d, expected %d\n", __FILE__, __LINE__,       \
                   #actual, a_, e_);                                                \
        }                                                                           \
    } while (0)

/* Float check. Written so that a NaN fails instead of slipping through. */
#define CHECK_NEAR(actual, expected, tol)                                           \
    do {                                                                            \
        double a_ = (actual), e_ = (expected);                                      \
        g_checks++;                                                                 \
        if (!(a_ - e_ <= (tol) && e_ - a_ <= (tol))) {                              \
            g_fails++;                                                              \
            printf("FAIL %s:%d: %s is %g, expected %g\n", __FILE__, __LINE__,       \
                   #actual, a_, e_);                                                \
        }                                                                           \
    } while (0)

/* The small test world most tests use: chunks SMALL_MIN to
 * SMALL_MIN + SMALL_CHUNKS - 1 on x and z (blocks -32 to 31) loaded with the
 * seed's ground, nothing else. In test_world.c. */
#define SMALL_MIN (-2)
#define SMALL_CHUNKS 4
void test_world_small(World *w, uint32_t seed);

/* The first seed from 2026 up whose small world is all dry grass land
 * (plains or hills, terrain_column()): for tests about walking on, digging
 * and lighting real ground, which a sea or a river would get in the way of.
 * Found once, then kept (test_world.c). */
uint32_t test_land_seed(void);

/* The small test world with every block air (test_world.c). */
void test_world_cleared(World *w);

/* Chunk (cx, cz)'s slot in w, loaded or not (test_world.c). */
Chunk *test_slot_chunk(World *w, int cx, int cz);

/* One function per test file, called from test_main.c. */
void test_world_coords(void);
void test_framebuffer(void);
void test_mat4(void);
void test_texswizzle(void);
void test_blocktex(void);
void test_mesh(void);
void test_look(void);
void test_camera(void);
void test_chunk(void);
void test_chunk_ref(void);
void test_terrain(void);
void test_world(void);
void test_stream(void);
void test_walk(void);
void test_player(void);
void test_raycast(void);
void test_break(void);
void test_place(void);
void test_hotbar(void);
void test_font(void);
void test_fps(void);
void test_settings(void);
void test_seed(void);
void test_newworld(void);
void test_save(void);
void test_save_files(void);
void test_save_world(void);
void test_saveui(void);
void test_worldname(void);
void test_save_names(void);
void test_worldlist(void);
void test_worldlist_draw(void); /* test_worldlist_draw.c, called by test_worldlist() */
void test_lidsave(void);
void test_save_backup(void);
void test_ao(void);
void test_sky(void);
void test_light(void);
void test_cloud(void);
void test_cloud_cells(void); /* test_cloud_cells.c, called by test_cloud() */
void test_particles(void);
void test_held(void);
void test_light_world(void);
void test_weather(void);
void test_weather_drops(void);
void test_weather_climate(void); /* test_weather_climate.c, called by test_weather_drops() */
void test_weather_save(void);
void test_save_job(void);
void test_water(void);
void test_water_ref(void);
void test_biome(void);
void test_biome_tables(void); /* test_biome_tables.c, called by test_biome() */
/* A biome's grass tint on a reference grass top face (6 vertices). */
void test_ref_grass_tint(MeshVertex *face, int biome);
void test_structure(void);
void test_structure_world(void);
void test_structure_biome(void);
/* Row 4-05's checks of one village built in the world (test_structure_world.c). */
void test_check_village(uint32_t seed, const VillagePlan *p);
void test_flow(void);       /* row 4-04: flowing water */
void test_flow_queue(void);
void test_flow_world(void);
void test_flow_mesh(void);
void test_flow_swim(void);

/* Row 3-03's reference torch light (test_light.c): fills a table for chunk
 * c and its neighbours (nb as chunk_mesh.h, NULL entries air) from the rule
 * in light.h; test_ref_level() reads it for local (x, y, z), x and z from
 * -CHUNK_SIZE to 2 * CHUNK_SIZE - 1 (0 outside, or outside the world). */
void test_ref_light(const Chunk *c, const Chunk *const nb[NB_COUNT]);
int test_ref_level(int x, int y, int z);

/* Row 3-01's reference corner shadow (test_ao.c): the level (0 to
 * MESH_AO_OPEN) of corner `corner` of face `face` of the block at (bx, by,
 * bz), from the rule in chunk_mesh.h, solid() answering for every cell. */
int test_ref_ao(int (*solid)(void *ctx, int x, int y, int z), void *ctx,
                int bx, int by, int bz, int face, int corner);

#endif
