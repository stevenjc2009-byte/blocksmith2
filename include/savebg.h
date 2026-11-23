#ifndef SAVEBG_H
#define SAVEBG_H

#include <stdbool.h>

#include "save.h"
#include "world.h"

/* The game's saves (fix batch, problem 5): SAVE and the autosave write
 * their files on bgjob.h's thread while the game goes on (save_job.h);
 * the lid, HOME, START and a new world save on the spot, as before, after
 * waiting for any save still being written. 3DS only. */

/* Give w its store: sw's, wrapped so that a chunk is never read or written
 * while a save may be writing its file (save_job_store_init()). */
void savebg_init(World *w, SaveWorld *sw);

/* Start a save of w (sw NULL: it has no folder) on the thread. False, and
 * nothing is done, while the last one is still to be collected
 * (savebg_poll()). */
bool savebg_start(World *w, SaveWorld *sw, const SaveInfo *info, const SaveSettings *s);

/* A save started earlier has been written: true once, its result in *ok.
 * Chunks it could not write are marked changed again. */
bool savebg_poll(World *w, bool *ok);

/* Save on the spot, once any save being written is done. True only if
 * every file was written (false with no folder). */
bool savebg_now(World *w, SaveWorld *sw, const SaveInfo *info, const SaveSettings *s);

#endif
