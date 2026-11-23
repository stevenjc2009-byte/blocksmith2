#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define RM_DIR(p) _rmdir(p)
#else
#include <unistd.h>
#define RM_DIR(p) rmdir(p)
#endif

#include "block.h"
#include "save.h"
#include "save_job.h"
#include "test.h"
#include "weather.h"
#include "world.h"

/* Fix batch, problem 5: a save packed to memory, then written
 * (save_job.h). Real files in folders of their own under build-host,
 * wiped first: a job must write exactly what a save on the spot writes. */

#define ROOT_A "build-host/savejob-a" /* saved on the spot, as before */
#define ROOT_B "build-host/savejob-b" /* saved by a job */
#define FILE_CAP (SAVE_CHUNK_FILE_MAX + 64)

static World s_w;
static SaveWorld s_sw;
static Chunk s_before;
static unsigned char s_fa[FILE_CAP], s_fb[FILE_CAP];

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

/* The whole of path into buf: its size, or -1. */
static long read_all(const char *path, unsigned char *buf)
{
    FILE *f = fopen(path, "rb");
    size_t n;

    if (f == NULL)
        return -1;
    n = fread(buf, 1, FILE_CAP, f);
    fclose(f);
    return (long)n;
}

/* Do the files in dir a and dir b match, name for name, byte for byte?
 * Returns how many files a has, or -1 at the first that differs or is
 * missing from b (or b has more). */
static int same_files(const char *a, const char *b)
{
    DIR *d = opendir(a);
    struct dirent *e;
    char pa[512], pb[512];
    int n = 0, nb = 0;

    if (d == NULL)
        return -1;
    while ((e = readdir(d)) != NULL) {
        long la, lb;

        if (e->d_name[0] == '.')
            continue;
        snprintf(pa, sizeof pa, "%s/%s", a, e->d_name);
        snprintf(pb, sizeof pb, "%s/%s", b, e->d_name);
        la = read_all(pa, s_fa);
        lb = read_all(pb, s_fb);
        if (la < 0 || la != lb || memcmp(s_fa, s_fb, (size_t)la) != 0) {
            closedir(d);
            return -1;
        }
        n++;
    }
    closedir(d);
    d = opendir(b);
    if (d == NULL)
        return -1;
    while ((e = readdir(d)) != NULL)
        nb += e->d_name[0] != '.';
    closedir(d);
    return nb == n ? n : -1;
}

static void make_info(SaveInfo *info)
{
    int k;

    memset(info, 0, sizeof *info);
    info->seed = 77;
    info->x = 1.5f;
    info->y = 40.0f;
    info->z = -3.25f;
    info->yaw = 0.5f;
    info->pitch = -0.2f;
    for (k = 0; k < HOTBAR_SLOTS; k++)
        info->hotbar[k] = BLOCK_STONE;
    info->chosen = 2;
    info->daytime = 0.25f;
    weather_init(&info->weather, 77);
}

/* A world with chunks (0..2, 0..1) loaded, three of them changed, given
 * root's world 1 (made new) as its store. */
static void make_world(const char *root)
{
    int cx, cz;

    wipe(root);
    CHECK_EQ_INT(save_root_make(root), 1);
    CHECK_EQ_INT(save_world_open(&s_sw, root, 1, true), 1);
    world_init(&s_w, 77);
    world_set_store(&s_w, &s_sw.store);
    for (cz = 0; cz < 2; cz++)
        for (cx = 0; cx < 3; cx++)
            world_load(&s_w, cx, cz);
    world_set(&s_w, 3, 2, 3, BLOCK_AIR);
    world_set(&s_w, 2 * CHUNK_SIZE + 5, WORLD_HEIGHT - 2, CHUNK_SIZE + 9, BLOCK_STONE);
    world_set(&s_w, CHUNK_SIZE + 1, 1, 4, BLOCK_GRASS);
}

static bool ends_with(const char *s, const char *end)
{
    size_t a = strlen(s), b = strlen(end);

    return a >= b && strcmp(s + a - b, end) == 0;
}

/* A job writes the same files a save on the spot does, in the order chunks,
 * world.dat, settings.dat; a chunk changed after the take stays changed and
 * its file holds it as it was at the take. */
static void test_job_matches_save(void)
{
    static SaveJob j;
    SaveInfo info;
    SaveSettings set = { 3, 1 };
    char da[SAVE_PATH_MAX], db[SAVE_PATH_MAX];
    int i;

    make_info(&info);
    make_world(ROOT_A);
    CHECK_EQ_INT(world_save_changed(&s_w), 0);
    CHECK_EQ_INT(save_world_write(&s_sw, &info), 1);
    CHECK_EQ_INT(save_settings_write(ROOT_A, &set), 1);
    snprintf(da, sizeof da, "%s", s_sw.dir);
    save_world_close(&s_sw);

    make_world(ROOT_B);
    save_job_take(&j, &s_w, &s_sw, &info, ROOT_B, &set);
    CHECK_EQ_INT(j.n, 5);
    CHECK_EQ_INT(j.failed, 0);
    for (i = 0; i < j.n - 2; i++)
        CHECK_EQ_INT(j.file[i].chunk, 1);
    CHECK_EQ_INT(ends_with(j.file[j.n - 2].path, "/world.dat"), 1);
    CHECK_EQ_INT(ends_with(j.file[j.n - 1].path, "/settings.dat"), 1);
    CHECK_EQ_INT(save_set_has(&s_sw.saved, 2, 1), 1); /* noted at the take */
    CHECK_EQ_INT(world_chunk_changed(&s_w, 0, 0) || world_chunk_changed(&s_w, 2, 1) ||
                 world_chunk_changed(&s_w, 1, 0), 0);
    /* Nothing is on the card until the job runs. */
    CHECK_EQ_INT(read_all(j.file[j.n - 2].path, s_fa), -1);
    /* Changed again before the job runs. */
    memcpy(&s_before, world_chunk(&s_w, 0, 0), sizeof s_before);
    world_set(&s_w, 3, 2, 3, BLOCK_DIRT);
    CHECK_EQ_INT(save_job_run(&j), 1);
    CHECK_EQ_INT(save_job_end(&j, &s_w), 1);
    CHECK_EQ_INT(j.n, 0);
    CHECK_EQ_INT(world_chunk_changed(&s_w, 0, 0), 1);
    CHECK_EQ_INT(world_chunk_changed(&s_w, 2, 1), 0);
    snprintf(db, sizeof db, "%s", s_sw.dir);
    CHECK_EQ_INT(same_files(da, db), 4); /* 3 chunks and world.dat */
    {
        unsigned char *sa = s_fa, *sb = s_fb;
        long la = read_all(ROOT_A "/settings.dat", sa), lb = read_all(ROOT_B "/settings.dat", sb);

        CHECK_EQ_INT(la == SAVE_SETTINGS_SIZE && la == lb && memcmp(sa, sb, (size_t)la) == 0, 1);
    }
    {
        static Chunk c;

        CHECK_EQ_INT(save_chunk_read(&s_sw, &c, 0, 0), 1);
        CHECK_EQ_INT(memcmp(c.blocks, s_before.blocks, sizeof c.blocks), 0);
    }
    save_world_close(&s_sw);
}

/* A file that cannot be written: the save fails, and its chunk is changed
 * again if it is still loaded (not one loaded since into its slot). */
static void test_job_write_fails(void)
{
    static SaveJob j;
    SaveInfo info;
    SaveSettings set = { 3, 1 };
    int i, chunks = 0;

    make_info(&info);
    make_world(ROOT_B);
    save_job_take(&j, &s_w, &s_sw, &info, ROOT_B, &set);
    CHECK_EQ_INT(j.n, 5);
    for (i = 0; i < j.n; i++)
        if (j.file[i].chunk) {
            snprintf(j.file[i].path, sizeof j.file[i].path, "%s/gone/x%dz%d.chk", ROOT_B,
                     j.file[i].cx, j.file[i].cz);
            chunks++;
        }
    CHECK_EQ_INT(chunks, 3);
    /* Chunk (2, 1) put away (unchanged now, so not saved) and another
     * chunk loaded into its slot. */
    world_unload(&s_w, 2, 1);
    world_load(&s_w, 2 + WORLD_SLOTS, 1);
    CHECK_EQ_INT(save_job_run(&j), 0);
    CHECK_EQ_INT(j.failed, 3);
    CHECK_EQ_INT(j.file[3].written && j.file[4].written, 1); /* world.dat, settings.dat */
    CHECK_EQ_INT(save_job_end(&j, &s_w), 0);
    CHECK_EQ_INT(world_chunk_changed(&s_w, 0, 0), 1);
    CHECK_EQ_INT(world_chunk_changed(&s_w, 1, 0), 1);
    CHECK_EQ_INT(world_chunk_changed(&s_w, 2 + WORLD_SLOTS, 1), 0);
    CHECK_EQ_INT(world_chunk_changed(&s_w, 1, 1), 0); /* never changed */
    save_world_close(&s_sw);
}

/* No world folder: only settings.dat, and the save counts as failed; the
 * world's changed chunks stay changed. A root too long for a path: nothing
 * to write. */
static void test_job_no_folder(void)
{
    static SaveJob j;
    SaveInfo info;
    SaveSettings set = { 4, 0 };
    char longroot[SAVE_PATH_MAX + 8];

    make_info(&info);
    make_world(ROOT_B);
    save_world_close(&s_sw);
    world_set_store(&s_w, NULL);
    remove(ROOT_B "/settings.dat");
    save_job_take(&j, &s_w, NULL, &info, ROOT_B, &set);
    CHECK_EQ_INT(j.n, 1);
    CHECK_EQ_INT(j.failed, 1);
    CHECK_EQ_INT(world_chunk_changed(&s_w, 0, 0), 1);
    CHECK_EQ_INT(save_job_run(&j), 0);
    CHECK_EQ_INT(j.file[0].written, 1);
    CHECK_EQ_INT(save_job_end(&j, &s_w), 0);
    CHECK_EQ_INT(read_all(ROOT_B "/settings.dat", s_fa), SAVE_SETTINGS_SIZE);

    memset(longroot, 'a', sizeof longroot - 1);
    longroot[sizeof longroot - 1] = '\0';
    save_job_take(&j, &s_w, NULL, &info, longroot, &set);
    CHECK_EQ_INT(j.n, 0);
    CHECK_EQ_INT(j.failed, 2);
    CHECK_EQ_INT(save_job_run(&j), 0);
    CHECK_EQ_INT(save_job_end(&j, &s_w), 0);
}

/* --- the store that waits for a job --- */

static int s_waits;

static void fake_wait(void)
{
    s_waits++;
}

typedef struct {
    int loads, saves;
    int waits_seen; /* s_waits when last called */
} Inner;

static bool inner_load(void *ctx, Chunk *c, int cx, int cz)
{
    Inner *in = ctx;

    (void)c;
    (void)cx;
    (void)cz;
    in->loads++;
    in->waits_seen = s_waits;
    return cx == 1;
}

static bool inner_save(void *ctx, const Chunk *c, int cx, int cz)
{
    Inner *in = ctx;

    (void)c;
    (void)cx;
    (void)cz;
    in->saves++;
    in->waits_seen = s_waits;
    return true;
}

static void test_job_store(void)
{
    Inner in = { 0, 0, 0 };
    WorldStore inner = { inner_load, inner_save, &in };
    SaveChunkSet set;
    SaveJobStore h;
    static Chunk c;

    save_set_init(&set);
    CHECK_EQ_INT(save_set_add(&set, 1, 2), 1);
    save_job_store_init(&h, &inner, &set, fake_wait);
    s_waits = 0;
    /* A chunk with a file: waits, then loads. */
    CHECK_EQ_INT(h.store.load(h.store.ctx, &c, 1, 2), 1);
    CHECK_EQ_INT(s_waits, 1);
    CHECK_EQ_INT(in.loads == 1 && in.waits_seen == 1, 1);
    /* One with none: no wait (the inner store still asked). */
    CHECK_EQ_INT(h.store.load(h.store.ctx, &c, 3, 3), 0);
    CHECK_EQ_INT(s_waits, 1);
    CHECK_EQ_INT(in.loads, 2);
    /* Saving always waits first. */
    CHECK_EQ_INT(h.store.save(h.store.ctx, &c, 3, 3), 1);
    CHECK_EQ_INT(s_waits, 2);
    CHECK_EQ_INT(in.saves == 1 && in.waits_seen == 2, 1);
    save_set_free(&set);
}

void test_save_job(void)
{
    test_job_matches_save();
    test_job_write_fails();
    test_job_no_folder();
    test_job_store();
    wipe(ROOT_A);
    wipe(ROOT_B);
}
