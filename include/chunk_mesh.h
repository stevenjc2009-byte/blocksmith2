#ifndef CHUNK_MESH_H
#define CHUNK_MESH_H

#include <stdbool.h>

#include "chunk.h"
#include "mesh.h"

/* Triangles for a whole chunk, skipping every face nobody can see: a face
 * is drawn only when the cell next to it is air.
 *
 * The cell next to a face:
 * - below the world (y < 0) counts as solid, so the bottom layer has no
 *   bottom faces;
 * - above the world (y >= WORLD_HEIGHT) counts as air;
 * - past the chunk's side it is the neighbour chunk's block, or air when
 *   that neighbour is NULL (or the whole list is NULL).
 *
 * Neighbour list order: */
enum {
    NB_POS_X, /* the chunk to the east */
    NB_NEG_X, /* west */
    NB_POS_Z, /* south */
    NB_NEG_Z, /* north */
    NB_COUNT
};

/* Is this face (FACE_*) of the block at local (x, y, z) seen? */
bool chunk_face_visible(const Chunk *c, const Chunk *const nb[NB_COUNT],
                        int x, int y, int z, int face);

/* How many faces chunk_mesh_build() will write. */
int chunk_mesh_count(const Chunk *c, const Chunk *const nb[NB_COUNT]);

/* Write the seen faces (MESH_FACE_VERTS vertices each), going up through y,
 * then z, then x, faces in FACE_* order. A block at local (x, y, z) is drawn
 * at world (ox + x, y, oz + z). Returns the number of vertices written, or
 * -1 if they would not fit in max_verts (nothing past max_verts is
 * touched). */
int chunk_mesh_build(const Chunk *c, const Chunk *const nb[NB_COUNT], float ox, float oz,
                     MeshVertex *out, int max_verts);

#endif
