#include "save.h"
#include "save_io.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

/* World backups (roadmap row 2-07, save.h): a world's files copied to its
 * backup folder, and back. */

/* Every file copied goes through here, one at a time (a chunk file is the
 * biggest there is). */
static uint8_t s_copy[SAVE_CHUNK_FILE_MAX];

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* Mark, version 1 and the CRC-32 at the end, as save.c seals a file. */
static bool sealed(const uint8_t *buf, long n, const char *mark)
{
    return n >= 12 && memcmp(buf, mark, 4) == 0 && get32(buf + 4) == 1u &&
           get32(buf + n - 4) == save_crc32(buf, (size_t)n - 4);
}

/* Is buf (n bytes) a whole file of the kind its name (arg) says, as the
 * game would take it: world.dat and name.dat unpack, a chunk file is
 * sealed, for the chunk in its name, with its runs' length right (its runs
 * are left to the game: a chunk file that will not unpack counts as never
 * saved). */
static bool whole(const uint8_t *buf, long n, void *arg)
{
    const char *name = arg;
    char word[WORLDNAME_SIZE];
    SaveInfo info;
    int cx, cz;

    if (strcmp(name, "world.dat") == 0)
        return n >= 0 && save_info_unpack(buf, (size_t)n, &info);
    if (strcmp(name, "name.dat") == 0)
        return n == SAVE_NAME_SIZE && save_name_unpack(buf, word);
    return save_chunk_name_parse(name, &cx, &cz) && n >= SAVE_CHUNK_HEAD + 4 &&
           sealed(buf, n, "BS2C") && (int32_t)get32(buf + 8) == cx &&
           (int32_t)get32(buf + 12) == cz && get32(buf + 16) == (uint32_t)(n - SAVE_CHUNK_HEAD - 4);
}

static bool exists(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (f == NULL)
        return false;
    fclose(f);
    return true;
}

/* from/name (or from/name.tmp, as the game reads it) to to/name, if either
 * is whole; false only if it could not be written. */
static bool copy_file(const char *from, const char *to, const char *name)
{
    char src[SAVE_PATH_MAX], dst[SAVE_PATH_MAX];
    long n;

    if (!save_join(src, from, name) || !save_join(dst, to, name))
        return false;
    n = save_read_file(src, s_copy, sizeof s_copy, whole, (void *)name);
    return n < 0 || save_write_file(dst, s_copy, (size_t)n);
}

/* Copy from's name.dat and chunk files to to (world.dat is the caller's,
 * last). One there only as <name>.tmp (a save cut short) is copied from
 * that; other files are left. False if any could not be written. */
static bool copy_files(const char *from, const char *to)
{
    char name[SAVE_PATH_MAX], plain[SAVE_PATH_MAX];
    DIR *d = opendir(from);
    struct dirent *e;
    bool ok = true;
    int cx, cz;

    if (d == NULL)
        return false;
    while ((e = readdir(d)) != NULL) {
        size_t n = strlen(e->d_name);

        if (n >= sizeof name)
            continue;
        memcpy(name, e->d_name, n + 1);
        if (n > 4 && strcmp(name + n - 4, ".tmp") == 0) {
            name[n - 4] = '\0';
            /* the file itself is there too: copied when its name comes */
            if (!save_join(plain, from, name) || exists(plain))
                continue;
        }
        n = strlen(name);
        if (n > 4 && strcmp(name + n - 4, ".tmp") == 0)
            continue;
        if (strcmp(name, "name.dat") == 0 || save_chunk_name_parse(name, &cx, &cz))
            ok = copy_file(from, to, name) && ok;
    }
    closedir(d);
    return ok;
}

static bool dirs(const char *root, int id, char world[SAVE_PATH_MAX], char backup[SAVE_PATH_MAX])
{
    return id >= 1 && id <= SAVE_WORLDS_MAX && save_world_dir(world, root, id) &&
           save_backup_dir(backup, root, id);
}

bool save_backup_exists(const char *root, int id)
{
    char world[SAVE_PATH_MAX], backup[SAVE_PATH_MAX], path[SAVE_PATH_MAX];

    return dirs(root, id, world, backup) && save_join(path, backup, "world.dat") &&
           save_read_file(path, s_copy, SAVE_INFO_SIZE, whole, "world.dat") >= 0;
}

bool save_backup_make(const char *root, int id)
{
    char world[SAVE_PATH_MAX], backup[SAVE_PATH_MAX];

    if (!dirs(root, id, world, backup) || !save_world_exists(root, id) || !save_make_dirs(backup))
        return false;
    save_dir_clear(backup, false); /* its world.dat first: no backup from here */
    return copy_files(world, backup) && copy_file(world, backup, "world.dat") &&
           save_backup_exists(root, id);
}

bool save_backup_restore(const char *root, int id)
{
    char world[SAVE_PATH_MAX], backup[SAVE_PATH_MAX];

    if (!dirs(root, id, world, backup) || !save_backup_exists(root, id) || !save_make_dirs(world))
        return false;
    save_dir_clear(world, true); /* world.dat kept: the world still reads */
    return copy_files(backup, world) && copy_file(backup, world, "world.dat") &&
           save_world_exists(root, id);
}

bool save_backup_delete(const char *root, int id)
{
    char world[SAVE_PATH_MAX], backup[SAVE_PATH_MAX];

    return dirs(root, id, world, backup) && save_dir_delete(backup);
}
