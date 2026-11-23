#include "bgjob.h"
#include "save_job.h"
#include "savebg.h"

/* The game's saves on a second thread (savebg.h). */

static SaveJob s_job;
static SaveJobStore s_hz;
static bool s_mine; /* the job started on bgjob.h's thread is a save */

static int run_job(void *arg)
{
    return save_job_run(arg);
}

void savebg_init(World *w, SaveWorld *sw)
{
    save_job_store_init(&s_hz, &sw->store, &sw->saved, bgjob_wait);
    world_set_store(w, &s_hz.store);
}

bool savebg_start(World *w, SaveWorld *sw, const SaveInfo *info, const SaveSettings *s)
{
    if (bgjob_started())
        return false;
    save_job_take(&s_job, w, sw, info, SAVE_ROOT, s);
    s_mine = bgjob_start(run_job, &s_job);
    return s_mine;
}

bool savebg_poll(World *w, bool *ok)
{
    int result;

    if (!s_mine || !bgjob_collect(&result))
        return false;
    s_mine = false;
    *ok = save_job_end(&s_job, w);
    return true;
}

bool savebg_now(World *w, SaveWorld *sw, const SaveInfo *info, const SaveSettings *s)
{
    bool ok;

    if (s_mine) {
        bgjob_wait();
        savebg_poll(w, &ok); /* this save writes what that one could not */
    }
    save_job_take(&s_job, w, sw, info, SAVE_ROOT, s);
    save_job_run(&s_job);
    return save_job_end(&s_job, w);
}
