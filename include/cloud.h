#ifndef CLOUD_H
#define CLOUD_H

#include <stdbool.h>
#include <stdint.h>

#include "mesh.h"

/* Clouds (roadmap row 3-04): a layer of flat white slabs high over the
 * ground, drifting east. Plain C, no 3DS calls, so the PC tests run it;
 * skydraw.c puts them on the screen.
 *
 * The sky is cut into CLOUD_CELL x CLOUD_CELL block squares (cells). Cell
 * (i, j) covers x from i * CLOUD_CELL and z from j * CLOUD_CELL, before the
 * drift. Whether a cell is cloud or clear is a fixed pattern, the same in
 * every world, that repeats every CLOUD_PERIOD cells east-west and
 * north-south. A cloud cell is a slab from CLOUD_Y up CLOUD_THICK: its
 * bottom and top always drawn, a side only where the next cell is clear, so
 * touching cells make one big cloud.
 *
 * The whole pattern drifts east (+x) with the time of day: one whole period
 * (CLOUD_PERIOD * CLOUD_CELL blocks) a day, so when the day wraps from just
 * under 1 back to 0 the clouds are back where they started and nothing
 * jumps. Only the cells within CLOUD_RADIUS of the eye, across the ground,
 * are drawn; skydraw.c fogs them out into the sky's colour by then. */

#define CLOUD_CELL 12          /* blocks across a cell */
#define CLOUD_PERIOD 48        /* cells before the pattern repeats */
#define CLOUD_Y 100.0f         /* the bottom of the clouds (the ground is 60 to 71) */
#define CLOUD_THICK 4.0f
#define CLOUD_RADIUS 96.0f     /* drawn out to this far across the ground */
#define CLOUD_FOG_START 40.0f  /* fogged from here out to CLOUD_RADIUS */

/* The most vertices cloud_build() ever writes, wherever the eye is (the PC
 * tests try every cell of the pattern). */
#define CLOUD_VERTS_MAX 3072

/* Cells across the most cloud_cells() looks at each way: the eye's square,
 * CLOUD_RADIUS either side, spans 2 * CLOUD_RADIUS / CLOUD_CELL + 1 cells,
 * and one more for rounding. */
#define CLOUD_SPAN (2 * (int)CLOUD_RADIUS / CLOUD_CELL + 2)

/* The cells cloud_build() draws (fix batch, problem 9: skydraw.c builds the
 * clouds again only when these change): bit (j - j0) * CLOUD_SPAN +
 * (i - i0) is cell (i, j). Every other bit is 0, so two can be compared
 * whole (memcmp()). */
typedef struct {
    int i0, j0;
    uint32_t bits[(CLOUD_SPAN * CLOUD_SPAN + 31) / 32];
} CloudCells;

/* Is cell (i, j) cloud? Any i and j; the pattern repeats every
 * CLOUD_PERIOD cells. */
bool cloud_at(int i, int j);

/* How far east (+x, in blocks) the pattern has drifted at time of day t
 * (0 .. 1, sky.h): 0 at t = 0, one whole period at t = 1. */
float cloud_shift(float t);

/* Which cells cloud_build() draws round the eye at (px, ez) in the pattern,
 * before the drift (px = the eye's x - cloud_shift(t)): the cloud cells any
 * part of which is within CLOUD_RADIUS. */
void cloud_cells(float px, float ez, CloudCells *out);

/* The clouds round the eye at (ex, ez) across the ground, at time of day t:
 * every face of every cloud cell any part of which is within CLOUD_RADIUS,
 * as MESH_FACE_VERTS vertices each, counter-clockwise seen from outside,
 * texture tile TILE_CLOUD, brightness top 1.0, sides 0.9, bottom 0.8, no
 * torch light. Returns how many vertices were written (never more than
 * CLOUD_VERTS_MAX). */
int cloud_build(float ex, float ez, float t, MeshVertex out[CLOUD_VERTS_MAX]);

#endif
