#ifndef MESHES_H
#define MESHES_H

#include <stdbool.h>

#include "mat4.h"
#include "world.h"

/* The world's chunk meshes, in linear memory for the GPU: one per world
 * slot, for the chunk in that slot (world.h). Built and freed between frames
 * only: gpu_frame_end() has waited for the GPU, so an old mesh is no longer
 * being read. 3DS calls, so not in the PC tests. */

#define MESHES_ALL (-1) /* meshes_stream(): every job left */

/* Keep world w round the player's chunk (pcx, pcz) out to view distance
 * view (stream.h): put away the far chunks and the meshes that should go,
 * then run up to jobs jobs (MESHES_ALL: until none are left). False if out
 * of memory for a mesh. */
bool meshes_stream(World *w, int pcx, int pcz, int view, int jobs);

/* Rebuild the built meshes a change to block column (x, z) touches (a chunk
 * still waiting for its mesh gets the change when it is built). False if
 * out of memory. */
bool meshes_rebuild_around(const World *w, int x, int z);

/* Every mesh, drawn with mvp (between gpu_frame_begin() and gpu_frame_end()). */
void meshes_draw(const Mat4 *mvp);

/* Free every mesh. */
void meshes_free(void);

#endif
