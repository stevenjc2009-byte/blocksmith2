#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
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
#include "stream.h"
#include "terrain.h"
#include "test.h"
#include "world.h"

/* Row 2-04: saving and loading, the files half. The file tests work in a
 * folder of their own under build-host (wiped first), never on a real SD
 * card. The packing is tested in test_save.c. */

#define ROOT "build-host/savetest"

static Chunk s_a, s_b, s_c;
static SaveWorld s_sw;
static uint8_t s_filebuf[SAVE_CHUNK_HEAD + SAVE_RLE_MAX + 4 + 16];

static int chunks_equal(const Chunk *a, const Chunk *b)
{
    return memcmp(a->blocks, b->blocks, sizeof a->blocks) == 0;
}

/* --- files the tests look at and damage --- */

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

/* --- files --- */

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

static void chunk_path(char *out, int id, int cx, int cz)
{
    char name[32];

    save_chunk_name(name, cx, cz);
    snprintf(out, SAVE_PATH_MAX, ROOT "/worlds/%d/%s", id, name);
}

static void test_files(void)
{
    SaveInfo in, out;
    SaveSettings st, st2;
    char path[SAVE_PATH_MAX], tmp[SAVE_PATH_MAX + 8];
    long n;

    wipe(ROOT);
    CHECK_EQ_INT(is_dir(ROOT), 0);
    CHECK_EQ_INT(save_root_make(ROOT), 1);
    CHECK_EQ_INT(is_dir(ROOT "/worlds"), 1);
    CHECK_EQ_INT(save_root_make(ROOT), 1); /* already there is fine */

    /* World numbers. */
    CHECK_EQ_INT(save_free_id(ROOT), 1);
    CHECK_EQ_INT(save_world_exists(ROOT, 1), 0);
    CHECK_EQ_INT(save_world_open(&s_sw, ROOT, 1, false), 0); /* no folder */
    CHECK_EQ_INT(save_world_open(&s_sw, ROOT, 1, true), 1);
    CHECK_EQ_INT(s_sw.id, 1);
    CHECK_EQ_INT(s_sw.saved.count, 0);
    CHECK_EQ_INT(save_free_id(ROOT), 2);
    CHECK_EQ_INT(save_world_exists(ROOT, 1), 0); /* a folder, no world.dat yet */
    CHECK_EQ_INT(save_world_open(&s_sw, ROOT, 0, true), 0);
    CHECK_EQ_INT(save_world_open(&s_sw, ROOT, SAVE_WORLDS_MAX + 1, true), 0);
    CHECK_EQ_INT(save_world_exists(ROOT, 0) || save_world_exists(ROOT, SAVE_WORLDS_MAX + 1), 0);
    CHECK_EQ_INT(save_world_open(&s_sw, ROOT, 1, false), 1);

    /* world.dat. */
    CHECK_EQ_INT(save_world_read(&s_sw, &out), 0);
    sample_info(&in);
    CHECK_EQ_INT(save_world_write(&s_sw, &in), 1);
    CHECK_EQ_INT(save_world_exists(ROOT, 1), 1);
    memset(&out, 0, sizeof out);
    CHECK_EQ_INT(save_world_read(&s_sw, &out), 1);
    CHECK_EQ_INT(info_equal(&in, &out), 1);
    CHECK_EQ_INT(file_exists(ROOT "/worlds/1/world.dat.tmp"), 0); /* renamed into place */
    in.seed = 99;
    CHECK_EQ_INT(save_world_write(&s_sw, &in), 1); /* over the old one */
    CHECK_EQ_INT(save_world_read(&s_sw, &out) && out.seed == 99, 1);
    /* A good world.dat in folder 0 and in the last folder: only the last
     * is a world. */
    n = file_read(ROOT "/worlds/1/world.dat", s_filebuf, sizeof s_filebuf);
    CHECK_EQ_INT((int)n, SAVE_INFO_SIZE);
    CHECK_EQ_INT(MAKE_DIR(ROOT "/worlds/0"), 0);
    CHECK_EQ_INT(file_write(ROOT "/worlds/0/world.dat", s_filebuf, (size_t)n), 1);
    CHECK_EQ_INT(save_world_exists(ROOT, 0), 0);
    snprintf(path, sizeof path, ROOT "/worlds/%d", SAVE_WORLDS_MAX);
    CHECK_EQ_INT(MAKE_DIR(path), 0);
    snprintf(tmp, sizeof tmp, ROOT "/worlds/%d/world.dat", SAVE_WORLDS_MAX);
    CHECK_EQ_INT(file_write(tmp, s_filebuf, (size_t)n), 1);
    CHECK_EQ_INT(save_world_exists(ROOT, SAVE_WORLDS_MAX), 1);
    wipe(ROOT "/worlds/0");
    wipe(path);
    /* One byte too many: not read. */
    s_filebuf[n] = 0;
    CHECK_EQ_INT(file_write(ROOT "/worlds/1/world.dat", s_filebuf, (size_t)n + 1), 1);
    CHECK_EQ_INT(save_world_read(&s_sw, &out) || save_world_exists(ROOT, 1), 0);
    CHECK_EQ_INT(file_write(ROOT "/worlds/1/world.dat", s_filebuf, (size_t)n), 1);
    CHECK_EQ_INT(save_world_read(&s_sw, &out) && out.seed == 99, 1);

    /* A chunk. */
    terrain_fill_chunk(&s_a, 2026, 3, -2);
    s_a.blocks[5][6][7] = BLOCK_AIR;
    s_a.blocks[120][0][15] = BLOCK_STONE;
    CHECK_EQ_INT(save_chunk_read(&s_sw, &s_b, 3, -2), 0);
    CHECK_EQ_INT(save_chunk_write(&s_sw, &s_a, 3, -2), 1);
    CHECK_EQ_INT(save_set_has(&s_sw.saved, 3, -2), 1);
    memset(s_b.blocks, 0x55, sizeof s_b.blocks);
    CHECK_EQ_INT(save_chunk_read(&s_sw, &s_b, 3, -2), 1);
    CHECK_EQ_INT(chunks_equal(&s_a, &s_b), 1);
    CHECK_EQ_INT(save_chunk_read(&s_sw, &s_b, 4, -2), 0);
    CHECK_EQ_INT(save_chunk_read(&s_sw, &s_b, -2, 3), 0);
    chunk_path(path, 1, 3, -2);
    CHECK_EQ_INT(file_exists(path), 1);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    CHECK_EQ_INT(file_exists(tmp), 0);

    /* Opened again: the chunk file is found by its name. */
    save_world_close(&s_sw);
    CHECK_EQ_INT(s_sw.saved.count, 0);
    save_world_close(&s_sw); /* twice is fine */
    CHECK_EQ_INT(save_world_open(&s_sw, ROOT, 1, false), 1);
    CHECK_EQ_INT(s_sw.saved.count, 1);
    CHECK_EQ_INT(save_set_has(&s_sw.saved, 3, -2), 1);
    memset(s_b.blocks, 0x55, sizeof s_b.blocks);
    CHECK_EQ_INT(save_chunk_read(&s_sw, &s_b, 3, -2), 1);
    CHECK_EQ_INT(chunks_equal(&s_a, &s_b), 1);

    /* A save cut short after the old file went: only <name>.tmp is left,
     * and it reads, before and after opening again. */
    terrain_fill_chunk(&s_c, 2026, 5, 5);
    s_c.blocks[1][1][1] = BLOCK_AIR;
    CHECK_EQ_INT(save_chunk_write(&s_sw, &s_c, 5, 5), 1);
    chunk_path(path, 1, 5, 5);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    CHECK_EQ_INT(rename(path, tmp), 0);
    memset(s_b.blocks, 0x55, sizeof s_b.blocks);
    CHECK_EQ_INT(save_chunk_read(&s_sw, &s_b, 5, 5), 1);
    CHECK_EQ_INT(chunks_equal(&s_c, &s_b), 1);
    save_world_close(&s_sw);
    CHECK_EQ_INT(save_world_open(&s_sw, ROOT, 1, false), 1);
    CHECK_EQ_INT(s_sw.saved.count, 2);
    memset(s_b.blocks, 0x55, sizeof s_b.blocks);
    CHECK_EQ_INT(save_chunk_read(&s_sw, &s_b, 5, 5), 1);
    CHECK_EQ_INT(chunks_equal(&s_c, &s_b), 1);
    /* Saved again: in place, the .tmp gone. */
    CHECK_EQ_INT(save_chunk_write(&s_sw, &s_c, 5, 5), 1);
    CHECK_EQ_INT(file_exists(path) && !file_exists(tmp), 1);

    /* A damaged file does not read (nothing to fall back on)... */
    chunk_path(path, 1, 3, -2);
    n = file_read(path, s_filebuf, sizeof s_filebuf);
    CHECK_EQ_INT(n > SAVE_CHUNK_HEAD + 4, 1);
    s_filebuf[n / 2] ^= 0x10;
    CHECK_EQ_INT(file_write(path, s_filebuf, (size_t)n), 1);
    CHECK_EQ_INT(save_chunk_read(&s_sw, &s_b, 3, -2), 0);
    /* ...unless a good <name>.tmp is there. */
    s_filebuf[n / 2] ^= 0x10;
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    CHECK_EQ_INT(file_write(tmp, s_filebuf, (size_t)n), 1);
    memset(s_b.blocks, 0x55, sizeof s_b.blocks);
    CHECK_EQ_INT(save_chunk_read(&s_sw, &s_b, 3, -2), 1);
    CHECK_EQ_INT(chunks_equal(&s_a, &s_b), 1);
    remove(tmp);
    /* Cut short, or holding another chunk's blocks: no. */
    CHECK_EQ_INT(file_write(path, s_filebuf, (size_t)n / 2), 1);
    CHECK_EQ_INT(save_chunk_read(&s_sw, &s_b, 3, -2), 0);
    CHECK_EQ_INT(file_write(path, s_filebuf, (size_t)n), 1);
    CHECK_EQ_INT(save_chunk_read(&s_sw, &s_b, 3, -2), 1); /* (put back) */
    chunk_path(tmp, 1, 7, 7);
    CHECK_EQ_INT(file_write(tmp, s_filebuf, (size_t)n), 1); /* (3, -2)'s file as (7, 7)'s */
    save_world_close(&s_sw);
    CHECK_EQ_INT(save_world_open(&s_sw, ROOT, 1, false), 1);
    CHECK_EQ_INT(save_set_has(&s_sw.saved, 7, 7), 1);
    CHECK_EQ_INT(save_chunk_read(&s_sw, &s_b, 7, 7), 0);
    /* A byte too many: no. */
    s_filebuf[n] = 0;
    CHECK_EQ_INT(file_write(path, s_filebuf, (size_t)n + 1), 1);
    CHECK_EQ_INT(save_chunk_read(&s_sw, &s_b, 3, -2), 0);
    /* A good file the folder scan never saw (made behind the save's back)
     * is not looked for until the world is opened again. */
    n = (long)save_chunk_pack(&s_a, 8, 8, s_filebuf);
    chunk_path(tmp, 1, 8, 8);
    CHECK_EQ_INT(file_write(tmp, s_filebuf, (size_t)n), 1);
    CHECK_EQ_INT(save_chunk_read(&s_sw, &s_b, 8, 8), 0);
    save_world_close(&s_sw);
    CHECK_EQ_INT(save_world_open(&s_sw, ROOT, 1, false), 1);
    memset(s_b.blocks, 0x55, sizeof s_b.blocks);
    CHECK_EQ_INT(save_chunk_read(&s_sw, &s_b, 8, 8), 1);
    CHECK_EQ_INT(chunks_equal(&s_a, &s_b), 1);

    /* world.dat damaged, its .tmp good. */
    n = file_read(ROOT "/worlds/1/world.dat", s_filebuf, sizeof s_filebuf);
    CHECK_EQ_INT((int)n, SAVE_INFO_SIZE);
    CHECK_EQ_INT(file_write(ROOT "/worlds/1/world.dat.tmp", s_filebuf, (size_t)n), 1);
    s_filebuf[20] ^= 1;
    CHECK_EQ_INT(file_write(ROOT "/worlds/1/world.dat", s_filebuf, (size_t)n), 1);
    CHECK_EQ_INT(save_world_read(&s_sw, &out) && out.seed == 99, 1);
    remove(ROOT "/worlds/1/world.dat.tmp");
    CHECK_EQ_INT(save_world_read(&s_sw, &out), 0);
    CHECK_EQ_INT(save_world_exists(ROOT, 1), 0);

    /* Another world keeps its own chunks. */
    CHECK_EQ_INT(save_free_id(ROOT), 2);
    save_world_close(&s_sw);
    CHECK_EQ_INT(save_world_open(&s_sw, ROOT, 2, true), 1);
    CHECK_EQ_INT(s_sw.saved.count, 0);
    CHECK_EQ_INT(save_chunk_read(&s_sw, &s_b, 5, 5), 0);
    CHECK_EQ_INT(save_free_id(ROOT), 3);
    CHECK_EQ_INT(MAKE_DIR(ROOT "/worlds/4"), 0);
    CHECK_EQ_INT(save_free_id(ROOT), 3); /* the lowest free number */
    save_world_close(&s_sw);

    /* settings.dat. */
    CHECK_EQ_INT(save_settings_read(ROOT, &st2), 0);
    st.view = 2;
    st.last = 4;
    CHECK_EQ_INT(save_settings_write(ROOT, &st), 1);
    st2.view = st2.last = -1;
    CHECK_EQ_INT(save_settings_read(ROOT, &st2), 1);
    CHECK_EQ_INT(st2.view == 2 && st2.last == 4, 1);
    st.view = 5;
    CHECK_EQ_INT(save_settings_write(ROOT, &st), 1);
    CHECK_EQ_INT(save_settings_read(ROOT, &st2) && st2.view == 5, 1);
    n = file_read(ROOT "/settings.dat", s_filebuf, sizeof s_filebuf);
    CHECK_EQ_INT((int)n, SAVE_SETTINGS_SIZE);
    s_filebuf[8] ^= 3;
    CHECK_EQ_INT(file_write(ROOT "/settings.dat", s_filebuf, (size_t)n), 1);
    CHECK_EQ_INT(save_settings_read(ROOT, &st2), 0);

    /* A root that cannot be there (a file in the way). */
    CHECK_EQ_INT(file_write(ROOT "/afile", s_filebuf, 1), 1);
    CHECK_EQ_INT(save_root_make(ROOT "/afile/sub"), 0);
    wipe(ROOT);
}

/* Every world number used: the last one is still given, then none. Makes
 * SAVE_WORLDS_MAX - 1 empty folders (a few seconds). */
static void test_free_id_full(void)
{
    char dir[SAVE_PATH_MAX];
    int id, made = 0;

    wipe(ROOT);
    CHECK_EQ_INT(save_root_make(ROOT), 1);
    for (id = 1; id < SAVE_WORLDS_MAX; id++) {
        snprintf(dir, sizeof dir, ROOT "/worlds/%d", id);
        made += MAKE_DIR(dir) == 0;
    }
    CHECK_EQ_INT(made, SAVE_WORLDS_MAX - 1);
    CHECK_EQ_INT(save_free_id(ROOT), SAVE_WORLDS_MAX);
    snprintf(dir, sizeof dir, ROOT "/worlds/%d", SAVE_WORLDS_MAX);
    CHECK_EQ_INT(MAKE_DIR(dir), 0);
    CHECK_EQ_INT(save_free_id(ROOT), 0);
    wipe(ROOT);
}

void test_save_files(void)
{
    test_files();
    test_free_id_full();
}
