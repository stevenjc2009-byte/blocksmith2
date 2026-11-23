#ifndef MESH_H
#define MESH_H

/* Building triangles for blocks. A block at (x, y, z) fills the unit box
 * from (x, y, z) to (x + 1, y + 1, z + 1). Every face is two triangles
 * (six vertices), counter-clockwise when seen from outside the block. */

typedef struct {
    float x, y, z; /* position */
    float u, v;    /* texture coordinates in the block texture picture */
    float r, g, b; /* brightness, the same in all three: faces are shaded by
                      the way they face, so a block's edges show, and each
                      corner is darkened by the blocks round it (row 3-01) */
} MeshVertex;

#define MESH_FACE_VERTS 6
#define MESH_BLOCK_VERTS (6 * MESH_FACE_VERTS)

/* Soft corner shadows (roadmap row 3-01, ambient occlusion): each corner of
 * a face gets a level from 0 (darkest) to MESH_AO_OPEN (nothing round it),
 * from the three cells that touch that corner in the layer just outside the
 * face (chunk_mesh.h says which). */
#define MESH_AO_OPEN 3

/* Brightness of a face: top 1.0, east and west 0.8, north and south 0.6,
 * bottom 0.5. */
float mesh_face_shade(int face);

/* How much a corner at shadow level `level` keeps of its face's brightness:
 * 0.6 at level 0 up to 1.0 at MESH_AO_OPEN, in even steps (a slight
 * darkening); 1.0 for a level out of range. */
float mesh_ao_scale(int level);

/* Corner `corner` (0 to 3) of face `face` (FACE_* from block.h): its offset
 * (each 0 or 1) from the block's low corner. The corners go bottom-left,
 * bottom-right, top-right, top-left as seen from outside the block; for side
 * faces "up" is +y, the top face is seen from above with -z up, the bottom
 * face from below with +z up. */
const signed char *mesh_face_corner(int face, int corner);

/* Write the six vertices of one face (FACE_* from block.h) of the block at
 * (x, y, z), showing texture tile `tile` (TILE_* from blocktex.h). Side
 * faces show the tile upright: the tile's top edge along the block's top.
 * Every corner has the face's brightness. */
void mesh_face(MeshVertex out[MESH_FACE_VERTS], float x, float y, float z,
               int face, int tile);

/* The same with corner shadows: corner c (mesh_face_corner() order) gets
 * the face's brightness times mesh_ao_scale(ao[c]). The square is cut into
 * its two triangles along the diagonal whose two corners have the closer
 * levels (corners 0 and 2 unless |ao[0] - ao[2]| > |ao[1] - ao[3]|, then 1
 * and 3), so a corner that differs from the rest shades only its own
 * triangle and the shadow looks the same whichever way a face is turned. */
void mesh_face_ao(MeshVertex out[MESH_FACE_VERTS], float x, float y, float z,
                  int face, int tile, const unsigned char ao[4]);

/* Write all six faces of a block (MESH_BLOCK_VERTS vertices).
 * Returns the number of vertices written: 0 for air or a bad block. */
int mesh_block(MeshVertex *out, float x, float y, float z, int block);

#endif
