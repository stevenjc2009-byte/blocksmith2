#include "stream.h"

#include <stdlib.h>

int stream_view(int view)
{
    return view < STREAM_VIEW_MIN ? STREAM_VIEW_MIN : view > STREAM_VIEW_MAX ? STREAM_VIEW_MAX : view;
}

int stream_ring(int cx, int cz, int pcx, int pcz)
{
    int dx = abs(cx - pcx), dz = abs(cz - pcz);

    return dx > dz ? dx : dz;
}

void stream_put_away(World *w, int pcx, int pcz, int view)
{
    int i, j, keep = stream_view(view) + 1;

    for (j = 0; j < WORLD_SLOTS; j++)
        for (i = 0; i < WORLD_SLOTS; i++) {
            const WorldSlot *s = &w->slots[j][i];

            if (s->loaded && stream_ring(s->cx, s->cz, pcx, pcz) > keep)
                world_unload(w, s->cx, s->cz);
        }
}

bool stream_keep_mesh(const World *w, int i, int j, int pcx, int pcz, int view)
{
    const WorldSlot *s = &w->slots[j][i];

    return s->loaded && stream_ring(s->cx, s->cz, pcx, pcz) <= stream_view(view);
}

static bool loaded(const World *w, int cx, int cz)
{
    return world_chunk(w, cx, cz) != NULL;
}

/* Loaded and not meshed yet. Its four neighbours are always loaded by then:
 * stream_next() only meshes ring d - 1 once every chunk out to ring d is
 * loaded, and a chunk's neighbours are at most one ring further out. */
static bool meshable(const World *w, const bool meshed[WORLD_SLOTS][WORLD_SLOTS], int cx, int cz)
{
    return loaded(w, cx, cz) && !meshed[world_slot(cz)][world_slot(cx)];
}

/* The nearest chunk in ring d round (pcx, pcz) that needs loading (load) or
 * is ready for its mesh (!load). False if there is none. */
static bool find(const World *w, const bool meshed[WORLD_SLOTS][WORLD_SLOTS], int pcx, int pcz,
                 int d, bool load, int *cx, int *cz)
{
    int dx, dz, best = -1;

    for (dz = -d; dz <= d; dz++)
        for (dx = -d; dx <= d; dx++) {
            int x = pcx + dx, z = pcz + dz, r2 = dx * dx + dz * dz;

            if (dx != -d && dx != d && dz != -d && dz != d)
                continue; /* inside the ring, not on it */
            if (load ? loaded(w, x, z) : !meshable(w, meshed, x, z))
                continue;
            if (best < 0 || r2 < best) {
                best = r2;
                *cx = x;
                *cz = z;
            }
        }
    return best >= 0;
}

bool stream_next(const World *w, const bool meshed[WORLD_SLOTS][WORLD_SLOTS], int pcx, int pcz,
                 int view, StreamJob *job)
{
    int d, keep = stream_view(view) + 1;

    for (d = 0; d <= keep; d++) {
        if (find(w, meshed, pcx, pcz, d, true, &job->cx, &job->cz)) {
            job->kind = STREAM_LOAD;
            return true;
        }
        if (d >= 1 && find(w, meshed, pcx, pcz, d - 1, false, &job->cx, &job->cz)) {
            job->kind = STREAM_MESH;
            return true;
        }
    }
    job->kind = STREAM_NONE;
    return false;
}
