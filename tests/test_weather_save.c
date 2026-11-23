#include <math.h>
#include <string.h>

#include "block.h"
#include "save.h"
#include "test.h"
#include "weather.h"

/* world.dat version 3 keeps the weather (row 3-06): whether it rains, how
 * long until it turns and the random sequence; version 2 files read with a
 * new world's weather. */

static void put32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void reseal(uint8_t *p, size_t size)
{
    put32le(p + size - 4, save_crc32(p, size - 4));
}

static void sample(SaveInfo *info)
{
    int k;

    memset(info, 0, sizeof *info);
    info->seed = 4242u;
    info->y = 70.0f;
    for (k = 0; k < HOTBAR_SLOTS; k++)
        info->hotbar[k] = BLOCK_AIR;
    info->daytime = 0.4f;
    info->weather.raining = true;
    info->weather.left = 123.5f;
    info->weather.strength = 0.3f; /* not kept: reads as full rain */
    info->weather.clock = 55.0f;   /* not kept */
    info->weather.rng = 0xCAFEF00Du;
}

/* Pack info and read it back. */
static int round_trip(const SaveInfo *in, SaveInfo *out)
{
    uint8_t buf[SAVE_INFO_SIZE];

    save_info_pack(in, buf);
    return save_info_unpack(buf, sizeof buf, out);
}

static void test_kept(void)
{
    SaveInfo in, out;
    uint8_t buf[SAVE_INFO_SIZE];

    CHECK_EQ_INT(SAVE_INFO_SIZE, 59);
    sample(&in);
    save_info_pack(&in, buf);
    CHECK_EQ_INT(buf[4], 3); /* version 3 */
    CHECK_EQ_INT(buf[37 + HOTBAR_SLOTS], 1);
    CHECK_EQ_INT(save_info_unpack(buf, sizeof buf, &out), 1);
    CHECK_EQ_INT(out.weather.raining, 1);
    CHECK_NEAR(out.weather.left, 123.5, 0.0);
    CHECK_EQ_INT(out.weather.rng == 0xCAFEF00Du, 1);
    CHECK_NEAR(out.weather.strength, 1.0, 0.0);
    CHECK_NEAR(out.weather.clock, 0.0, 0.0);
    CHECK_NEAR(out.daytime, 0.4f, 0.0);
    in.weather.raining = false;
    in.weather.left = WEATHER_CLEAR_MAX; /* the longest clear spell */
    CHECK_EQ_INT(round_trip(&in, &out), 1);
    CHECK_EQ_INT(out.weather.raining, 0);
    CHECK_NEAR(out.weather.strength, 0.0, 0.0);
    CHECK_NEAR(out.weather.left, WEATHER_CLEAR_MAX, 0.0);
}

static void test_rejected(void)
{
    SaveInfo in, out;
    uint8_t buf[SAVE_INFO_SIZE];
    const float bad_left[5] = { 0.0f, -5.0f, NAN, INFINITY, WEATHER_RAIN_MAX + 1.0f };
    const uint8_t bad_byte[2] = { 2, 255 };
    int i;

    sample(&in);
    in.weather.left = WEATHER_RAIN_MAX;
    CHECK_EQ_INT(round_trip(&in, &out), 1);
    for (i = 0; i < 5; i++) { /* raining: a spell not over 0, or past the longest rain */
        in.weather.left = bad_left[i];
        CHECK_EQ_INT(round_trip(&in, &out), 0);
    }
    in.weather.raining = false;
    in.weather.left = WEATHER_RAIN_MAX + 1.0f; /* fine for a clear spell */
    CHECK_EQ_INT(round_trip(&in, &out), 1);
    in.weather.left = WEATHER_CLEAR_MAX + 1.0f;
    CHECK_EQ_INT(round_trip(&in, &out), 0);
    sample(&in);
    save_info_pack(&in, buf);
    for (i = 0; i < 2; i++) {
        buf[37 + HOTBAR_SLOTS] = bad_byte[i];
        reseal(buf, sizeof buf);
        CHECK_EQ_INT(save_info_unpack(buf, sizeof buf, &out), 0);
    }
    /* A failed read leaves *info as it was. */
    memset(&out, 0x5A, sizeof out);
    CHECK_EQ_INT(save_info_unpack(buf, sizeof buf, &out), 0);
    CHECK_EQ_INT(out.weather.rng == 0x5A5A5A5Au, 1);
}

/* A world.dat saved before row 3-06: version 2, SAVE_INFO_SIZE_V2 bytes. */
static void test_v2(void)
{
    SaveInfo in, out;
    Weather fresh;
    uint8_t buf[SAVE_INFO_SIZE], old[SAVE_INFO_SIZE_V2];
    size_t n;

    sample(&in);
    save_info_pack(&in, buf);
    memcpy(old, buf, SAVE_INFO_SIZE_V2 - 4); /* all but the weather and CRC */
    old[4] = 2;
    reseal(old, sizeof old);
    CHECK_EQ_INT(save_info_unpack(old, sizeof old, &out), 1);
    CHECK_NEAR(out.daytime, 0.4f, 0.0); /* the time of day is kept */
    weather_init(&fresh, in.seed);
    CHECK_EQ_INT(out.weather.raining == fresh.raining && out.weather.left == fresh.left &&
                     out.weather.rng == fresh.rng && out.weather.strength == fresh.strength &&
                     out.weather.clock == fresh.clock, 1);
    old[4] = 3; /* version 3 at version 2's size */
    reseal(old, sizeof old);
    CHECK_EQ_INT(save_info_unpack(old, sizeof old, &out), 0);
    buf[4] = 2; /* version 2 at version 3's size */
    reseal(buf, sizeof buf);
    CHECK_EQ_INT(save_info_unpack(buf, sizeof buf, &out), 0);
    for (n = SAVE_INFO_SIZE_V2 + 1; n < SAVE_INFO_SIZE; n++) {
        buf[4] = 3;
        reseal(buf, n);
        CHECK_EQ_INT(save_info_unpack(buf, n, &out), 0);
    }
}

void test_weather_save(void)
{
    test_kept();
    test_rejected();
    test_v2();
}
