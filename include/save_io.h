#ifndef SAVE_IO_H
#define SAVE_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "save.h"

/* The file helpers save_files.c and save_worlds.c share (save.h is what
 * the game uses). */

/* "<a>/<b>" into out; false if it does not fit. */
bool save_join(char out[SAVE_PATH_MAX], const char *a, const char *b);

/* World id's folder, "<root>/worlds/<id>"; false if it does not fit. */
bool save_world_dir(char out[SAVE_PATH_MAX], const char *root, int id);

/* Write n bytes to path by way of path.tmp. */
bool save_write_file(const char *path, const void *data, size_t n);

/* path, or path.tmp, whichever first reads (at most cap bytes into buf)
 * and passes ok(): how many bytes, or -1 if neither does. */
long save_read_file(const char *path, uint8_t *buf, size_t cap,
                    bool (*ok)(const uint8_t *buf, long n, void *arg), void *arg);

#endif
