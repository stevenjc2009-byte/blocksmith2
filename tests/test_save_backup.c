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

/* Row 2-07: world backups, one per world in backups/<id>: making one,
 * putting the world back as it holds it, deleting it. The file tests work
 * in a folder of their own under build-host (wiped first), never on a real
 * SD card. */

#define ROOT "build-host/savebackup"
#define W1 ROOT "/worlds/1"
#define B1 ROOT "/backups/1"

static Chunk s_c;
static SaveWorld s_sw;
static uint8_t s_a[SAVE_CHUNK_FILE_MAX + 1], s_b[SAVE_CHUNK_FILE_MAX + 1];

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
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

/* path's bytes into buf (at most cap): how many, or -1. */
static long file_read(const char *path, uint8_t *buf, size_t cap)
{
    FILE *f = fopen(path, "rb");
    size_t n;

    if (f == NULL)
        return -1;
    n = fread(buf, 1, cap, f);
    fclose(f);
    return (long)n;
}

static int file_exists(const char *path)
{
    return file_read(path, s_b, 0) >= 0;
}

/* Do a and b both read, the same bytes? */
static int same(const char *a, const char *b)
{
    long na = file_read(a, s_a, sizeof s_a), nb = file_read(b, s_b, sizeof s_b);

    return na >= 0 && na == nb && memcmp(s_a, s_b, (size_t)na) == 0;
}

/* How many names in dir (not "." or ".."). */
static int names_in(const char *dir)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    int n = 0;

    if (d == NULL)
        return -1;
    while ((e = readdir(d)) != NULL)
        if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0)
            n++;
    closedir(d);
    return n;
}

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

/* World 1's world.dat with seed seed. */
static void write_info(uint32_t seed)
{
    SaveInfo info;

    sample_info(&info, seed);
    CHECK_EQ_INT(save_world_open(&s_sw, ROOT, 1, true), 1);
    CHECK_EQ_INT(save_world_write(&s_sw, &info), 1);
    save_world_close(&s_sw);
}

/* World 1's chunk (cx, cz) with block b at its foot. */
static void write_chunk(int cx, int cz, uint8_t b)
{
    memset(&s_c, 0, sizeof s_c);
    s_c.blocks[0][0][0] = b;
    CHECK_EQ_INT(save_world_open(&s_sw, ROOT, 1, true), 1);
    CHECK_EQ_INT(save_chunk_write(&s_sw, &s_c, cx, cz), 1);
    save_world_close(&s_sw);
}

/* World 1's seed as it reads, or 0. */
static uint32_t seed_of_world(void)
{
    SaveInfo info;
    uint32_t seed = 0;

    if (save_world_open(&s_sw, ROOT, 1, false) && save_world_read(&s_sw, &info))
        seed = info.seed;
    save_world_close(&s_sw);
    return seed;
}

static void test_make(void)
{
    char name[WORLDNAME_SIZE];
    long n;

    wipe(ROOT);
    CHECK_EQ_INT(save_root_make(ROOT), 1);
    /* No world 1: no backup, and no folder made for one. */
    CHECK_EQ_INT(save_backup_make(ROOT, 1), 0);
    CHECK_EQ_INT(is_dir(B1), 0);
    CHECK_EQ_INT(save_backup_exists(ROOT, 1), 0);
    /* World 1: world.dat, a name and three chunk files. */
    write_info(11);
    CHECK_EQ_INT(save_name_write(ROOT, 1, "CASTLE"), 1);
    write_chunk(0, 0, BLOCK_STONE);
    write_chunk(1, -1, BLOCK_DIRT);
    write_chunk(-2, 30, BLOCK_GRASS);
    CHECK_EQ_INT(save_backup_make(ROOT, 1), 1);
    CHECK_EQ_INT(save_backup_exists(ROOT, 1), 1);
    CHECK_EQ_INT(same(W1 "/world.dat", B1 "/world.dat"), 1);
    CHECK_EQ_INT(same(W1 "/name.dat", B1 "/name.dat"), 1);
    CHECK_EQ_INT(same(W1 "/x0z0.chk", B1 "/x0z0.chk"), 1);
    CHECK_EQ_INT(same(W1 "/x1z-1.chk", B1 "/x1z-1.chk"), 1);
    CHECK_EQ_INT(same(W1 "/x-2z30.chk", B1 "/x-2z30.chk"), 1);
    CHECK_EQ_INT(names_in(B1), 5); /* nothing else, no .tmp left */
    /* The world itself is as it was. */
    CHECK_EQ_INT(names_in(W1), 5);
    CHECK_EQ_INT((int)seed_of_world(), 11);
    /* Only what the game would read is copied: not a damaged chunk file,
     * one whose inside names another chunk, one with another mark or
     * version, or a file the game does not make (x0z0.chk.tmp.tmp); a chunk file there only as .tmp (a save cut short) is copied
     * as the file itself, and so is a damaged name.dat's whole .tmp. */
    CHECK_EQ_INT(file_write(W1 "/x5z5.chk", (const uint8_t *)"BS2Cjunk", 8), 1);
    n = file_read(W1 "/x0z0.chk", s_a, sizeof s_a);
    CHECK_EQ_INT(n > SAVE_CHUNK_HEAD + 4, 1);
    CHECK_EQ_INT(file_write(W1 "/x9z9.chk", s_a, (size_t)n), 1); /* chunk (0, 0) inside */
    CHECK_EQ_INT(file_write(W1 "/x0z0.chk.tmp.tmp", s_a, (size_t)n), 1); /* no such save */
    write_chunk(3, 3, BLOCK_STONE);
    n = file_read(W1 "/x3z3.chk", s_a, sizeof s_a);
    s_a[n / 2] ^= 1; /* a byte off: the CRC no longer fits */
    CHECK_EQ_INT(file_write(W1 "/x3z3.chk", s_a, (size_t)n), 1);
    write_chunk(4, 4, BLOCK_STONE);
    n = file_read(W1 "/x4z4.chk", s_a, sizeof s_a);
    put32(s_a + 16, (uint32_t)(n - SAVE_CHUNK_HEAD - 4 - 2)); /* the runs' length 2 short */
    put32(s_a + n - 4, save_crc32(s_a, (size_t)n - 4));
    CHECK_EQ_INT(file_write(W1 "/x4z4.chk", s_a, (size_t)n), 1);
    write_chunk(6, 6, BLOCK_STONE);
    n = file_read(W1 "/x6z6.chk", s_a, sizeof s_a);
    s_a[3] = 'X'; /* another kind of file's mark, resealed */
    put32(s_a + n - 4, save_crc32(s_a, (size_t)n - 4));
    CHECK_EQ_INT(file_write(W1 "/x6z6.chk", s_a, (size_t)n), 1);
    write_chunk(8, 8, BLOCK_STONE);
    n = file_read(W1 "/x8z8.chk", s_a, sizeof s_a);
    put32(s_a + 4, 2); /* a version this game does not know, resealed */
    put32(s_a + n - 4, save_crc32(s_a, (size_t)n - 4));
    CHECK_EQ_INT(file_write(W1 "/x8z8.chk", s_a, (size_t)n), 1);
    CHECK_EQ_INT(file_write(W1 "/notes.txt", (const uint8_t *)"hi", 2), 1);
    CHECK_EQ_INT(rename(W1 "/x1z-1.chk", W1 "/x1z-1.chk.tmp"), 0);
    CHECK_EQ_INT(file_read(W1 "/name.dat", s_b, sizeof s_b), SAVE_NAME_SIZE);
    CHECK_EQ_INT(file_write(W1 "/name.dat.tmp", s_b, SAVE_NAME_SIZE), 1);
    s_b[9] ^= 4;
    CHECK_EQ_INT(file_write(W1 "/name.dat", s_b, SAVE_NAME_SIZE), 1);
    /* A file in the backup from before goes. */
    CHECK_EQ_INT(file_write(B1 "/x7z7.chk", (const uint8_t *)"old", 3), 1);
    CHECK_EQ_INT(save_backup_make(ROOT, 1), 1);
    CHECK_EQ_INT(file_exists(B1 "/x5z5.chk"), 0);
    CHECK_EQ_INT(file_exists(B1 "/x9z9.chk"), 0);
    CHECK_EQ_INT(file_exists(B1 "/x3z3.chk"), 0);
    CHECK_EQ_INT(file_exists(B1 "/x4z4.chk"), 0);
    CHECK_EQ_INT(file_exists(B1 "/x6z6.chk"), 0);
    CHECK_EQ_INT(file_exists(B1 "/x8z8.chk"), 0);
    CHECK_EQ_INT(file_exists(B1 "/notes.txt"), 0);
    CHECK_EQ_INT(file_exists(B1 "/x7z7.chk"), 0);
    CHECK_EQ_INT(same(W1 "/x1z-1.chk.tmp", B1 "/x1z-1.chk"), 1);
    CHECK_EQ_INT(same(W1 "/name.dat.tmp", B1 "/name.dat"), 1);
    CHECK_EQ_INT(names_in(B1), 5);
    /* World.dat there only as .tmp: copied as world.dat. */
    CHECK_EQ_INT(rename(W1 "/world.dat", W1 "/world.dat.tmp"), 0);
    CHECK_EQ_INT(save_backup_make(ROOT, 1), 1);
    CHECK_EQ_INT(same(W1 "/world.dat.tmp", B1 "/world.dat"), 1);
    CHECK_EQ_INT(file_exists(B1 "/world.dat.tmp"), 0);
    CHECK_EQ_INT(rename(W1 "/world.dat.tmp", W1 "/world.dat"), 0);
    /* Put the world straight for the tests after. */
    CHECK_EQ_INT(rename(W1 "/x1z-1.chk.tmp", W1 "/x1z-1.chk"), 0);
    remove(W1 "/x5z5.chk");
    remove(W1 "/x9z9.chk");
    remove(W1 "/x0z0.chk.tmp.tmp");
    remove(W1 "/x3z3.chk");
    remove(W1 "/x4z4.chk");
    remove(W1 "/x6z6.chk");
    remove(W1 "/x8z8.chk");
    remove(W1 "/notes.txt");
    remove(W1 "/name.dat.tmp");
    CHECK_EQ_INT(save_name_write(ROOT, 1, "CASTLE"), 1);
    CHECK_EQ_INT(save_backup_make(ROOT, 1), 1);
    CHECK_EQ_INT(save_name_read(ROOT, 1, name), 1);
    CHECK_EQ_INT(strcmp(name, "CASTLE"), 0);
    /* Worlds that cannot be. */
    CHECK_EQ_INT(save_backup_make(ROOT, 0), 0);
    CHECK_EQ_INT(save_backup_make(ROOT, SAVE_WORLDS_MAX + 1), 0);
    CHECK_EQ_INT(save_backup_exists(ROOT, 0), 0);
    CHECK_EQ_INT(save_backup_exists(ROOT, SAVE_WORLDS_MAX + 1), 0);
}

static void test_restore(void)
{
    char name[WORLDNAME_SIZE];

    /* test_make() left world 1 (seed 11, CASTLE, chunks x0z0 x1z-1
     * x-2z30) and its backup the same. Then the world changes: a new
     * seed, a chunk changed, one added, one gone, the name gone. */
    write_info(22);
    write_chunk(0, 0, BLOCK_GRASS);
    write_chunk(7, 7, BLOCK_STONE);
    CHECK_EQ_INT(remove(W1 "/x-2z30.chk"), 0);
    CHECK_EQ_INT(remove(W1 "/name.dat"), 0);
    CHECK_EQ_INT(save_backup_restore(ROOT, 1), 1);
    CHECK_EQ_INT((int)seed_of_world(), 11);
    CHECK_EQ_INT(same(W1 "/world.dat", B1 "/world.dat"), 1);
    CHECK_EQ_INT(same(W1 "/x0z0.chk", B1 "/x0z0.chk"), 1);
    CHECK_EQ_INT(same(W1 "/x-2z30.chk", B1 "/x-2z30.chk"), 1);
    CHECK_EQ_INT(same(W1 "/x1z-1.chk", B1 "/x1z-1.chk"), 1);
    CHECK_EQ_INT(file_exists(W1 "/x7z7.chk"), 0);
    CHECK_EQ_INT(save_name_read(ROOT, 1, name), 1);
    CHECK_EQ_INT(strcmp(name, "CASTLE"), 0);
    CHECK_EQ_INT(names_in(W1), 5);
    /* The backup is kept, as it was. */
    CHECK_EQ_INT(save_backup_exists(ROOT, 1), 1);
    CHECK_EQ_INT(names_in(B1), 5);
    /* A name given after the backup goes too (the backup had none). */
    CHECK_EQ_INT(remove(B1 "/name.dat"), 0);
    CHECK_EQ_INT(save_backup_restore(ROOT, 1), 1);
    CHECK_EQ_INT(file_exists(W1 "/name.dat"), 0);
    CHECK_EQ_INT(save_name_read(ROOT, 1, name), 0);
    CHECK_EQ_INT(strcmp(name, "WORLD 1"), 0);
    /* The backup's world.dat there only as .tmp: it is a backup. */
    CHECK_EQ_INT(rename(B1 "/world.dat", B1 "/world.dat.tmp"), 0);
    CHECK_EQ_INT(save_backup_exists(ROOT, 1), 1);
    write_info(33);
    CHECK_EQ_INT(save_backup_restore(ROOT, 1), 1);
    CHECK_EQ_INT((int)seed_of_world(), 11);
    CHECK_EQ_INT(file_exists(W1 "/world.dat.tmp"), 0);
    /* Its world.dat damaged: no backup, nothing restored, the world as it
     * was. */
    CHECK_EQ_INT(file_read(B1 "/world.dat.tmp", s_a, sizeof s_a), SAVE_INFO_SIZE);
    s_a[20] ^= 2;
    CHECK_EQ_INT(file_write(B1 "/world.dat.tmp", s_a, SAVE_INFO_SIZE), 1);
    CHECK_EQ_INT(save_backup_exists(ROOT, 1), 0);
    write_info(44);
    write_chunk(7, 7, BLOCK_STONE);
    CHECK_EQ_INT(save_backup_restore(ROOT, 1), 0);
    CHECK_EQ_INT((int)seed_of_world(), 44);
    CHECK_EQ_INT(file_exists(W1 "/x7z7.chk"), 1);
    /* Worlds that cannot be. */
    CHECK_EQ_INT(save_backup_restore(ROOT, 0), 0);
    CHECK_EQ_INT(save_backup_restore(ROOT, SAVE_WORLDS_MAX + 1), 0);
    CHECK_EQ_INT(save_backup_restore(ROOT, 2), 0);
    CHECK_EQ_INT(is_dir(ROOT "/worlds/2"), 0);
}

static void test_cut_short(void)
{
    /* A backup cut short is no backup: a folder where a chunk file must go
     * stops the copy, and the old backup's world.dat went first. */
    remove(W1 "/x7z7.chk");
    write_info(55);
    CHECK_EQ_INT(save_backup_make(ROOT, 1), 1);
    CHECK_EQ_INT(remove(B1 "/x1z-1.chk"), 0);
    CHECK_EQ_INT(MAKE_DIR(B1 "/x1z-1.chk"), 0);
    CHECK_EQ_INT(file_write(B1 "/x1z-1.chk/f", (const uint8_t *)"x", 1), 1);
    CHECK_EQ_INT(save_backup_make(ROOT, 1), 0);
    CHECK_EQ_INT(save_backup_exists(ROOT, 1), 0);
    CHECK_EQ_INT(file_exists(B1 "/world.dat"), 0);
    CHECK_EQ_INT(file_exists(B1 "/world.dat.tmp"), 0);
    wipe(B1 "/x1z-1.chk");
    CHECK_EQ_INT(save_backup_make(ROOT, 1), 1);
    /* A restore cut short leaves a world that reads: its world.dat stays
     * until the rest is in (restore again to finish). */
    write_info(66);
    CHECK_EQ_INT(remove(W1 "/x1z-1.chk"), 0);
    CHECK_EQ_INT(MAKE_DIR(W1 "/x1z-1.chk"), 0);
    CHECK_EQ_INT(file_write(W1 "/x1z-1.chk/f", (const uint8_t *)"x", 1), 1);
    CHECK_EQ_INT(save_backup_restore(ROOT, 1), 0);
    CHECK_EQ_INT(save_world_exists(ROOT, 1), 1);
    CHECK_EQ_INT((int)seed_of_world(), 66);
    wipe(W1 "/x1z-1.chk");
    CHECK_EQ_INT(save_backup_restore(ROOT, 1), 1);
    CHECK_EQ_INT((int)seed_of_world(), 55);
    CHECK_EQ_INT(same(W1 "/x1z-1.chk", B1 "/x1z-1.chk"), 1);
}

static void test_delete(void)
{
    /* Deleting a backup: its folder goes, the world stays. */
    CHECK_EQ_INT(save_backup_exists(ROOT, 1), 1);
    CHECK_EQ_INT(save_backup_delete(ROOT, 1), 1);
    CHECK_EQ_INT(is_dir(B1), 0);
    CHECK_EQ_INT(save_backup_exists(ROOT, 1), 0);
    CHECK_EQ_INT(save_world_exists(ROOT, 1), 1);
    CHECK_EQ_INT(save_backup_delete(ROOT, 1), 1); /* none: nothing to do */
    CHECK_EQ_INT(save_backup_delete(ROOT, 0), 0);
    CHECK_EQ_INT(save_backup_delete(ROOT, SAVE_WORLDS_MAX + 1), 0);
    /* Deleting a world deletes its backup too. */
    CHECK_EQ_INT(save_backup_make(ROOT, 1), 1);
    CHECK_EQ_INT(save_world_delete(ROOT, 1), 1);
    CHECK_EQ_INT(is_dir(W1), 0);
    CHECK_EQ_INT(is_dir(B1), 0);
    /* A backup folder left with no world (a delete cut short) keeps its
     * number from a new world, so no new world finds an old backup. */
    CHECK_EQ_INT(save_free_id(ROOT), 1);
    CHECK_EQ_INT(save_root_make(ROOT), 1);
    MAKE_DIR(ROOT "/backups");
    CHECK_EQ_INT(MAKE_DIR(B1), 0);
    CHECK_EQ_INT(save_free_id(ROOT), 2);
    wipe(ROOT);
}

void test_save_backup(void)
{
    test_make();
    test_restore();
    test_cut_short();
    test_delete();
}
