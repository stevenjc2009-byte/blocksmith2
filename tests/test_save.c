#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "save.h"
#include "stream.h"
#include "terrain.h"
#include "test.h"

/* Row 2-04: saving and loading, the packing half (bytes in, bytes out) and
 * the autosave timer. The files are in test_save_files.c. */

static Chunk s_a, s_b;
static uint8_t s_buf[SAVE_RLE_MAX + 64];

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* A new CRC on the end of a packed file, after a test changed it. */
static void reseal(uint8_t *p, size_t size)
{
    put32(p + size - 4, save_crc32(p, size - 4));
}

static int chunks_equal(const Chunk *a, const Chunk *b)
{
    return memcmp(a->blocks, b->blocks, sizeof a->blocks) == 0;
}

/* --- packing --- */

static void test_crc(void)
{
    CHECK_EQ_INT(save_crc32("123456789", 9) == 0xCBF43926u, 1);
    CHECK_EQ_INT(save_crc32("", 0) == 0u, 1);
    CHECK_EQ_INT(save_crc32("a", 1) == 0xE8B7BE43u, 1);
}

static void rle_round_trip(const Chunk *c, size_t want_n)
{
    size_t n = save_rle_pack(c, s_buf);

    CHECK_EQ_INT(n % 2 == 0 && n >= 2 && n <= SAVE_RLE_MAX, 1);
    if (want_n)
        CHECK_EQ_INT((int)n, (int)want_n);
    memset(s_b.blocks, 0x77, sizeof s_b.blocks);
    CHECK_EQ_INT(save_rle_unpack(s_buf, n, &s_b), 1);
    CHECK_EQ_INT(chunks_equal(c, &s_b), 1);
}

static void test_rle(void)
{
    uint8_t *b = &s_a.blocks[0][0][0];
    size_t i, n;

    /* All air: runs of 255 and the rest. */
    memset(s_a.blocks, BLOCK_AIR, sizeof s_a.blocks);
    rle_round_trip(&s_a, 2 * ((SAVE_CHUNK_BLOCKS + 254) / 255));
    /* The seed's ground, a few chunks. */
    terrain_fill_chunk(&s_a, 2026, 0, 0);
    rle_round_trip(&s_a, 0);
    terrain_fill_chunk(&s_a, 7, -5, 13);
    rle_round_trip(&s_a, 0);
    /* Ground packs small: under a tenth of the worst case. */
    CHECK_EQ_INT(save_rle_pack(&s_a, s_buf) < SAVE_RLE_MAX / 10, 1);
    /* The worst case: every block unlike the one before. */
    for (i = 0; i < SAVE_CHUNK_BLOCKS; i++)
        b[i] = (uint8_t)(i % 2 ? BLOCK_STONE : BLOCK_GRASS);
    rle_round_trip(&s_a, SAVE_RLE_MAX);
    /* Runs longer than 255 split; every block kind. */
    for (i = 0; i < SAVE_CHUNK_BLOCKS; i++)
        b[i] = (uint8_t)((i / 300) % BLOCK_COUNT);
    rle_round_trip(&s_a, 0);
    /* One different block at each end. */
    memset(s_a.blocks, BLOCK_DIRT, sizeof s_a.blocks);
    b[0] = BLOCK_STONE;
    b[SAVE_CHUNK_BLOCKS - 1] = BLOCK_GRASS;
    rle_round_trip(&s_a, 0);

    /* Runs that do not fill the chunk exactly, or are not runs, fail. */
    memset(s_a.blocks, BLOCK_AIR, sizeof s_a.blocks);
    n = save_rle_pack(&s_a, s_buf);
    CHECK_EQ_INT(save_rle_unpack(s_buf, n, &s_b), 1);       /* (good to start) */
    CHECK_EQ_INT(save_rle_unpack(s_buf, n - 2, &s_b), 0);   /* a run short */
    CHECK_EQ_INT(save_rle_unpack(s_buf, n - 1, &s_b), 0);   /* odd */
    CHECK_EQ_INT(save_rle_unpack(s_buf, 0, &s_b), 0);       /* nothing */
    s_buf[n] = 1;
    s_buf[n + 1] = BLOCK_AIR;
    CHECK_EQ_INT(save_rle_unpack(s_buf, n + 2, &s_b), 0);   /* a run too many */
    s_buf[n] = 0;
    CHECK_EQ_INT(save_rle_unpack(s_buf, n + 2, &s_b), 0);   /* an empty run at the end */
    s_buf[0] = 0;
    CHECK_EQ_INT(save_rle_unpack(s_buf, n, &s_b), 0);       /* an empty run */
    s_buf[0] = 255;
    s_buf[1] = BLOCK_COUNT;
    CHECK_EQ_INT(save_rle_unpack(s_buf, n, &s_b), 0);       /* no such block */
    s_buf[1] = 255;
    CHECK_EQ_INT(save_rle_unpack(s_buf, n, &s_b), 0);
    s_buf[1] = BLOCK_AIR;
    CHECK_EQ_INT(save_rle_unpack(s_buf, n, &s_b), 1);       /* (put back) */
    /* The last run reaching past the end. */
    s_buf[n - 2] = (uint8_t)(s_buf[n - 2] + 1);
    CHECK_EQ_INT(save_rle_unpack(s_buf, n, &s_b), 0);
}

static void sample_info(SaveInfo *info)
{
    int k;

    info->seed = 0xDEADBEEFu;
    info->x = -1234.5f;
    info->y = 67.25f;
    info->z = 89012.125f;
    info->yaw = 5.5f;
    info->pitch = -1.25f;
    for (k = 0; k < HOTBAR_SLOTS; k++)
        info->hotbar[k] = (uint8_t)(k % BLOCK_COUNT);
    info->chosen = 8;
}

static int info_equal(const SaveInfo *a, const SaveInfo *b)
{
    return a->seed == b->seed && a->x == b->x && a->y == b->y && a->z == b->z &&
           a->yaw == b->yaw && a->pitch == b->pitch &&
           memcmp(a->hotbar, b->hotbar, HOTBAR_SLOTS) == 0 && a->chosen == b->chosen;
}

static void test_info(void)
{
    SaveInfo in, out;
    uint8_t buf[SAVE_INFO_SIZE], bad[SAVE_INFO_SIZE];
    float nan = NAN, inf = INFINITY;
    size_t i;
    int flips_caught = 0;

    sample_info(&in);
    save_info_pack(&in, buf);
    CHECK_EQ_INT(memcmp(buf, "BS2W", 4), 0);
    CHECK_EQ_INT(save_info_unpack(buf, &out), 1);
    CHECK_EQ_INT(info_equal(&in, &out), 1);
    /* Every single bit flipped is caught. */
    for (i = 0; i < SAVE_INFO_SIZE * 8; i++) {
        memcpy(bad, buf, sizeof bad);
        bad[i / 8] ^= (uint8_t)(1u << (i % 8));
        flips_caught += !save_info_unpack(bad, &out);
    }
    CHECK_EQ_INT(flips_caught, SAVE_INFO_SIZE * 8);
    /* With a good CRC: resealing alone still reads... */
    memcpy(bad, buf, sizeof bad);
    reseal(bad, sizeof bad);
    CHECK_EQ_INT(save_info_unpack(bad, &out), 1);
    /* ...but another version, a chosen slot past the last, a slot with no
     * such block, or a number that is not finite does not. */
    memcpy(bad, buf, sizeof bad);
    bad[4] = 2;
    reseal(bad, sizeof bad);
    CHECK_EQ_INT(save_info_unpack(bad, &out), 0);
    memcpy(bad, buf, sizeof bad);
    bad[3] = 'S'; /* another file's mark, "BS2S" */
    reseal(bad, sizeof bad);
    CHECK_EQ_INT(save_info_unpack(bad, &out), 0);
    memcpy(bad, buf, sizeof bad);
    bad[32 + HOTBAR_SLOTS] = HOTBAR_SLOTS;
    reseal(bad, sizeof bad);
    CHECK_EQ_INT(save_info_unpack(bad, &out), 0);
    memcpy(bad, buf, sizeof bad);
    bad[32 + 4] = BLOCK_COUNT;
    reseal(bad, sizeof bad);
    CHECK_EQ_INT(save_info_unpack(bad, &out), 0);
    for (i = 0; i < 5; i++) {
        memcpy(bad, buf, sizeof bad);
        memcpy(bad + 12 + 4 * i, i % 2 ? &inf : &nan, 4);
        reseal(bad, sizeof bad);
        CHECK_EQ_INT(save_info_unpack(bad, &out), 0);
    }
    /* A failed read leaves *info as it was. */
    memset(&out, 0, sizeof out);
    CHECK_EQ_INT(save_info_unpack(bad, &out), 0);
    CHECK_EQ_INT(out.seed == 0 && out.chosen == 0, 1);
}

static void test_settings_pack(void)
{
    SaveSettings in, out;
    uint8_t buf[SAVE_SETTINGS_SIZE];
    int v, k;
    static const int lasts[4] = { 0, 1, 77, SAVE_WORLDS_MAX };

    for (v = STREAM_VIEW_MIN; v <= STREAM_VIEW_MAX; v++)
        for (k = 0; k < 4; k++) {
            in.view = v;
            in.last = lasts[k];
            save_settings_pack(&in, buf);
            out.view = out.last = -1;
            CHECK_EQ_INT(save_settings_unpack(buf, &out), 1);
            CHECK_EQ_INT(out.view, v);
            CHECK_EQ_INT(out.last, lasts[k]);
        }
    in.view = STREAM_VIEW_MIN - 1;
    in.last = 1;
    save_settings_pack(&in, buf);
    CHECK_EQ_INT(save_settings_unpack(buf, &out), 0);
    in.view = STREAM_VIEW_MAX + 1;
    save_settings_pack(&in, buf);
    CHECK_EQ_INT(save_settings_unpack(buf, &out), 0);
    in.view = STREAM_VIEW_MIN;
    in.last = SAVE_WORLDS_MAX + 1;
    save_settings_pack(&in, buf);
    CHECK_EQ_INT(save_settings_unpack(buf, &out), 0);
    in.last = 3;
    save_settings_pack(&in, buf);
    buf[9] ^= 1;
    CHECK_EQ_INT(save_settings_unpack(buf, &out), 0);
    buf[9] ^= 1;
    buf[0] = 'X';
    CHECK_EQ_INT(save_settings_unpack(buf, &out), 0);
    buf[0] = 'B';
    CHECK_EQ_INT(save_settings_unpack(buf, &out), 1);
}

static void name_round_trip(int cx, int cz)
{
    char name[32];
    int x = 12345, z = 12345;

    save_chunk_name(name, cx, cz);
    CHECK_EQ_INT((int)strlen(name) < 32, 1);
    CHECK_EQ_INT(save_chunk_name_parse(name, &x, &z), 1);
    CHECK_EQ_INT(x, cx);
    CHECK_EQ_INT(z, cz);
    strcat(name, ".tmp");
    x = z = 12345;
    CHECK_EQ_INT(save_chunk_name_parse(name, &x, &z), 1);
    CHECK_EQ_INT(x == cx && z == cz, 1);
}

static void test_names(void)
{
    static const char *const bad[] = {
        "x+3z1.chk", "x03z1.chk", "x-0z1.chk", "x3z-0.chk", "x3z01.chk", "x3z1.chk.bak",
        "x3z1", "y3z1.chk", "x3z.chk", "xz1.chk", "x3z1.chkx", "x3z1.CHK", "X3z1.chk",
        "x2147483648z0.chk", "x-2147483649z0.chk", "x12345678901z0.chk", "x3 z1.chk",
        "world.dat", "world.dat.tmp", ".", "..", "", "x-z1.chk", "x3z1.tmp", "x3z1.chk.tmp.tmp"
    };
    char name[32];
    size_t i;
    int x, z, rejected = 0;

    save_chunk_name(name, -3, 12);
    CHECK_EQ_INT(strcmp(name, "x-3z12.chk"), 0);
    name_round_trip(0, 0);
    name_round_trip(-1, 1);
    name_round_trip(10, -10);
    name_round_trip(INT_MIN, INT_MAX);
    name_round_trip(INT_MAX, INT_MIN);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++)
        rejected += !save_chunk_name_parse(bad[i], &x, &z);
    CHECK_EQ_INT(rejected, (int)(sizeof bad / sizeof bad[0]));
}

static void test_set(void)
{
    SaveChunkSet s;
    int i, all = 1, none = 1;

    save_set_init(&s);
    CHECK_EQ_INT(save_set_has(&s, 0, 0), 0);
    CHECK_EQ_INT(s.count, 0);
    for (i = 0; i < 10000; i++)
        all &= save_set_add(&s, i % 100 - 50, (i / 100) * 7 - 300);
    CHECK_EQ_INT(all, 1);
    CHECK_EQ_INT(s.count, 10000);
    CHECK_EQ_INT(s.count * 2 <= s.cap, 1);
    for (i = 0; i < 10000; i++)
        all &= save_set_has(&s, i % 100 - 50, (i / 100) * 7 - 300);
    CHECK_EQ_INT(all, 1);
    /* Neighbours never added are not there. */
    for (i = 0; i < 10000; i++)
        none &= !save_set_has(&s, i % 100 - 50, (i / 100) * 7 - 299);
    CHECK_EQ_INT(none, 1);
    CHECK_EQ_INT(save_set_has(&s, 50, -300), 0);
    /* Adding again changes nothing. */
    CHECK_EQ_INT(save_set_add(&s, -50, -300), 1);
    CHECK_EQ_INT(s.count, 10000);
    /* The extremes. */
    CHECK_EQ_INT(save_set_add(&s, INT_MIN, INT_MAX), 1);
    CHECK_EQ_INT(save_set_add(&s, INT_MAX, INT_MIN), 1);
    CHECK_EQ_INT(save_set_has(&s, INT_MIN, INT_MAX) && save_set_has(&s, INT_MAX, INT_MIN), 1);
    CHECK_EQ_INT(save_set_has(&s, INT_MIN, INT_MIN), 0);
    CHECK_EQ_INT(s.count, 10002);
    save_set_free(&s);
    CHECK_EQ_INT(s.count == 0 && s.cap == 0 && !save_set_has(&s, -50, -300), 1);
}

/* A chunk file reads only as the chunk its head names: one place off in
 * either direction alone is refused. */
static void test_chunk_head(void)
{
    long n;

    terrain_fill_chunk(&s_a, 2026, 3, -2);
    n = (long)save_chunk_pack(&s_a, 3, -2, s_buf);
    memset(s_b.blocks, 0x55, sizeof s_b.blocks);
    CHECK_EQ_INT(save_chunk_unpack(s_buf, n, 3, -2, &s_b), 1);
    CHECK_EQ_INT(chunks_equal(&s_a, &s_b), 1);
    CHECK_EQ_INT(save_chunk_unpack(s_buf, n, 4, -2, &s_b), 0);
    CHECK_EQ_INT(save_chunk_unpack(s_buf, n, 3, -1, &s_b), 0);
    CHECK_EQ_INT(save_chunk_unpack(s_buf, n, -2, 3, &s_b), 0);
}

static void test_due(void)
{
    CHECK_EQ_INT(save_due(0, 0), 0);
    CHECK_EQ_INT(save_due(SAVE_AUTOSAVE_MS - 1, 0), 0);
    CHECK_EQ_INT(save_due(SAVE_AUTOSAVE_MS, 0), 1);
    CHECK_EQ_INT(save_due(SAVE_AUTOSAVE_MS + 1, 0), 1);
    CHECK_EQ_INT(save_due(5000 + SAVE_AUTOSAVE_MS - 1, 5000), 0);
    CHECK_EQ_INT(save_due(5000 + SAVE_AUTOSAVE_MS, 5000), 1);
    CHECK_EQ_INT(save_due(5, 10), 0); /* the clock went back */
    CHECK_EQ_INT(save_due(0, UINT64_MAX), 0);
    CHECK_EQ_INT(save_due(UINT64_MAX, UINT64_MAX - SAVE_AUTOSAVE_MS), 1);
    CHECK_EQ_INT(save_due(UINT64_MAX, UINT64_MAX - SAVE_AUTOSAVE_MS + 1), 0);
    CHECK_EQ_INT(SAVE_AUTOSAVE_MS == 180000u, 1);
}

void test_save(void)
{
    test_crc();
    test_rle();
    test_info();
    test_settings_pack();
    test_names();
    test_set();
    test_chunk_head();
    test_due();
}
