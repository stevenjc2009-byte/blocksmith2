#ifndef WATER_MESH_H
#define WATER_MESH_H

#include "chunk.h"
#include "chunk_mesh.h"
#include "mesh.h"

/* The water's triangles for a whole chunk (roadmap row 4-01), apart from
 * the solid mesh (chunk_mesh.h leaves water out) so they can be drawn after
 * it, blended. Pure C, so the PC tests can run it.
 *
 * The cell next to a face of a water cell is water, solid (block_solid(),
 * or below the world) or open (air, a torch, above the world, or in a
 * missing neighbour chunk; neighbours as chunk_mesh.h, only the four
 * across a side are looked at). A water cell gets:
 * - a side or bottom face where that cell is open (none against water,
 *   none against a solid block);
 * - a top face where the cell above is not water (open or solid).
 *
 * Height: a water cell with water above fills its cell; any other is
 * WATER_TOP high (water.h): its top face at y + WATER_TOP, its sides from y
 * to y + WATER_TOP showing the tile's bottom WATER_TOP_TEXELS rows, so the
 * ripples are not squeezed. Full-height sides, tops and bottoms show the
 * whole of TILE_WATER.
 *
 * Light: every corner gets its face's brightness (mesh_face_shade(), no
 * corner shadows) and torch light t = light_brightness() of the water
 * cell's own level (light.h); day and night come from the shader, as for
 * every mesh.
 *
 * Corners go in mesh_face_corner() order, cut along corners 0 and 2
 * (triangles 0 1 2 and 0 2 3), counter-clockwise from outside like a
 * block's. */

/* How many faces water_mesh_build() will write. */
int water_mesh_count(const Chunk *c, const Chunk *const nb[NB_COUNT]);

/* Write the water's faces (MESH_FACE_VERTS vertices each), going up through
 * y, then z, then x, faces in FACE_* order. A cell at local (x, y, z) is
 * drawn at world (ox + x, y, oz + z). Returns the number of vertices
 * written, or -1 if they would not fit in max_verts (nothing past
 * max_verts is touched). */
int water_mesh_build(const Chunk *c, const Chunk *const nb[NB_COUNT], float ox, float oz,
                     MeshVertex *out, int max_verts);

#endif
