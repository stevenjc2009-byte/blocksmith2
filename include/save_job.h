#ifndef SAVE_JOB_H
#define SAVE_JOB_H

#include <stdbool.h>
#include <stddef.h>

#include "save.h"
#include "world.h"

/* A save split in two (fix batch, problem 5: a save stopped the game for
 * about 0.2 s of SD card calls). save_job_take() packs every file a save
 * writes into memory, on the game's thread, in one go: the world as it
 * stands at that moment, as a save always took it. save_job_run() then
 * writes those files and touches nothing but the job and the SD card, so
 * a second thread can run it (savebg.c) while the game goes on.
 * save_job_end(), back on the game's thread, marks the chunks whose file
 * could not be written changed again (the next save tries them again, as
 * a save always did) and frees the job. Pure C: the PC tests run it. */

/* Every chunk the world holds, then world.dat and settings.dat. */
#define SAVE_JOB_FILES (WORLD_SLOTS * WORLD_SLOTS + 2)

typedef struct {
    char path[SAVE_PATH_MAX];
    unsigned char *data; /* malloc()ed, n bytes */
    size_t n;
    bool chunk;          /* a chunk file, for chunk (cx, cz) */
    int cx, cz;
    bool written;        /* save_job_run() wrote it */
} SaveJobFile;

typedef struct {
    SaveJobFile file[SAVE_JOB_FILES]; /* the order they are written in */
    int n;
    int failed; /* files not packed or not written; with no world folder, 1 more */
} SaveJob;

/* Take a save: every changed chunk of w for sw's folder (each noted in
 * sw->saved and no longer marked changed; one with no memory to pack it
 * in is written at once instead, as before), then world.dat from info,
 * then settings.dat from s in root. sw NULL: the world has no folder, so
 * only settings.dat (and the save counts as failed, as it always has).
 * Call it with no job of sw's being written. */
void save_job_take(SaveJob *j, World *w, SaveWorld *sw, const SaveInfo *info, const char *root,
                   const SaveSettings *s);

/* Write the job's files, in order. True if every one was written and
 * nothing failed at take. */
bool save_job_run(SaveJob *j);

/* After save_job_run(): mark each chunk whose file was not written changed
 * again, if it is still loaded, then free the job. Returns what
 * save_job_run() did. */
bool save_job_end(SaveJob *j, World *w);

/* A store to give the world while jobs run (world_set_store()): it hands
 * on to inner, but first calls wait() (which returns once no job is being
 * written) before a chunk is saved, and before one is loaded from the SD
 * card (one in saved), so no file is read or written while a job may be
 * writing it. Loading a chunk that never had a file does not wait. */
typedef struct {
    WorldStore store; /* give the world this */
    const WorldStore *inner;
    const SaveChunkSet *saved;
    void (*wait)(void);
} SaveJobStore;

void save_job_store_init(SaveJobStore *h, const WorldStore *inner, const SaveChunkSet *saved,
                         void (*wait)(void));

#endif
