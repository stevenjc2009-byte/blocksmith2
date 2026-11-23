#include <math.h>
#include <string.h>

#include "block.h"
#include "stream.h"
#include "terrain.h"
#include "test.h"
#include "test_stream_harness.h"
#include "world.h"

/* The stream tests' world (test_stream_harness.h). */

World s_w;
bool s_meshed[WORLD_SLOTS][WORLD_SLOTS];
int s_loads, s_meshes, s_bad_mesh, s_runaway;
int s_view;

int view_used(void)
{
    return s_view < 2 ? 2 : s_view > 5 ? 5 : s_view;
}

int ring(int cx, int cz, int pcx, int pcz)
{
    int dx = cx > pcx ? cx - pcx : pcx - cx, dz = cz > pcz ? cz - pcz : pcz - cz;

    return dx > dz ? dx : dz;
}

bool run_job(int pcx, int pcz, StreamJob *job)
{
    if (!stream_next(&s_w, s_meshed, pcx, pcz, s_view, job))
        return false;
    if (job->kind == STREAM_LOAD) {
        world_load(&s_w, job->cx, job->cz);
        s_meshed[world_slot(job->cz)][world_slot(job->cx)] = false;
        s_loads++;
    } else {
        const Chunk *nb[NB_COUNT];

        world_neighbours(&s_w, job->cx, job->cz, nb);
        s_bad_mesh += world_chunk(&s_w, job->cx, job->cz) == NULL || !nb[0] || !nb[1] || !nb[2] ||
                      !nb[3] || s_meshed[world_slot(job->cz)][world_slot(job->cx)];
        s_meshed[world_slot(job->cz)][world_slot(job->cx)] = true;
        s_meshes++;
    }
    return true;
}

void tidy(int pcx, int pcz)
{
    int i, j;

    stream_put_away(&s_w, pcx, pcz, s_view);
    for (j = 0; j < WORLD_SLOTS; j++)
        for (i = 0; i < WORLD_SLOTS; i++)
            if (s_meshed[j][i] && !stream_keep_mesh(&s_w, i, j, pcx, pcz, s_view))
                s_meshed[j][i] = false;
}

void settle(int pcx, int pcz)
{
    StreamJob job;
    int n = 0;

    s_loads = s_meshes = 0;
    tidy(pcx, pcz);
    while (run_job(pcx, pcz, &job))
        if (++n > 1000) {
            s_runaway++;
            break;
        }
}

void fresh(uint32_t seed, int view)
{
    s_view = view;
    memset(&s_w, 0xA5, sizeof s_w);
    world_init(&s_w, seed);
    memset(s_meshed, 0, sizeof s_meshed);
    s_bad_mesh = s_runaway = s_loads = s_meshes = 0;
}

void check_settled(int pcx, int pcz, bool ground)
{
    static Chunk c;
    int view = view_used(), keep = view + 1;
    int dx, dz, i, j, missing = 0, wrong = 0, loaded = 0, meshed = 0, bad = 0;

    for (dz = -keep; dz <= keep; dz++)
        for (dx = -keep; dx <= keep; dx++) {
            const Chunk *got = world_chunk(&s_w, pcx + dx, pcz + dz);

            if (got == NULL) {
                missing++;
                continue;
            }
            if (ground) {
                terrain_fill_chunk(&c, s_w.seed, pcx + dx, pcz + dz);
                wrong += memcmp(got, &c, sizeof c) != 0;
            }
        }
    for (j = 0; j < WORLD_SLOTS; j++)
        for (i = 0; i < WORLD_SLOTS; i++) {
            const WorldSlot *s = &s_w.slots[j][i];

            loaded += s->loaded;
            if (s_meshed[j][i]) {
                meshed++;
                bad += !s->loaded || ring(s->cx, s->cz, pcx, pcz) > view;
            }
        }
    CHECK_EQ_INT(missing, 0);
    CHECK_EQ_INT(wrong, 0);
    CHECK_EQ_INT(loaded, (2 * keep + 1) * (2 * keep + 1));
    CHECK_EQ_INT(meshed, (2 * view + 1) * (2 * view + 1));
    CHECK_EQ_INT(bad, 0);
    CHECK_EQ_INT(s_bad_mesh, 0);
    CHECK_EQ_INT(s_runaway, 0);
}
