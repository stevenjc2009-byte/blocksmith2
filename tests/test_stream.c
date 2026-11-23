#include <math.h>
#include <string.h>

#include "block.h"
#include "stream.h"
#include "terrain.h"
#include "test.h"
#include "world.h"

static World s_w;
static bool s_meshed[WORLD_SLOTS][WORLD_SLOTS];
static int s_loads, s_meshes, s_bad_mesh, s_runaway;
static int s_view; /* the view distance every job and tidy-up is asked with */

/* Mesh jobs are told apart from loads in a job list by this flag. */
#define MESHED_JOB 1000000

/* The view distance stream.c should use for s_view, worked out here apart
 * from stream_view(): 2 to 5, anything outside counting as the nearer end. */
static int view_used(void)
{
    return s_view < 2 ? 2 : s_view > 5 ? 5 : s_view;
}

static int ring(int cx, int cz, int pcx, int pcz)
{
    int dx = cx > pcx ? cx - pcx : pcx - cx, dz = cz > pcz ? cz - pcz : pcz - cz;

    return dx > dz ? dx : dz;
}

/* One job, done the way main.c does it: a load forgets the slot's old mesh,
 * a mesh marks the slot meshed. Mesh jobs whose chunk or a neighbour is not
 * loaded are counted in s_bad_mesh. False when there is no job. */
static bool run_job(int pcx, int pcz, StreamJob *job)
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

/* main.c's start of a frame: put away far chunks, drop meshes that should go. */
static void tidy(int pcx, int pcz)
{
    int i, j;

    stream_put_away(&s_w, pcx, pcz, s_view);
    for (j = 0; j < WORLD_SLOTS; j++)
        for (i = 0; i < WORLD_SLOTS; i++)
            if (s_meshed[j][i] && !stream_keep_mesh(&s_w, i, j, pcx, pcz, s_view))
                s_meshed[j][i] = false;
}

/* Tidy, then run jobs until there are none; counts start from zero. */
static void settle(int pcx, int pcz)
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

/* An empty world, view distance view. */
static void fresh(uint32_t seed, int view)
{
    s_view = view;
    memset(&s_w, 0xA5, sizeof s_w);
    world_init(&s_w, seed);
    memset(s_meshed, 0, sizeof s_meshed);
    s_bad_mesh = s_runaway = s_loads = s_meshes = 0;
}

/* Exactly the chunks out to ring view + 1 round (pcx, pcz) loaded (9 x 9
 * at view 3), exactly those out to ring view meshed (7 x 7); with ground,
 * every loaded chunk's blocks are its seed's ground. */
static void check_settled(int pcx, int pcz, bool ground)
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

static void test_ring(void)
{
    CHECK_EQ_INT(stream_ring(0, 0, 0, 0), 0);
    CHECK_EQ_INT(stream_ring(3, -1, 0, 0), 3);
    CHECK_EQ_INT(stream_ring(-4, 2, 0, 0), 4);
    CHECK_EQ_INT(stream_ring(5, 5, 2, 1), 4);
    CHECK_EQ_INT(stream_ring(-1, -7, -2, -3), 4);
    CHECK_EQ_INT(stream_ring(-62500, 0, 62500, 0), 125000);
}

/* From nothing, round chunk (0, 0) at view distance view: the exact order
 * of the first jobs, and where each ring's first mesh comes. */
static void test_first_fill(int view)
{
    static const int k_first[10][3] = {
        { 0, 0, 0 },
        { 0, 0, -1 }, { 0, -1, 0 }, { 0, 1, 0 }, { 0, 0, 1 },
        { 0, -1, -1 }, { 0, 1, -1 }, { 0, -1, 1 }, { 0, 1, 1 },
        { MESHED_JOB, 0, 0 },
    };
    StreamJob job;
    int list[400][3];
    int n = 0, k, r, wrong = 0, load_ring = 0, mesh_ring = 0, back = 0;

    fresh(2026u, view);
    while (n < 400 && run_job(0, 0, &job)) {
        list[n][0] = job.kind == STREAM_MESH ? MESHED_JOB : 0;
        list[n][1] = job.cx;
        list[n][2] = job.cz;
        n++;
    }
    CHECK_EQ_INT(n, (2 * view + 3) * (2 * view + 3) + (2 * view + 1) * (2 * view + 1));
    CHECK_EQ_INT(s_loads, (2 * view + 3) * (2 * view + 3));
    CHECK_EQ_INT(s_meshes, (2 * view + 1) * (2 * view + 1));
    for (k = 0; k < 10; k++)
        wrong += memcmp(list[k], k_first[k], sizeof list[k]) != 0;
    CHECK_EQ_INT(wrong, 0);
    /* Loads and meshes each go outwards ring by ring, never back. */
    for (k = 0; k < n; k++) {
        int rk = ring(list[k][1], list[k][2], 0, 0);
        int *last = list[k][0] ? &mesh_ring : &load_ring;

        back += rk < *last;
        *last = rk;
    }
    CHECK_EQ_INT(back, 0);
    /* Ring r's first mesh comes after every load out to ring r + 1 and
     * every mesh inside ring r: job (2r + 3)^2 + (2r - 1)^2, counting from
     * 0 (just 9 for ring 0). */
    for (r = 0; r <= view; r++) {
        int first = -1;

        for (k = 0; k < n && first < 0; k++)
            if (list[k][0] && ring(list[k][1], list[k][2], 0, 0) == r)
                first = k;
        CHECK_EQ_INT(first, (2 * r + 3) * (2 * r + 3) + (r ? (2 * r - 1) * (2 * r - 1) : 0));
    }
    check_settled(0, 0, true);
    CHECK_EQ_INT(stream_next(&s_w, s_meshed, 0, 0, view, &job), 0);
    CHECK_EQ_INT(job.kind, STREAM_NONE);
}

/* One chunk east: one column of 9 loads, one column of 7 meshes, and the
 * far column's 7 meshes dropped. Then one chunk diagonally. */
static void test_steps(void)
{
    int z, gone = 0, kept = 0;

    fresh(2026u, 3);
    settle(0, 0);
    settle(1, 0);
    CHECK_EQ_INT(s_loads, 9);
    CHECK_EQ_INT(s_meshes, 7);
    for (z = -4; z <= 4; z++) {
        gone += world_chunk(&s_w, -4, z) == NULL;
        kept += world_chunk(&s_w, -3, z) != NULL;
    }
    CHECK_EQ_INT(gone, 9);
    CHECK_EQ_INT(kept, 9);
    CHECK_EQ_INT(s_meshed[world_slot(0)][world_slot(-3)], 0);
    CHECK_EQ_INT(s_meshed[world_slot(0)][world_slot(-2)], 1);
    CHECK_EQ_INT(s_meshed[world_slot(0)][world_slot(4)], 1);
    check_settled(1, 0, true);
    settle(2, 1);
    CHECK_EQ_INT(s_loads, 17);
    CHECK_EQ_INT(s_meshes, 13);
    check_settled(2, 1, true);
    settle(2, 1);
    CHECK_EQ_INT(s_loads + s_meshes, 0);
}

/* A long walk east, north and back, a jump far away and home again: after
 * each move everything round the player is loaded and meshed, nothing else.
 * steps moves of each kind (60 at view 3). */
static void test_long_walk(int view, int steps)
{
    int k, x = 0, z = 0, all = (2 * view + 3) * (2 * view + 3);

    fresh(0xFFFFFFFFu, view);
    settle(0, 0);
    for (k = 1; k <= steps; k++) {
        settle(++x, z);
        check_settled(x, z, k % 10 == 0);
    }
    for (k = 1; k <= steps; k++) {
        settle(x, --z);
        check_settled(x, z, k % 10 == 0);
    }
    for (k = 1; k <= steps / 2; k++) {
        x -= 2;
        z += 2;
        settle(x, z);
        check_settled(x, z, k % 10 == 0);
    }
    settle(62500, -62500);
    CHECK_EQ_INT(s_loads, all);
    CHECK_EQ_INT(s_meshes, (2 * view + 1) * (2 * view + 1));
    check_settled(62500, -62500, true);
    settle(0, 0);
    CHECK_EQ_INT(s_loads, all);
    check_settled(0, 0, true);
}

/* Like the game: one job a frame, 30 frames a second, the player walking at
 * full speed with view distance view. The player's own chunk and the ring
 * round it are always loaded (nothing to walk into but ground), and
 * everything out to ring view - 1 always has its mesh. */
static void walk_frames(int view, float dir_x, float dir_z, int frames)
{
    StreamJob job;
    float x = 0.5f, z = 0.5f;
    int f, i, j, unloaded = 0, unmeshed = 0, idle = 0;

    fresh(2026u, view);
    settle(0, 0);
    for (f = 0; f < frames; f++) {
        int pcx = block_to_chunk((int)floorf(x)), pcz = block_to_chunk((int)floorf(z));

        tidy(pcx, pcz);
        idle += !run_job(pcx, pcz, &job);
        for (j = -(view - 1); j <= view - 1; j++)
            for (i = -(view - 1); i <= view - 1; i++) {
                int cx = pcx + i, cz = pcz + j;

                if (world_chunk(&s_w, cx, cz) == NULL)
                    unloaded++;
                else if (!s_meshed[world_slot(cz)][world_slot(cx)])
                    unmeshed++;
            }
        x += dir_x * 4.3f / 30.0f;
        z += dir_z * 4.3f / 30.0f;
    }
    CHECK_EQ_INT(unloaded, 0);
    CHECK_EQ_INT(unmeshed, 0);
    CHECK_EQ_INT(s_bad_mesh, 0);
    CHECK_EQ_INT(idle > frames / 2, 1); /* the jobs keep up with time to spare */
}

static void test_keeps_up(int view)
{
    walk_frames(view, 0.0f, -1.0f, 3000);
    walk_frames(view, 0.70710678f, 0.70710678f, 3000);
    walk_frames(view, -0.70710678f, -0.70710678f, 3000);
}

/* A change stays while its chunk is loaded and is lost once the chunk is
 * put away (until saving, roadmap row 2-04). */
static void test_edits(int view)
{
    fresh(2026u, view);
    settle(0, 0);
    world_set(&s_w, 40, 100, 5, BLOCK_STONE); /* chunk (2, 0) */
    CHECK_EQ_INT(world_get(&s_w, 40, 100, 5), BLOCK_STONE);
    settle(3, 0);
    settle(2 + view + 1, 0); /* chunk 2 is ring view + 1: still kept */
    CHECK_EQ_INT(world_get(&s_w, 40, 100, 5), BLOCK_STONE);
    settle(2 + view + 2, 0); /* one ring further: put away */
    CHECK_EQ_INT(world_chunk(&s_w, 2, 0) == NULL, 1);
    settle(0, 0);
    CHECK_EQ_INT(world_get(&s_w, 40, 100, 5), BLOCK_AIR);
}

static void test_put_away_and_keep(void)
{
    int z, loaded = 0, i, j;

    fresh(2026u, 3);
    settle(0, 0);
    stream_put_away(&s_w, 0, 0, 3);
    stream_put_away(&s_w, 1, 0, 3);
    for (j = 0; j < WORLD_SLOTS; j++)
        for (i = 0; i < WORLD_SLOTS; i++)
            loaded += s_w.slots[j][i].loaded;
    CHECK_EQ_INT(loaded, 72);
    for (z = -4; z <= 4; z++) {
        CHECK_EQ_INT(world_chunk(&s_w, -4, z) == NULL, 1);
        CHECK_EQ_INT(world_chunk(&s_w, -3, z) != NULL, 1);
        CHECK_EQ_INT(world_chunk(&s_w, 4, z) != NULL, 1);
    }
    /* Chunk (3, 0)'s mesh: kept with the player at (0, 0) or (1, 0), not at
     * (-1, 0); an empty slot keeps no mesh. */
    CHECK_EQ_INT(stream_keep_mesh(&s_w, world_slot(3), world_slot(0), 0, 0, 3), 1);
    CHECK_EQ_INT(stream_keep_mesh(&s_w, world_slot(3), world_slot(0), 1, 0, 3), 1);
    CHECK_EQ_INT(stream_keep_mesh(&s_w, world_slot(3), world_slot(0), -1, 0, 3), 0);
    CHECK_EQ_INT(stream_keep_mesh(&s_w, world_slot(-4), world_slot(0), -3, 0, 3), 0);
    CHECK_EQ_INT(stream_keep_mesh(&s_w, world_slot(-3), world_slot(-3), 0, 0, 3), 1);
    CHECK_EQ_INT(stream_keep_mesh(&s_w, world_slot(-3), world_slot(-4), 0, 0, 3), 0);
}

/* The view distance itself: 2 to 5, 3 at start, anything outside counts as
 * the nearer end. */
static void test_view_range(void)
{
    CHECK_EQ_INT(STREAM_VIEW_MIN, 2);
    CHECK_EQ_INT(STREAM_VIEW_MAX, 5);
    CHECK_EQ_INT(STREAM_VIEW_DEFAULT, 3);
    CHECK_EQ_INT(stream_view(2), 2);
    CHECK_EQ_INT(stream_view(3), 3);
    CHECK_EQ_INT(stream_view(4), 4);
    CHECK_EQ_INT(stream_view(5), 5);
    CHECK_EQ_INT(stream_view(1), 2);
    CHECK_EQ_INT(stream_view(6), 5);
    CHECK_EQ_INT(stream_view(-2147483647 - 1), 2);
    CHECK_EQ_INT(stream_view(2147483647), 5);
}

/* Changing the view distance with the player standing still: a longer one
 * loads and meshes only the new rings, a shorter one puts the outer rings
 * and their meshes away and makes nothing, and going back out makes them
 * again. */
static void test_view_changes(void)
{
    fresh(2026u, 3);
    settle(0, 0);
    check_settled(0, 0, true);
    s_view = 5;
    settle(0, 0);
    CHECK_EQ_INT(s_loads, 13 * 13 - 9 * 9);
    CHECK_EQ_INT(s_meshes, 11 * 11 - 7 * 7);
    check_settled(0, 0, true);
    s_view = 2;
    settle(0, 0);
    CHECK_EQ_INT(s_loads + s_meshes, 0);
    check_settled(0, 0, true);
    CHECK_EQ_INT(world_chunk(&s_w, 4, 0) == NULL, 1);
    CHECK_EQ_INT(world_chunk(&s_w, 3, -3) != NULL, 1);
    CHECK_EQ_INT(s_meshed[world_slot(0)][world_slot(3)], 0);
    CHECK_EQ_INT(s_meshed[world_slot(2)][world_slot(-2)], 1);
    s_view = 3;
    settle(0, 0);
    CHECK_EQ_INT(s_loads, 9 * 9 - 7 * 7);
    CHECK_EQ_INT(s_meshes, 7 * 7 - 5 * 5);
    check_settled(0, 0, true);

    /* Outside 2..5: the nearer end. */
    fresh(2026u, 9);
    settle(0, 0);
    CHECK_EQ_INT(s_loads, 13 * 13);
    CHECK_EQ_INT(s_meshes, 11 * 11);
    check_settled(0, 0, false);
    s_view = 0;
    settle(0, 0);
    CHECK_EQ_INT(s_loads + s_meshes, 0);
    check_settled(0, 0, false);
    s_view = -2147483647 - 1;
    settle(5, 5);
    check_settled(5, 5, false);
}

void test_stream(void)
{
    int v;

    test_ring();
    test_view_range();
    for (v = STREAM_VIEW_MIN; v <= STREAM_VIEW_MAX; v++)
        test_first_fill(v);
    test_steps();
    test_long_walk(3, 60);
    test_long_walk(5, 30);
    test_long_walk(2, 30);
    for (v = STREAM_VIEW_MIN; v <= STREAM_VIEW_MAX; v++) {
        test_keeps_up(v);
        test_edits(v);
    }
    test_view_changes();
    test_put_away_and_keep();
}
