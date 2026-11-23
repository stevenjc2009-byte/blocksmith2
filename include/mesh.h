#ifndef MESH_H
#define MESH_H

/* Building triangles for blocks. A block at (x, y, z) fills the unit box
 * from (x, y, z) to (x + 1, y + 1, z + 1). Every face is two triangles
 * (six vertices), counter-clockwise when seen from outside the block. */

typedef struct {
    float x, y, z; /* position */
    float u, v;    /* texture coordinates in the block texture picture */
    float r, g, b; /* brightness, the same in all three: faces are shaded by
                      the way they face, so a block's edges show */
} MeshVertex;

#define MESH_FACE_VERTS 6
#define MESH_BLOCK_VERTS (6 * MESH_FACE_VERTS)

/* Brightness of a face: top 1.0, east and west 0.8, north and south 0.6,
 * bottom 0.5. */
float mesh_face_shade(int face);

/* Write the six vertices of one face (FACE_* from block.h) of the block at
 * (x, y, z), showing texture tile `tile` (TILE_* from blocktex.h). Side
 * faces show the tile upright: the tile's top edge along the block's top. */
void mesh_face(MeshVertex out[MESH_FACE_VERTS], float x, float y, float z,
               int face, int tile);

/* Write all six faces of a block (MESH_BLOCK_VERTS vertices).
 * Returns the number of vertices written: 0 for air or a bad block. */
int mesh_block(MeshVertex *out, float x, float y, float z, int block);

#endif
