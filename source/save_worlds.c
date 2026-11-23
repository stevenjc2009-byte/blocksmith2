#include "save.h"
#include "save_io.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define REMOVE_DIR(p) _rmdir(p)
#else
#include <unistd.h>
#define REMOVE_DIR(p) rmdir(p)
#endif

/* The world list's half of the SD card (roadmap row 2-05): which worlds
 * there are, their names, deleting one. */

/* A delete goes over a folder until a pass finds nothing to remove, at
 * most this many times (a card may skip names removed while it lists). */
#define DELETE_PASSES 8

/* A world folder's number from its name: digits only, no leading zero,
 * 1 to SAVE_WORLDS_MAX; 0 for any other name. */
static int folder_id(const char *name)
{
    int id = 0;

    if (name[0] < '1' || name[0] > '9')
        return 0;
    for (; *name != '\0'; name++) {
        if (*name < '0' || *name > '9')
            return 0;
        id = id * 10 + (*name - '0');
        if (id > SAVE_WORLDS_MAX)
            return 0;
    }
    return id;
}

/* Put id in ids (n of them, lowest first, at most cap); returns the new n.
 * With no room, id takes the place of the highest if it is lower. */
static int insert_id(int *ids, int n, int cap, int id)
{
    int i;

    if (n == cap) {
        if (id > ids[n - 1])
            return n;
        n--; /* the highest goes */
    }
    for (i = n; i > 0 && ids[i - 1] > id; i--)
        ids[i] = ids[i - 1];
    ids[i] = id;
    return n + 1;
}

int save_world_ids(const char *root, int *ids, int cap)
{
    char worlds[SAVE_PATH_MAX];
    DIR *d;
    struct dirent *e;
    int n = 0;

    if (cap <= 0 || !save_join(worlds, root, "worlds"))
        return 0;
    d = opendir(worlds);
    if (d == NULL)
        return 0;
    while ((e = readdir(d)) != NULL) {
        int id = folder_id(e->d_name);

        if (id > 0 && save_world_exists(root, id))
            n = insert_id(ids, n, cap, id);
    }
    closedir(d);
    return n;
}

static bool name_path(char out[SAVE_PATH_MAX], const char *root, int id)
{
    char dir[SAVE_PATH_MAX];

    return id >= 1 && id <= SAVE_WORLDS_MAX && save_world_dir(dir, root, id) &&
           save_join(out, dir, "name.dat");
}

static bool name_ok(const uint8_t *buf, long n, void *arg)
{
    return n == SAVE_NAME_SIZE && save_name_unpack(buf, arg);
}

bool save_name_read(const char *root, int id, char name[WORLDNAME_SIZE])
{
    char path[SAVE_PATH_MAX];
    uint8_t buf[SAVE_NAME_SIZE];

    if (name_path(path, root, id) && save_read_file(path, buf, sizeof buf, name_ok, name) >= 0)
        return true;
    worldname_default(id, name);
    return false;
}

bool save_name_write(const char *root, int id, const char *name)
{
    char path[SAVE_PATH_MAX];
    uint8_t buf[SAVE_NAME_SIZE];

    if (!worldname_valid(name) || !name_path(path, root, id))
        return false;
    save_name_pack(name, buf);
    return save_write_file(path, buf, sizeof buf);
}

/* Remove every file in dir; how many went. */
static int remove_files(const char *dir)
{
    char path[SAVE_PATH_MAX];
    DIR *d = opendir(dir);
    struct dirent *e;
    int gone = 0;

    if (d == NULL)
        return 0;
    while ((e = readdir(d)) != NULL)
        if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0 &&
            save_join(path, dir, e->d_name) && remove(path) == 0)
            gone++;
    closedir(d);
    return gone;
}

bool save_world_delete(const char *root, int id)
{
    char dir[SAVE_PATH_MAX], path[SAVE_PATH_MAX];
    DIR *d;
    int pass;

    if (id < 1 || id > SAVE_WORLDS_MAX || !save_world_dir(dir, root, id))
        return false;
    /* From here on the world does not read, whatever is left. */
    if (save_join(path, dir, "world.dat"))
        remove(path);
    if (save_join(path, dir, "world.dat.tmp"))
        remove(path);
    for (pass = 0; pass < DELETE_PASSES && remove_files(dir) > 0; pass++)
        ;
    REMOVE_DIR(dir);
    d = opendir(dir);
    if (d == NULL)
        return true;
    closedir(d);
    return false;
}
