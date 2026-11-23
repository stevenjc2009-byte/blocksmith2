#ifndef LIGHT_H
#define LIGHT_H

#include <stdbool.h>

#include "chunk.h"
#include "chunk_mesh.h"
#include "world.h"

/* Torch light (roadmap row 3-03, kickoff W-8). Pure C, so the PC tests can
 * run it.
 *
 * The rule: a torch's own cell has level LIGHT_TORCH (14). Light goes from
 * a cell to each of its six neighbours that is not solid (block_solid():
 * air and torches let it through), one level less per step, so a cell
 * LIGHT_REACH (13) steps away along the shortest such path gets 1 and
 * farther ones nothing from that torch. A cell's level is the highest any
 * torch gives it. Light never leaves the world (y below 0 or past
 * WORLD_HEIGHT - 1) and, for a chunk's mesh, never leaves that chunk and its
 * eight neighbours (no torch outside them is near enough to matter: see
 * light.c). A missing neighbour chunk is all air.
 *
 * The level becomes a brightness (light_brightness()) that the shader uses
 * as the vertex's lowest light, tinted warm, so by day (light 1) torches
 * change nothing and at night (0.35) they light a pool round them. */

#define LIGHT_TORCH 14
#define LIGHT_REACH (LIGHT_TORCH - 1)

/* Work out the light levels of the cells chunk c's mesh looks at: local x
 * and z from -1 to CHUNK_SIZE, y from ylo to yhi (clamped to the world),
 * from every torch in c and its eight neighbours (nb in NB_* order; NULL or
 * a NULL entry: no chunk). Returns false, and works nothing out, when no
 * torch is near enough to light any of those cells: then they all have
 * level 0. */
bool light_build(const Chunk *c, const Chunk *const nb[NB_COUNT], int ylo, int yhi);

/* The level (0 to LIGHT_TORCH) of local cell (x, y, z) from the last
 * light_build() that returned true, for a cell in the range it was asked
 * for; 0 for any other cell, or when the last build returned false. */
int light_level(int x, int y, int z);

/* A level as a brightness: level / LIGHT_TORCH, so 0 for level 0 (or
 * less) and 1 for LIGHT_TORCH (or more). */
float light_brightness(int level);

/* The chunks whose meshes' light can change when the block at world (x, y,
 * z) has just changed (its own and neighbouring chunks' faces and corner
 * shadows are world_touched_chunks()'s job): within LIGHT_REACH steps of a
 * torch placed or broken there (torch_edited), and within LIGHT_REACH of
 * every torch now within LIGHT_REACH steps of it (a solid block placed or
 * broken there blocks or opens their light), plus the one-cell ring a mesh
 * samples. Writes the smallest box of chunks holding all of them to box
 * (cx0, cz0, cx1, cz1, inclusive) and returns true, or returns false when
 * no light can have changed. Some of those chunks may not be loaded. */
bool light_touched_box(const World *w, int x, int y, int z, bool torch_edited, int box[4]);

#endif
