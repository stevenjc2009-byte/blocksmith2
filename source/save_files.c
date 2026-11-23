#include "save.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MAKE_DIR(p) mkdir((p), 0777)
#endif

/* The SD card half of save.h: folders and files (the packing is in
 * save.c). */

/* One buffer for a chunk file, read or written (the game saves and loads
 * one file at a time). */
static uint8_t s_file[SAVE_CHUNK_FILE_MAX];

/* --- Files --- */

/* "<a>/<b>" into out; false if it does not fit. */
static bool join(char out[SAVE_PATH_MAX], const char *a, const char *b)
{
    int n = snprintf(out, SAVE_PATH_MAX, "%s/%s", a, b);

    return n > 0 && n < SAVE_PATH_MAX;
}

static bool is_dir(const char *path)
{
    DIR *d = opendir(path);

    if (d == NULL)
        return false;
    closedir(d);
    return true;
}

/* Make path and every folder above it (mkdir failing because one is
 * already there is fine); true if path is a folder afterwards. */
static bool make_dirs(const char *path)
{
    char part[SAVE_PATH_MAX];
    size_t i, n = strlen(path);

    if (n >= SAVE_PATH_MAX)
        return false;
    for (i = 1; i <= n; i++)
        if (path[i] == '/' || path[i] == '\0') {
            memcpy(part, path, i);
            part[i] = '\0';
            if (part[i - 1] != ':' && part[i - 1] != '/')
                MAKE_DIR(part);
        }
    return is_dir(path);
}

/* Write n bytes to path by way of path.tmp. */
static bool write_file(const char *path, const void *data, size_t n)
{
    char tmp[SAVE_PATH_MAX];
    FILE *f;
    bool ok;
    int len = snprintf(tmp, sizeof tmp, "%s.tmp", path);

    if (len <= 0 || len >= (int)sizeof tmp)
        return false;
    f = fopen(tmp, "wb");
    if (f == NULL)
        return false;
    ok = fwrite(data, 1, n, f) == n;
    ok = fclose(f) == 0 && ok;
    if (!ok) {
        remove(tmp);
        return false;
    }
    remove(path);
    return rename(tmp, path) == 0;
}

/* Up to cap bytes of path into buf: how many, or -1 if it cannot be read
 * or holds more than cap. */
static long read_one(const char *path, uint8_t *buf, size_t cap)
{
    FILE *f = fopen(path, "rb");
    size_t n;

    if (f == NULL)
        return -1;
    n = fread(buf, 1, cap, f);
    if (n == cap && fgetc(f) != EOF)
        n = cap + 1; /* too big */
    fclose(f);
    return n > cap ? -1 : (long)n;
}

/* path, or path.tmp, whichever first reads and passes ok(). */
static long read_file(const char *path, uint8_t *buf, size_t cap,
                      bool (*ok)(const uint8_t *buf, long n, void *arg), void *arg)
{
    char tmp[SAVE_PATH_MAX];
    long n = read_one(path, buf, cap);
    int len;

    if (n >= 0 && ok(buf, n, arg))
        return n;
    len = snprintf(tmp, sizeof tmp, "%s.tmp", path);
    if (len <= 0 || len >= (int)sizeof tmp)
        return -1;
    n = read_one(tmp, buf, cap);
    return n >= 0 && ok(buf, n, arg) ? n : -1;
}

bool save_root_make(const char *root)
{
    char worlds[SAVE_PATH_MAX];

    return make_dirs(root) && join(worlds, root, "worlds") && make_dirs(worlds);
}

static bool world_dir(char out[SAVE_PATH_MAX], const char *root, int id)
{
    int n = snprintf(out, SAVE_PATH_MAX, "%s/worlds/%d", root, id);

    return n > 0 && n < SAVE_PATH_MAX;
}

int save_free_id(const char *root)
{
    char dir[SAVE_PATH_MAX];
    int id;

    for (id = 1; id <= SAVE_WORLDS_MAX; id++)
        if (world_dir(dir, root, id) && !is_dir(dir))
            return id;
    return 0;
}

static bool info_ok(const uint8_t *buf, long n, void *arg)
{
    return n == SAVE_INFO_SIZE && save_info_unpack(buf, arg);
}

static bool read_info(const char *dir, SaveInfo *info)
{
    char path[SAVE_PATH_MAX];
    uint8_t buf[SAVE_INFO_SIZE];

    return join(path, dir, "world.dat") && read_file(path, buf, sizeof buf, info_ok, info) >= 0;
}

bool save_world_exists(const char *root, int id)
{
    char dir[SAVE_PATH_MAX];
    SaveInfo info;

    return id >= 1 && id <= SAVE_WORLDS_MAX && world_dir(dir, root, id) && read_info(dir, &info);
}

static bool store_load(void *ctx, Chunk *c, int cx, int cz)
{
    return save_chunk_read(ctx, c, cx, cz);
}

static bool store_save(void *ctx, const Chunk *c, int cx, int cz)
{
    return save_chunk_write(ctx, c, cx, cz);
}

bool save_world_open(SaveWorld *sw, const char *root, int id, bool make)
{
    DIR *d;
    struct dirent *e;
    bool ok = true;

    save_set_init(&sw->saved);
    sw->id = id;
    sw->store.load = store_load;
    sw->store.save = store_save;
    sw->store.ctx = sw;
    if (id < 1 || id > SAVE_WORLDS_MAX || !world_dir(sw->dir, root, id))
        return false;
    if (make && !make_dirs(sw->dir))
        return false;
    d = opendir(sw->dir);
    if (d == NULL)
        return false;
    while (ok && (e = readdir(d)) != NULL) {
        int cx, cz;

        if (save_chunk_name_parse(e->d_name, &cx, &cz))
            ok = save_set_add(&sw->saved, cx, cz);
    }
    closedir(d);
    if (!ok)
        save_world_close(sw);
    return ok;
}

void save_world_close(SaveWorld *sw)
{
    save_set_free(&sw->saved);
}

bool save_world_read(const SaveWorld *sw, SaveInfo *info)
{
    return read_info(sw->dir, info);
}

bool save_world_write(SaveWorld *sw, const SaveInfo *info)
{
    char path[SAVE_PATH_MAX];
    uint8_t buf[SAVE_INFO_SIZE];

    save_info_pack(info, buf);
    return join(path, sw->dir, "world.dat") && write_file(path, buf, sizeof buf);
}

typedef struct {
    int cx, cz;
    Chunk *c;
} ChunkWant;

/* A whole chunk file for the chunk wanted, its runs unpacked into it. */
static bool chunk_ok(const uint8_t *buf, long n, void *arg)
{
    const ChunkWant *want = arg;

    return save_chunk_unpack(buf, n, want->cx, want->cz, want->c);
}

static bool chunk_path(char out[SAVE_PATH_MAX], const SaveWorld *sw, int cx, int cz)
{
    char name[32];

    save_chunk_name(name, cx, cz);
    return join(out, sw->dir, name);
}

bool save_chunk_read(const SaveWorld *sw, Chunk *c, int cx, int cz)
{
    char path[SAVE_PATH_MAX];
    ChunkWant want;

    if (!save_set_has(&sw->saved, cx, cz) || !chunk_path(path, sw, cx, cz))
        return false;
    want.cx = cx;
    want.cz = cz;
    want.c = c;
    return read_file(path, s_file, sizeof s_file, chunk_ok, &want) >= 0;
}

bool save_chunk_write(SaveWorld *sw, const Chunk *c, int cx, int cz)
{
    char path[SAVE_PATH_MAX];
    size_t size = save_chunk_pack(c, cx, cz, s_file);

    /* Noted before writing: a write cut short can still leave a good .tmp. */
    return chunk_path(path, sw, cx, cz) && save_set_add(&sw->saved, cx, cz) &&
           write_file(path, s_file, size);
}

static bool settings_ok(const uint8_t *buf, long n, void *arg)
{
    return n == SAVE_SETTINGS_SIZE && save_settings_unpack(buf, arg);
}

bool save_settings_read(const char *root, SaveSettings *s)
{
    char path[SAVE_PATH_MAX];
    uint8_t buf[SAVE_SETTINGS_SIZE];

    return join(path, root, "settings.dat") &&
           read_file(path, buf, sizeof buf, settings_ok, s) >= 0;
}

bool save_settings_write(const char *root, const SaveSettings *s)
{
    char path[SAVE_PATH_MAX];
    uint8_t buf[SAVE_SETTINGS_SIZE];

    save_settings_pack(s, buf);
    return join(path, root, "settings.dat") && write_file(path, buf, sizeof buf);
}
