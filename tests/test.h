#ifndef TEST_H
#define TEST_H

/* Tiny PC test helpers. No libraries: every check is counted, and the run
 * fails if any check fails or if no checks ran at all. */

#include <stdint.h>
#include <stdio.h>

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

#endif
