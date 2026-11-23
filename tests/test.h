#ifndef TEST_H
#define TEST_H

/* Tiny PC test helpers. No libraries: every check is counted, and the run
 * fails if any check fails or if no checks ran at all. */

#include <stdio.h>

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

#endif
