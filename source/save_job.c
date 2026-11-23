#include "save_job.h"
#include "save_io.h"

#include <stdlib.h>
#include <string.h>

/* A save packed to memory, then written (save_job.h). */

/* A new file at the end of the job: name in dir, n bytes to fill. NULL
 * (and the job counts a failure) if the path is too long, the job is full
 * or there is no memory. */
static SaveJobFile *add_file(SaveJob *j, const char *dir, const char *name, size_t n)
{
    SaveJobFile *f;

    if (j->n >= SAVE_JOB_FILES) {
        j->failed++;
        return NULL;
    }
    f = &j->file[j->n];
    memset(f, 0, sizeof *f);
    if (!save_join(f->path, dir, name) || (f->data = malloc(n)) == NULL) {
        j->failed++;
        return NULL;
    }
    f->n = n;
    j->n++;
    return f;
}

typedef struct {
    SaveJob *j;
    SaveWorld *sw;
} Packing;

static bool no_load(void *ctx, Chunk *c, int cx, int cz)
{
    (void)ctx;
    (void)c;
    (void)cx;
    (void)cz;
    return false;
}

/* The store world_save_changed() hands each changed chunk to at take: the
 * chunk's file is packed into the job. True if it was (or, with no memory
 * for it, written at once); false leaves the chunk changed. */
static bool pack_chunk(void *ctx, const Chunk *c, int cx, int cz)
{
    Packing *pk = ctx;
    SaveJob *j = pk->j;
    SaveJobFile *f;
    unsigned char *small;
    char name[32];

    if (j->n >= SAVE_JOB_FILES)
        return false;
    save_chunk_name(name, cx, cz);
    f = &j->file[j->n];
    memset(f, 0, sizeof *f);
    if (!save_join(f->path, pk->sw->dir, name))
        return false;
    f->data = malloc(SAVE_CHUNK_FILE_MAX);
    if (f->data == NULL)
        return save_chunk_write(pk->sw, c, cx, cz);
    /* Noted before writing, as save_chunk_write() does. */
    if (!save_set_add(&pk->sw->saved, cx, cz)) {
        free(f->data);
        f->data = NULL;
        return false;
    }
    f->n = save_chunk_pack(c, cx, cz, f->data);
    small = realloc(f->data, f->n);
    if (small != NULL)
        f->data = small;
    f->chunk = true;
    f->cx = cx;
    f->cz = cz;
    j->n++;
    return true;
}

void save_job_take(SaveJob *j, World *w, SaveWorld *sw, const SaveInfo *info, const char *root,
                   const SaveSettings *s)
{
    SaveJobFile *f;

    j->n = 0;
    j->failed = 0;
    if (sw != NULL) {
        Packing pk = { j, sw };
        WorldStore packer = { no_load, pack_chunk, &pk };
        const WorldStore *real = w->store;

        w->store = &packer;
        j->failed += world_save_changed(w);
        w->store = real;
        f = add_file(j, sw->dir, "world.dat", SAVE_INFO_SIZE);
        if (f != NULL)
            save_info_pack(info, f->data);
    } else {
        j->failed++;
    }
    f = add_file(j, root, "settings.dat", SAVE_SETTINGS_SIZE);
    if (f != NULL)
        save_settings_pack(s, f->data);
}

bool save_job_run(SaveJob *j)
{
    int i;

    for (i = 0; i < j->n; i++) {
        SaveJobFile *f = &j->file[i];

        f->written = save_write_file(f->path, f->data, f->n);
        if (!f->written)
            j->failed++;
    }
    return j->failed == 0;
}

bool save_job_end(SaveJob *j, World *w)
{
    bool ok = j->failed == 0;
    int i;

    for (i = 0; i < j->n; i++) {
        SaveJobFile *f = &j->file[i];

        if (f->chunk && !f->written) {
            WorldSlot *s = &w->slots[world_slot(f->cz)][world_slot(f->cx)];

            if (s->loaded && s->cx == f->cx && s->cz == f->cz)
                s->changed = true;
        }
        free(f->data);
        f->data = NULL;
    }
    j->n = 0;
    return ok;
}

static bool wait_load(void *ctx, Chunk *c, int cx, int cz)
{
    const SaveJobStore *h = ctx;

    if (save_set_has(h->saved, cx, cz))
        h->wait();
    return h->inner->load(h->inner->ctx, c, cx, cz);
}

static bool wait_save(void *ctx, const Chunk *c, int cx, int cz)
{
    const SaveJobStore *h = ctx;

    h->wait();
    return h->inner->save(h->inner->ctx, c, cx, cz);
}

void save_job_store_init(SaveJobStore *h, const WorldStore *inner, const SaveChunkSet *saved,
                         void (*wait)(void))
{
    h->store.load = wait_load;
    h->store.save = wait_save;
    h->store.ctx = h;
    h->inner = inner;
    h->saved = saved;
    h->wait = wait;
}
