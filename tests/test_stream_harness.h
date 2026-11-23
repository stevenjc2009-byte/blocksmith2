#ifndef TEST_STREAM_HARNESS_H
#define TEST_STREAM_HARNESS_H

#include <stdbool.h>
#include <stdint.h>

#include "stream.h"
#include "world.h"

/* The stream tests' world, run the way meshes.c runs it, shared by
 * the tests in test_stream.c. */

extern World s_w;
extern bool s_meshed[WORLD_SLOTS][WORLD_SLOTS];
extern int s_loads, s_meshes, s_bad_mesh, s_runaway;
extern int s_view; /* the view distance every job and tidy-up is asked with */

/* Mesh jobs are told apart from loads in a job list by this flag. */
#define MESHED_JOB 1000000

/* The view distance stream.c should use for s_view, worked out here apart
 * from stream_view(): 2 to 5, anything outside counting as the nearer end. */
int view_used(void);

/* The ring chunk (cx, cz) is in round (pcx, pcz): 0 for that chunk itself. */
int ring(int cx, int cz, int pcx, int pcz);

/* One job, done the way meshes.c does it: a load forgets the slot's old mesh,
 * a mesh marks the slot meshed. Mesh jobs whose chunk or a neighbour is not
 * loaded are counted in s_bad_mesh. False when there is no job. */
bool run_job(int pcx, int pcz, StreamJob *job);

/* meshes_stream()'s start of a frame: put away far chunks, drop meshes that should go. */
void tidy(int pcx, int pcz);

/* Tidy, then run jobs until there are none; counts start from zero. */
void settle(int pcx, int pcz);

/* An empty world, view distance view. */
void fresh(uint32_t seed, int view);

/* Exactly the chunks out to ring view + 1 round (pcx, pcz) loaded (9 x 9
 * at view 3), exactly those out to ring view meshed (7 x 7); with ground,
 * every loaded chunk's blocks are its seed's ground. */
void check_settled(int pcx, int pcz, bool ground);

#endif
