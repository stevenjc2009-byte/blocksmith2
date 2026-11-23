#ifndef BGJOB_H
#define BGJOB_H

#include <stdbool.h>

/* One job at a time on a second thread (fix batch, problems 5 and 6), so
 * SD card writes do not stop the game's frames: a save (savebg.c), a
 * backup or a restore (start.c). The thread runs at one step above the
 * game's priority: it spends nearly all its time waiting on the SD card,
 * so it should send the next request the moment one comes back. 3DS
 * only. */

/* Start fn(arg) on the thread. False, and nothing starts, while a job
 * started earlier has not been collected (bgjob_collect()). With no thread
 * to be had, fn runs here and now instead, and is collected the same
 * way. */
bool bgjob_start(int (*fn)(void *), void *arg);

/* Is a job started and not collected? (Running, or ended.) */
bool bgjob_started(void);

/* Wait until a started job has ended. At once with none. */
void bgjob_wait(void);

/* A job that has ended: its thread is freed, its result in *result, and
 * true (once per job). False if none has ended. */
bool bgjob_collect(int *result);

#endif
