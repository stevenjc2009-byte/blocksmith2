#include <dirent.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(p) _mkdir(p)
#define RM_DIR(p) _rmdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MAKE_DIR(p) mkdir((p), 0777)
#define RM_DIR(p) rmdir(p)
#endif

#include "block.h"
#include "save.h"
#include "test.h"
#include "worldname.h"

/* Row 2-05: the world list's half of saving: name.dat, which worlds there
 * are, renaming and deleting. The file tests work in a folder of their own
 * under build-host (wiped first), never on a real SD card. */

#define ROOT "build-host/savenames"

static Chunk s_c;
static SaveWorld s_sw;

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* A new CRC on the end of a packed name, after a test changed it. */
static void reseal(uint8_t *p)
{
    put32(p + SAVE_NAME_SIZE - 4, save_crc32(p, SAVE_NAME_SIZE - 4));
}

static int is_dir(const char *path)
{
    DIR *d = opendir(path);

    if (d == NULL)
        return 0;
    closedir(d);
    return 1;
}

static void wipe(const char *path)
{
    DIR *d = opendir(path);
    struct dirent *e;
    char sub[1024];

    if (d == NULL) {
        remove(path);
        return;
    }
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        snprintf(sub, sizeof sub, "%s/%s", path, e->d_name);
        wipe(sub);
    }
    closedir(d);
    RM_DIR(path);
}

static int file_write(const char *path, const uint8_t *buf, size_t n)
{
    FILE *f = fopen(path, "wb");
    int ok;

    if (f == NULL)
        return 0;
    ok = fwrite(buf, 1, n, f) == n;
    return fclose(f) == 0 && ok;
}

static int file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (f == NULL)
        return 0;
    fclose(f);
    return 1;
}

/* --- packing --- */

static void name_round_trip(const char *name)
{
    uint8_t buf[SAVE_NAME_SIZE];
    char got[WORLDNAME_SIZE];
    size_t n = strlen(name), i;
    int zeros = 1;

    memset(buf, 0xEE, sizeof buf);
    save_name_pack(name, buf);
    CHECK_EQ_INT(memcmp(buf, "BS2N", 4), 0);
    CHECK_EQ_INT(buf[4] == 1 && buf[5] == 0 && buf[6] == 0 && buf[7] == 0, 1);
    CHECK_EQ_INT(memcmp(buf + 8, name, n), 0);
    for (i = n; i < WORLDNAME_MAX; i++)
        zeros &= buf[8 + i] == 0;
    CHECK_EQ_INT(zeros, 1);
    memset(got, 'Q', sizeof got);
    CHECK_EQ_INT(save_name_unpack(buf, got), 1);
    CHECK_EQ_INT(strcmp(got, name), 0);
}

static void test_name_pack(void)
{
    uint8_t buf[SAVE_NAME_SIZE], good[SAVE_NAME_SIZE];
    char got[WORLDNAME_SIZE];

    CHECK_EQ_INT(SAVE_NAME_SIZE, 4 + 4 + WORLDNAME_MAX + 4);
    name_round_trip("WORLD 1");
    name_round_trip("A");
    name_round_trip("MY HOUSE");
    name_round_trip("ABCDEFGHIJKLMNOP"); /* 16: no '\0' in the file */
    name_round_trip("-9 Z-");
    save_name_pack("MY HOUSE", good);
    /* Each damage refuses it and leaves got alone. */
    memcpy(buf, good, sizeof buf);
    buf[0] = 'X';
    strcpy(got, "KEEP");
    CHECK_EQ_INT(save_name_unpack(buf, got), 0);
    CHECK_EQ_INT(strcmp(got, "KEEP"), 0);
    memcpy(buf, good, sizeof buf);
    buf[3] = 'S'; /* settings.dat's mark */
    reseal(buf);
    CHECK_EQ_INT(save_name_unpack(buf, got), 0);
    memcpy(buf, good, sizeof buf);
    buf[4] = 2; /* version 2 */
    reseal(buf);
    CHECK_EQ_INT(save_name_unpack(buf, got), 0);
    memcpy(buf, good, sizeof buf);
    buf[10] ^= 1; /* the CRC no longer fits */
    CHECK_EQ_INT(save_name_unpack(buf, got), 0);
    memcpy(buf, good, sizeof buf);
    buf[SAVE_NAME_SIZE - 1] ^= 0x80;
    CHECK_EQ_INT(save_name_unpack(buf, got), 0);
    memcpy(buf, good, sizeof buf);
    buf[8 + 12] = 'X'; /* after the name's end */
    reseal(buf);
    CHECK_EQ_INT(save_name_unpack(buf, got), 0);
    memcpy(buf, good, sizeof buf);
    buf[8 + WORLDNAME_MAX - 1] = 'X'; /* the last byte, after the end */
    reseal(buf);
    CHECK_EQ_INT(save_name_unpack(buf, got), 0);
    memcpy(buf, good, sizeof buf);
    buf[8] = 'm'; /* a small letter */
    reseal(buf);
    CHECK_EQ_INT(save_name_unpack(buf, got), 0);
    memcpy(buf, good, sizeof buf);
    memset(buf + 8, 0, WORLDNAME_MAX); /* no name */
    reseal(buf);
    CHECK_EQ_INT(save_name_unpack(buf, got), 0);
    memcpy(buf, good, sizeof buf);
    buf[8 + 2] = ' '; /* "MY  OUSE": two spaces */
    buf[8 + 3] = ' ';
    reseal(buf);
    CHECK_EQ_INT(save_name_unpack(buf, got), 0);
    CHECK_EQ_INT(strcmp(got, "KEEP"), 0);
}

/* --- files --- */

static void sample_info(SaveInfo *info, uint32_t seed)
{
    int k;

    info->seed = seed;
    info->x = 1.5f;
    info->y = 40.0f;
    info->z = -2.5f;
    info->yaw = 0.25f;
    info->pitch = -0.5f;
    for (k = 0; k < HOTBAR_SLOTS; k++)
        info->hotbar[k] = (uint8_t)(k % BLOCK_COUNT);
    info->chosen = 0;
    info->daytime = 0.5f;
    weather_init(&info->weather, info->seed);
}

/* World id made with a world.dat (seed id) and chunks chunk files. */
static void make_world(int id, int chunks)
{
    SaveInfo info;
    int k;

    sample_info(&info, (uint32_t)id);
    CHECK_EQ_INT(save_world_open(&s_sw, ROOT, id, true), 1);
    CHECK_EQ_INT(save_world_write(&s_sw, &info), 1);
    memset(&s_c, 0, sizeof s_c);
    for (k = 0; k < chunks; k++)
        CHECK_EQ_INT(save_chunk_write(&s_sw, &s_c, k, -k), 1);
    save_world_close(&s_sw);
}

static int ids_are(const int *got, int n, const int *want, int wn)
{
    return n == wn && memcmp(got, want, sizeof want[0] * (size_t)n) == 0;
}

static void test_ids(void)
{
    static const int want4[] = { 1, 2, 3, 12 }, want2[] = { 1, 2 }, want_hi[] = { 1, 2, 3, 12, 9999 };
    uint8_t dat[SAVE_INFO_SIZE];
    SaveInfo info;
    int ids[16], n;

    wipe(ROOT);
    CHECK_EQ_INT(save_world_ids(ROOT, ids, 16), 0); /* no root at all */
    CHECK_EQ_INT(save_root_make(ROOT), 1);
    CHECK_EQ_INT(save_world_ids(ROOT, ids, 16), 0);
    make_world(12, 0);
    make_world(3, 1);
    make_world(1, 0);
    make_world(2, 2);
    /* Folders that are not worlds: no world.dat, a damaged one, names that
     * are not world numbers (leading zero, past the last, letters, 0). */
    MAKE_DIR(ROOT "/worlds/7");
    make_world(8, 0);
    memset(dat, 0, sizeof dat);
    CHECK_EQ_INT(file_write(ROOT "/worlds/8/world.dat", dat, sizeof dat), 1);
    sample_info(&info, 5);
    save_info_pack(&info, dat);
    MAKE_DIR(ROOT "/worlds/005");
    CHECK_EQ_INT(file_write(ROOT "/worlds/005/world.dat", dat, sizeof dat), 1);
    MAKE_DIR(ROOT "/worlds/10000");
    CHECK_EQ_INT(file_write(ROOT "/worlds/10000/world.dat", dat, sizeof dat), 1);
    MAKE_DIR(ROOT "/worlds/4a");
    CHECK_EQ_INT(file_write(ROOT "/worlds/4a/world.dat", dat, sizeof dat), 1);
    MAKE_DIR(ROOT "/worlds/0");
    CHECK_EQ_INT(file_write(ROOT "/worlds/0/world.dat", dat, sizeof dat), 1);
    /* Names that, read carelessly, are worlds already listed (12, 3, and
     * 3 again past the int's end): each would be listed twice. */
    MAKE_DIR(ROOT "/worlds/0012");
    CHECK_EQ_INT(file_write(ROOT "/worlds/0012/world.dat", dat, sizeof dat), 1);
    MAKE_DIR(ROOT "/worlds/012");
    CHECK_EQ_INT(file_write(ROOT "/worlds/012/world.dat", dat, sizeof dat), 1);
    MAKE_DIR(ROOT "/worlds/1)"); /* 1 * 10 + (')' - '0') = 3 */
    CHECK_EQ_INT(file_write(ROOT "/worlds/1)/world.dat", dat, sizeof dat), 1);
    MAKE_DIR(ROOT "/worlds/4294967299"); /* 2^32 + 3 */
    CHECK_EQ_INT(file_write(ROOT "/worlds/4294967299/world.dat", dat, sizeof dat), 1);
    CHECK_EQ_INT(file_write(ROOT "/worlds/6", dat, sizeof dat), 1); /* a file, not a folder */
    n = save_world_ids(ROOT, ids, 16);
    CHECK_EQ_INT(ids_are(ids, n, want4, 4), 1);
    /* Only cap of them: the lowest. */
    n = save_world_ids(ROOT, ids, 2);
    CHECK_EQ_INT(ids_are(ids, n, want2, 2), 1);
    CHECK_EQ_INT(save_world_ids(ROOT, ids, 0), 0);
    /* A world whose world.dat is only the .tmp of a save cut short. */
    make_world(SAVE_WORLDS_MAX, 0);
    CHECK_EQ_INT(rename(ROOT "/worlds/9999/world.dat", ROOT "/worlds/9999/world.dat.tmp"), 0);
    n = save_world_ids(ROOT, ids, 16);
    CHECK_EQ_INT(ids_are(ids, n, want_hi, 5), 1);
    n = save_world_ids(ROOT, ids, 4);
    CHECK_EQ_INT(ids_are(ids, n, want4, 4), 1);
}

static void test_name_files(void)
{
    uint8_t buf[SAVE_NAME_SIZE];
    char name[WORLDNAME_SIZE];

    /* test_ids() left worlds 1, 2, 3, 12 and 9999. */
    CHECK_EQ_INT(save_name_read(ROOT, 2, name), 0);
    CHECK_EQ_INT(strcmp(name, "WORLD 2"), 0);
    CHECK_EQ_INT(save_name_write(ROOT, 2, "MY HOUSE"), 1);
    CHECK_EQ_INT(file_exists(ROOT "/worlds/2/name.dat"), 1);
    CHECK_EQ_INT(file_exists(ROOT "/worlds/2/name.dat.tmp"), 0);
    CHECK_EQ_INT(save_name_read(ROOT, 2, name), 1);
    CHECK_EQ_INT(strcmp(name, "MY HOUSE"), 0);
    CHECK_EQ_INT(save_name_read(ROOT, 3, name), 0); /* the others keep theirs */
    CHECK_EQ_INT(strcmp(name, "WORLD 3"), 0);
    CHECK_EQ_INT(save_name_write(ROOT, 2, "CASTLE"), 1); /* over the old one */
    CHECK_EQ_INT(save_name_read(ROOT, 2, name), 1);
    CHECK_EQ_INT(strcmp(name, "CASTLE"), 0);
    /* Refused: names worldname_valid() refuses, worlds that cannot be. */
    CHECK_EQ_INT(save_name_write(ROOT, 2, "castle"), 0);
    CHECK_EQ_INT(save_name_write(ROOT, 2, ""), 0);
    CHECK_EQ_INT(save_name_write(ROOT, 2, " CASTLE"), 0);
    CHECK_EQ_INT(save_name_write(ROOT, 0, "CASTLE"), 0);
    CHECK_EQ_INT(save_name_write(ROOT, SAVE_WORLDS_MAX + 1, "CASTLE"), 0);
    CHECK_EQ_INT(save_name_write(ROOT, 50, "CASTLE"), 0); /* no folder */
    CHECK_EQ_INT(is_dir(ROOT "/worlds/50"), 0);
    CHECK_EQ_INT(save_name_read(ROOT, 2, name), 1);
    CHECK_EQ_INT(strcmp(name, "CASTLE"), 0);
    CHECK_EQ_INT(save_name_read(ROOT, 0, name), 0);
    /* A damaged name.dat reads as none; its .tmp is taken if it reads. */
    save_name_pack("CASTLE", buf);
    buf[9] ^= 4;
    CHECK_EQ_INT(file_write(ROOT "/worlds/2/name.dat", buf, sizeof buf), 1);
    CHECK_EQ_INT(save_name_read(ROOT, 2, name), 0);
    CHECK_EQ_INT(strcmp(name, "WORLD 2"), 0);
    save_name_pack("TOWER", buf);
    CHECK_EQ_INT(file_write(ROOT "/worlds/2/name.dat.tmp", buf, sizeof buf), 1);
    CHECK_EQ_INT(save_name_read(ROOT, 2, name), 1);
    CHECK_EQ_INT(strcmp(name, "TOWER"), 0);
    /* A name.dat one byte too long is refused. */
    save_name_pack("CASTLE", buf);
    CHECK_EQ_INT(file_write(ROOT "/worlds/3/name.dat", buf, sizeof buf), 1);
    CHECK_EQ_INT(save_name_read(ROOT, 3, name), 1);
    {
        uint8_t big[SAVE_NAME_SIZE + 1];

        memcpy(big, buf, sizeof buf);
        big[SAVE_NAME_SIZE] = 0;
        CHECK_EQ_INT(file_write(ROOT "/worlds/3/name.dat", big, sizeof big), 1);
    }
    CHECK_EQ_INT(save_name_read(ROOT, 3, name), 0);
    /* ...and one a byte short. */
    CHECK_EQ_INT(file_write(ROOT "/worlds/3/name.dat", buf, sizeof buf - 1), 1);
    CHECK_EQ_INT(save_name_read(ROOT, 3, name), 0);
    CHECK_EQ_INT(strcmp(name, "WORLD 3"), 0);
}

static void test_delete(void)
{
    static const int left[] = { 1, 3, 12, 9999 }, left2[] = { 3, 12, 9999 };
    int ids[16], n;
    SaveInfo info;

    /* World 2: world.dat, two chunk files, name.dat and its .tmp, and a
     * stray .tmp; all go, then the folder. */
    CHECK_EQ_INT(file_exists(ROOT "/worlds/2/name.dat.tmp"), 1);
    CHECK_EQ_INT(file_write(ROOT "/worlds/2/x0z0.chk.tmp", (const uint8_t *)"x", 1), 1);
    CHECK_EQ_INT(save_world_delete(ROOT, 2), 1);
    CHECK_EQ_INT(is_dir(ROOT "/worlds/2"), 0);
    CHECK_EQ_INT(save_world_exists(ROOT, 2), 0);
    n = save_world_ids(ROOT, ids, 16);
    CHECK_EQ_INT(ids_are(ids, n, left, 4), 1);
    /* The others are as they were. */
    CHECK_EQ_INT(save_world_exists(ROOT, 3), 1);
    CHECK_EQ_INT(file_exists(ROOT "/worlds/3/x0z0.chk"), 1);
    /* Its number is free again for a new world. */
    CHECK_EQ_INT(save_free_id(ROOT), 2);
    /* A world of many chunk files. */
    make_world(1, 300);
    CHECK_EQ_INT(save_world_delete(ROOT, 1), 1);
    CHECK_EQ_INT(is_dir(ROOT "/worlds/1"), 0);
    n = save_world_ids(ROOT, ids, 16);
    CHECK_EQ_INT(ids_are(ids, n, left2, 3), 1);
    /* No such world: nothing to do, and the folder is not there. */
    CHECK_EQ_INT(save_world_delete(ROOT, 50), 1);
    CHECK_EQ_INT(save_world_delete(ROOT, 0), 0);
    CHECK_EQ_INT(save_world_delete(ROOT, SAVE_WORLDS_MAX + 1), 0);
    /* A folder inside cannot go: false, but world.dat went first, so the
     * world no longer reads. */
    MAKE_DIR(ROOT "/worlds/12/inner");
    CHECK_EQ_INT(file_write(ROOT "/worlds/12/inner/f", (const uint8_t *)"x", 1), 1);
    CHECK_EQ_INT(save_world_delete(ROOT, 12), 0);
    CHECK_EQ_INT(save_world_exists(ROOT, 12), 0);
    CHECK_EQ_INT(file_exists(ROOT "/worlds/12/world.dat.tmp"), 0);
    /* A world.dat only as .tmp goes as well. */
    CHECK_EQ_INT(save_world_delete(ROOT, 9999), 1);
    CHECK_EQ_INT(save_world_exists(ROOT, 9999), 0);
    CHECK_EQ_INT(save_world_open(&s_sw, ROOT, 3, false), 1);
    CHECK_EQ_INT(save_world_read(&s_sw, &info), 1);
    CHECK_EQ_INT((int)info.seed, 3);
    save_world_close(&s_sw);
    wipe(ROOT);
}

void test_save_names(void)
{
    test_name_pack();
    test_ids();
    test_name_files();
    test_delete();
}
