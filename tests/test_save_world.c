#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define RM_DIR(p) _rmdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>
#define RM_DIR(p) rmdir(p)
#endif

#include "block.h"
#include "save.h"
#include "stream.h"
#include "terrain.h"
#include "test.h"
#include "world.h"

/* Row 2-04: saving and loading, the world handing chunks to a store (a fake
 * one, then real files in a folder of its own under build-host, wiped
 * first). The files themselves are tested in test_save_files.c. */

#define ROOT "build-host/savetest"

static Chunk s_a;
static World s_w;
static SaveWorld s_sw;

static int chunks_equal(const Chunk *a, const Chunk *b)
{
    return memcmp(a->blocks, b->blocks, sizeof a->blocks) == 0;
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

static int file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (f == NULL)
        return 0;
    fclose(f);
    return 1;
}

/* --- the world handing chunks to a store --- */

typedef struct {
    int loads, saves, refused;
    bool fail;
    int last_cx, last_cz;
} Fake;

static bool fake_load(void *ctx, Chunk *c, int cx, int cz)
{
    Fake *f = ctx;

    (void)c;
    (void)cx;
    (void)cz;
    f->loads++;
    return false;
}

static bool fake_save(void *ctx, const Chunk *c, int cx, int cz)
{
    Fake *f = ctx;

    (void)c;
    f->last_cx = cx;
    f->last_cz = cz;
    if (f->fail) {
        f->refused++;
        return false;
    }
    f->saves++;
    return true;
}

/* A block in chunk (cx, cz) at height y that is not air. */
static void solid_spot(int cx, int cz, int *x, int *y, int *z)
{
    *x = cx * CHUNK_SIZE + 5;
    *z = cz * CHUNK_SIZE + 6;
    *y = 1;
}

static void test_world_store(void)
{
    Fake f = { 0, 0, 0, false, 0, 0 };
    WorldStore store = { fake_load, fake_save, &f };
    int x, y, z, b;

    world_init(&s_w, 2026);
    world_set_store(&s_w, &store);
    world_load(&s_w, 0, 0);
    CHECK_EQ_INT(f.loads, 1);
    CHECK_EQ_INT(world_chunk_changed(&s_w, 0, 0), 0);
    solid_spot(0, 0, &x, &y, &z);
    b = world_get(&s_w, x, y, z);
    CHECK_EQ_INT(b != BLOCK_AIR, 1);
    world_set(&s_w, x, y, z, b); /* the same block: no change */
    CHECK_EQ_INT(world_chunk_changed(&s_w, 0, 0), 0);
    world_set(&s_w, x, y, z, BLOCK_AIR);
    CHECK_EQ_INT(world_chunk_changed(&s_w, 0, 0), 1);
    CHECK_EQ_INT(world_chunk_changed(&s_w, 1, 0), 0);
    world_unload(&s_w, 0, 0);
    CHECK_EQ_INT(f.saves, 1);
    CHECK_EQ_INT(f.last_cx == 0 && f.last_cz == 0, 1);
    CHECK_EQ_INT(s_w.lost, 0);
    CHECK_EQ_INT(world_chunk_changed(&s_w, 0, 0), 0);
    /* Unchanged: not handed over. */
    world_load(&s_w, 0, 0);
    world_unload(&s_w, 0, 0);
    CHECK_EQ_INT(f.saves, 1);
    /* Broken blocks count as changes. */
    world_load(&s_w, 0, 0);
    CHECK_EQ_INT(world_break(&s_w, x, y, z), 1);
    CHECK_EQ_INT(world_chunk_changed(&s_w, 0, 0), 1);
    /* A chunk put away by the one loaded into its slot goes first. */
    world_load(&s_w, WORLD_SLOTS, 0);
    CHECK_EQ_INT(f.saves, 2);
    CHECK_EQ_INT(f.last_cx == 0 && f.last_cz == 0, 1);
    world_unload(&s_w, WORLD_SLOTS, 0);
    CHECK_EQ_INT(f.saves, 2);
    /* The store refusing: counted as lost. */
    f.fail = true;
    world_load(&s_w, 0, 0);
    world_set(&s_w, x, y, z, BLOCK_AIR);
    world_unload(&s_w, 0, 0);
    CHECK_EQ_INT(s_w.lost, 1);
    CHECK_EQ_INT(f.refused, 1);
    s_w.lost = 0;
    /* A refused chunk put away by the one loaded into its slot: the new
     * chunk does not come in marked changed. */
    world_load(&s_w, 0, 0);
    world_set(&s_w, x, y, z, BLOCK_AIR);
    world_load(&s_w, WORLD_SLOTS, 0);
    CHECK_EQ_INT(s_w.lost, 1);
    CHECK_EQ_INT(world_chunk_changed(&s_w, WORLD_SLOTS, 0), 0);
    world_unload(&s_w, WORLD_SLOTS, 0);
    CHECK_EQ_INT(s_w.lost, 1);
    s_w.lost = 0;

    /* Saving every changed chunk. */
    f.fail = false;
    world_load(&s_w, 0, 0);
    world_load(&s_w, 1, 0);
    world_load(&s_w, 2, 0);
    world_set(&s_w, 0 * CHUNK_SIZE + 3, 2, 3, BLOCK_AIR);
    world_set(&s_w, 2 * CHUNK_SIZE + 3, 2, 3, BLOCK_AIR);
    CHECK_EQ_INT(world_save_changed(&s_w), 0);
    CHECK_EQ_INT(f.saves, 4);
    CHECK_EQ_INT(world_chunk_changed(&s_w, 0, 0) || world_chunk_changed(&s_w, 2, 0), 0);
    CHECK_EQ_INT(world_save_changed(&s_w), 0);
    CHECK_EQ_INT(f.saves, 4); /* nothing new */
    f.fail = true;
    world_set(&s_w, 1 * CHUNK_SIZE + 3, 2, 3, BLOCK_AIR);
    CHECK_EQ_INT(world_save_changed(&s_w), 1);
    CHECK_EQ_INT(world_chunk_changed(&s_w, 1, 0), 1); /* still to keep */
    f.fail = false;
    CHECK_EQ_INT(world_save_changed(&s_w), 0);
    CHECK_EQ_INT(f.saves, 5);
    CHECK_EQ_INT(s_w.lost, 0);
    /* The last slot column and row are saved too, and a chunk is changed
     * only at its own place, not at another sharing its slot. */
    world_load(&s_w, WORLD_SLOTS - 1, WORLD_SLOTS - 1);
    world_set(&s_w, (WORLD_SLOTS - 1) * CHUNK_SIZE + 3, 2, (WORLD_SLOTS - 1) * CHUNK_SIZE + 3, BLOCK_AIR);
    CHECK_EQ_INT(world_chunk_changed(&s_w, WORLD_SLOTS - 1, WORLD_SLOTS - 1), 1);
    CHECK_EQ_INT(world_chunk_changed(&s_w, WORLD_SLOTS - 1, -1), 0);
    CHECK_EQ_INT(world_chunk_changed(&s_w, -1, WORLD_SLOTS - 1), 0);
    CHECK_EQ_INT(world_save_changed(&s_w), 0);
    CHECK_EQ_INT(f.saves, 6);
    CHECK_EQ_INT(f.last_cx == WORLD_SLOTS - 1 && f.last_cz == WORLD_SLOTS - 1, 1);
    CHECK_EQ_INT(world_chunk_changed(&s_w, WORLD_SLOTS - 1, WORLD_SLOTS - 1), 0);

    /* No store: changes cannot be kept. */
    world_init(&s_w, 2026);
    CHECK_EQ_INT(s_w.store == NULL && s_w.lost == 0, 1);
    world_load(&s_w, 0, 0);
    world_set(&s_w, x, y, z, BLOCK_AIR);
    CHECK_EQ_INT(world_save_changed(&s_w), 1);
    world_unload(&s_w, 0, 0);
    CHECK_EQ_INT(s_w.lost, 1);
    world_load(&s_w, 0, 0);
    CHECK_EQ_INT(world_get(&s_w, x, y, z), b); /* made from the seed again */
    world_init(&s_w, 2026);
    CHECK_EQ_INT(s_w.lost, 0);
}

/* The world and the files together: a changed chunk put away comes back
 * from its file, in this world and in a new one opening the folder. */
static void test_world_files(void)
{
    int x, y, z, b, b2;

    wipe(ROOT);
    CHECK_EQ_INT(save_root_make(ROOT), 1);
    CHECK_EQ_INT(save_world_open(&s_sw, ROOT, 1, true), 1);
    world_init(&s_w, 2026);
    world_set_store(&s_w, &s_sw.store);
    world_load(&s_w, 0, 0);
    world_load(&s_w, 1, 0);
    solid_spot(0, 0, &x, &y, &z);
    b = world_get(&s_w, x, y, z);
    b2 = world_get(&s_w, x + 2, y, z);
    world_set(&s_w, x, y, z, BLOCK_AIR);
    world_unload(&s_w, 0, 0);
    world_unload(&s_w, 1, 0); /* unchanged: no file */
    CHECK_EQ_INT(s_w.lost, 0);
    CHECK_EQ_INT(s_sw.saved.count, 1);
    CHECK_EQ_INT(file_exists(ROOT "/worlds/1/x0z0.chk"), 1);
    CHECK_EQ_INT(file_exists(ROOT "/worlds/1/x1z0.chk"), 0);
    world_load(&s_w, 0, 0);
    CHECK_EQ_INT(world_get(&s_w, x, y, z), BLOCK_AIR);
    CHECK_EQ_INT(world_get(&s_w, x + 2, y, z), b2);
    CHECK_EQ_INT(b != BLOCK_AIR, 1);
    /* Changed again, kept by world_save_changed() while still loaded. */
    world_set(&s_w, x + 2, y, z, BLOCK_AIR);
    CHECK_EQ_INT(world_save_changed(&s_w), 0);
    save_world_close(&s_sw);

    /* A new world opening the folder: the changes, and the seed's ground
     * for every chunk never changed. */
    CHECK_EQ_INT(save_world_open(&s_sw, ROOT, 1, false), 1);
    world_init(&s_w, 2026);
    world_set_store(&s_w, &s_sw.store);
    world_load(&s_w, 0, 0);
    world_load(&s_w, 1, 0);
    CHECK_EQ_INT(world_get(&s_w, x, y, z), BLOCK_AIR);
    CHECK_EQ_INT(world_get(&s_w, x + 2, y, z), BLOCK_AIR);
    terrain_fill_chunk(&s_a, 2026, 1, 0);
    CHECK_EQ_INT(chunks_equal(world_chunk(&s_w, 1, 0), &s_a), 1);
    terrain_fill_chunk(&s_a, 2026, 0, 0);
    s_a.blocks[y][6][5] = BLOCK_AIR;
    s_a.blocks[y][6][7] = BLOCK_AIR;
    CHECK_EQ_INT(chunks_equal(world_chunk(&s_w, 0, 0), &s_a), 1);
    CHECK_EQ_INT(world_chunk_changed(&s_w, 0, 0), 0);
    save_world_close(&s_sw);
    wipe(ROOT);
}

void test_save_world(void)
{
    test_world_store();
    test_world_files();
}
