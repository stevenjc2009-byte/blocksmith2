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
 * - past the chunk's side (or corner) it is the neighbour chunk's block, or
 *   air when that neighbour is NULL (or the whole list is NULL).
 *
 * Corner shadows (roadmap row 3-01): each corner of a seen face gets a
 * shadow level (mesh.h mesh_face_ao()) from the cells round it in the layer
 * just outside the face (the layer the face looks into). In that layer the
 * corner touches four cells: the one in front of the face (air, or the face
 * would not be seen), two that share an edge with it (the sides) and one
 * across the corner. Both sides solid: level 0; else MESH_AO_OPEN less one
 * for each of the three that is solid. Cells are judged solid by the same
 * rules as above, so a corner on a chunk's corner looks into the chunk
 * across that corner too.
 *
 * Neighbour list order: */
enum {
    NB_POS_X, /* the chunk to the east */
    NB_NEG_X, /* west */
    NB_POS_Z, /* south */
    NB_NEG_Z, /* north */
    NB_SIDES, /* the four across a side come first */
    NB_POS_X_POS_Z = NB_SIDES, /* south-east: the four across a corner, for corner shadows */
    NB_NEG_X_POS_Z,            /* south-west */
    NB_POS_X_NEG_Z,            /* north-east */
    NB_NEG_X_NEG_Z,            /* north-west */
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
