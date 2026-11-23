#include <string.h>

#include "seed.h"
#include "terrain.h"
#include "test.h"

/* Row 2-03: world seeds, random or typed. */

/* seed_from_text() on text; *ok whether it took it. The seed is 0xDEADBEEF
 * when it was left alone. */
static uint32_t typed(const char *text, int *ok)
{
    uint32_t s = 0xDEADBEEFu;

    *ok = seed_from_text(text, &s);
    return s;
}

/* The words' sum worked out here the long way: h = h * 31 + c. */
static uint32_t sum31(const char *text)
{
    uint32_t h = 0;
    size_t i;

    for (i = 0; i < strlen(text); i++)
        h = h * 31u + (unsigned char)text[i];
    return h;
}

static void test_numbers(void)
{
    int ok;

    CHECK_EQ_INT((int)typed("2026", &ok), 2026);
    CHECK_EQ_INT(ok, 1);
    CHECK_EQ_INT((int)typed("0", &ok), 0);
    CHECK_EQ_INT(ok, 1);
    CHECK_EQ_INT((int)typed("007", &ok), 7);
    CHECK_EQ_INT((int)typed("-1", &ok), -1);
    CHECK_EQ_INT(typed("-1", &ok) == 0xFFFFFFFFu, 1);
    CHECK_EQ_INT((int)typed("-0", &ok), 0);
    CHECK_EQ_INT(typed("2147483647", &ok) == 0x7FFFFFFFu, 1);
    CHECK_EQ_INT(typed("-2147483648", &ok) == 0x80000000u, 1);
    CHECK_EQ_INT(ok, 1);
    /* Spaces at either end do not count. */
    CHECK_EQ_INT((int)typed("  2026 ", &ok), 2026);
    CHECK_EQ_INT((int)typed("\t-55\n", &ok), -55);
}

static void test_words(void)
{
    int ok;

    /* Not whole numbers in range: the words' sum. */
    CHECK_EQ_INT(typed("2147483648", &ok) == sum31("2147483648"), 1);
    CHECK_EQ_INT(ok, 1);
    CHECK_EQ_INT(typed("-2147483649", &ok) == sum31("-2147483649"), 1);
    CHECK_EQ_INT(typed("99999999999999999999", &ok) == sum31("99999999999999999999"), 1);
    CHECK_EQ_INT(typed("-", &ok) == sum31("-"), 1);
    CHECK_EQ_INT(typed("--5", &ok) == sum31("--5"), 1);
    CHECK_EQ_INT(typed("12a", &ok) == sum31("12a"), 1);
    CHECK_EQ_INT(typed("1 2", &ok) == sum31("1 2"), 1);
    CHECK_EQ_INT(typed("+5", &ok) == sum31("+5"), 1);
    CHECK_EQ_INT(typed("Hello world", &ok) == sum31("Hello world"), 1);
    CHECK_EQ_INT(ok, 1);
    /* A known value, so the sum itself is pinned: 'a' 'b' = 97 * 31 + 98. */
    CHECK_EQ_INT((int)typed("ab", &ok), 97 * 31 + 98);
    CHECK_EQ_INT((int)typed("a", &ok), 97);
    /* The trimmed text is what counts; case counts. */
    CHECK_EQ_INT(typed("  my world  ", &ok) == sum31("my world"), 1);
    CHECK_EQ_INT(typed("My world", &ok) != typed("my world", &ok), 1);
    /* The same words make the same seed every time. */
    CHECK_EQ_INT(typed("Castle", &ok) == typed("Castle", &ok), 1);
}

static void test_nothing(void)
{
    int ok;

    CHECK_EQ_INT(typed("", &ok) == 0xDEADBEEFu, 1);
    CHECK_EQ_INT(ok, 0);
    CHECK_EQ_INT(typed("   ", &ok) == 0xDEADBEEFu, 1);
    CHECK_EQ_INT(ok, 0);
    CHECK_EQ_INT(typed(" \t\r\n", &ok) == 0xDEADBEEFu, 1);
    CHECK_EQ_INT(ok, 0);
}

static void test_random(void)
{
    uint32_t a = seed_random(0), b = seed_random(1), c = seed_random(2);
    uint64_t t;
    int same = 0, bits_moved = 0, i;
    uint32_t ones = 0, zeros = 0;

    /* Same tick, same seed; neighbouring ticks far apart. */
    CHECK_EQ_INT(seed_random(12345) == seed_random(12345), 1);
    CHECK_EQ_INT(a != b && b != c && a != c, 1);
    /* Every one of the 64 tick bits moves the seed. */
    for (i = 0; i < 64; i++)
        bits_moved += seed_random(0x0123456789ABCDEFull) != seed_random(0x0123456789ABCDEFull ^ (1ull << i));
    CHECK_EQ_INT(bits_moved, 64);
    /* Over 1000 ticks one apart (the clock's smallest step): no two seeds
     * the same, and each seed bit set about half the time. */
    for (t = 1000; t < 2000; t++) {
        uint32_t s = seed_random(t);
        uint64_t u;

        for (u = t + 1; u < 2000; u++)
            same += seed_random(u) == s;
        ones |= s;
        zeros |= ~s;
    }
    CHECK_EQ_INT(same, 0);
    CHECK_EQ_INT(ones == 0xFFFFFFFFu, 1);
    CHECK_EQ_INT(zeros == 0xFFFFFFFFu, 1);
    for (i = 0; i < 32; i++) {
        int n = 0;

        for (t = 0; t < 1000; t++)
            n += (seed_random(t) >> i) & 1u;
        CHECK_EQ_INT(n > 400 && n < 600, 1);
    }
    /* splitmix64 pinned: tick 0 gives 0xE220A8397B1DCDAF, the top half. */
    CHECK_EQ_INT(a == 0xE220A839u, 1);
}

static void check_text(uint32_t seed, const char *want)
{
    char text[SEED_TEXT_SIZE + 4];
    int ok;

    memset(text, 'Z', sizeof text);
    seed_to_text(seed, text);
    CHECK_EQ_INT(strcmp(text, want), 0);
    CHECK_EQ_INT(text[SEED_TEXT_SIZE] == 'Z', 1); /* nothing past the size */
    if (strcmp(text, want))
        printf("     seed_to_text(%u) is \"%s\", expected \"%s\"\n", (unsigned)seed, text, want);
    /* Typed back in, it is the same seed. */
    CHECK_EQ_INT(typed(text, &ok) == seed, 1);
    CHECK_EQ_INT(ok, 1);
}

static void test_text(void)
{
    uint64_t t;
    int back = 0;

    check_text(0, "0");
    check_text(7, "7");
    check_text(2026, "2026");
    check_text(2147483647u, "2147483647");
    check_text(0xFFFFFFFFu, "-1");
    check_text(0x80000000u, "-2147483648");
    check_text(0x80000001u, "-2147483647");
    check_text((uint32_t)-1234, "-1234");
    check_text(1000000000u, "1000000000");
    /* Every random seed reads back as itself. */
    for (t = 0; t < 2000; t++) {
        char text[SEED_TEXT_SIZE];
        uint32_t s = seed_random(t), r = 0;

        seed_to_text(s, text);
        back += seed_from_text(text, &r) && r == s && strlen(text) < SEED_TEXT_SIZE;
    }
    CHECK_EQ_INT(back, 2000);
}

/* The seed is what decides the ground: typing a seed in makes the same
 * ground as that seed given straight to the terrain. */
static void test_ground(void)
{
    uint32_t s;
    int x, z, same_a = 0, same_b = 0, differ = 0;

    CHECK_EQ_INT(seed_from_text("2026", &s), 1);
    for (z = -40; z <= 40; z += 5)
        for (x = -40; x <= 40; x += 5) {
            same_a += terrain_height(s, x, z) == terrain_height(2026u, x, z);
            same_b += terrain_height(s, x, z) == terrain_height(s, x, z);
            differ += terrain_height(s, x, z) != terrain_height(s + 1u, x, z);
        }
    CHECK_EQ_INT(same_a, 17 * 17);
    CHECK_EQ_INT(same_b, 17 * 17);
    CHECK_EQ_INT(differ > 17 * 17 / 4, 1);
}

void test_seed(void)
{
    test_numbers();
    test_words();
    test_nothing();
    test_random();
    test_text();
    test_ground();
}
