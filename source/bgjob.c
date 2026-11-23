#include <3ds.h>

#include "bgjob.h"

/* One job at a time on a second thread (bgjob.h). */

/* fopen(), fwrite() and the SD card calls under them need little stack. */
#define BGJOB_STACK (32 * 1024)

static Thread s_thread;  /* NULL when the job ran here and now */
static bool s_started;   /* a job started and not collected */
static int s_done;       /* the job has ended (read with __atomic) */
static int s_result;
static int (*s_fn)(void *);
static void *s_arg;

static void entry(void *arg)
{
    (void)arg;
    s_result = s_fn(s_arg);
    __atomic_store_n(&s_done, 1, __ATOMIC_RELEASE);
}

bool bgjob_start(int (*fn)(void *), void *arg)
{
    s32 prio = 0x30;

    if (s_started)
        return false;
    s_fn = fn;
    s_arg = arg;
    s_done = 0;
    s_started = true;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    prio = prio - 1 < 0x18 ? 0x18 : prio - 1;
    /* -2: the game's own core; false: joined, not detached. */
    s_thread = threadCreate(entry, NULL, BGJOB_STACK, prio, -2, false);
    if (s_thread == NULL)
        entry(NULL);
    return true;
}

bool bgjob_started(void)
{
    return s_started;
}

void bgjob_wait(void)
{
    if (s_started && s_thread != NULL)
        threadJoin(s_thread, U64_MAX);
}

bool bgjob_collect(int *result)
{
    if (!s_started || !__atomic_load_n(&s_done, __ATOMIC_ACQUIRE))
        return false;
    if (s_thread != NULL) {
        threadJoin(s_thread, U64_MAX);
        threadFree(s_thread);
        s_thread = NULL;
    }
    s_started = false;
    *result = s_result;
    return true;
}
