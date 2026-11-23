#ifndef SAVE_IO_H
#define SAVE_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "save.h"

/* The file helpers save_files.c, save_worlds.c and save_backup.c share
 * (save.h is what the game uses). */

/* "<a>/<b>" into out; false if it does not fit. */
bool save_join(char out[SAVE_PATH_MAX], const char *a, const char *b);

/* World id's folder, "<root>/worlds/<id>"; false if it does not fit. */
bool save_world_dir(char out[SAVE_PATH_MAX], const char *root, int id);

/* World id's backup folder, "<root>/backups/<id>"; false if it does not
 * fit. */
bool save_backup_dir(char out[SAVE_PATH_MAX], const char *root, int id);

/* Make path and every folder above it (mkdir failing because one is
 * already there is fine); true if path is a folder afterwards. */
bool save_make_dirs(const char *path);

/* Remove the files in dir, going over it until a pass finds nothing to
 * remove (a card may skip names removed while it lists). keep_world
 * keeps world.dat and world.dat.tmp; without it they go first, so from
 * then on nothing in dir reads as a world. */
void save_dir_clear(const char *dir, bool keep_world);

/* save_dir_clear(dir, false), then the folder. True if it is gone
 * afterwards. */
bool save_dir_delete(const char *dir);

/* Write n bytes to path by way of path.tmp. */
bool save_write_file(const char *path, const void *data, size_t n);

/* path, or path.tmp, whichever first reads (at most cap bytes into buf)
 * and passes ok(): how many bytes, or -1 if neither does. */
long save_read_file(const char *path, uint8_t *buf, size_t cap,
                    bool (*ok)(const uint8_t *buf, long n, void *arg), void *arg);

#endif
