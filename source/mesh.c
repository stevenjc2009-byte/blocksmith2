#include <stdlib.h>

#include "block.h"
#include "blocktex.h"
#include "mesh.h"

/* The four corners of each face, offsets from the block's low corner, in
 * the order bottom-left, bottom-right, top-right, top-left as seen from
 * outside the block. For side faces "up" is +y; the top face is seen from
 * above with -z up, the bottom face from below with +z up. */
static const signed char k_corners[FACE_COUNT][4][3] = {
    { { 1, 0, 1 }, { 1, 0, 0 }, { 1, 1, 0 }, { 1, 1, 1 } }, /* +x */
    { { 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 1 }, { 0, 1, 0 } }, /* -x */
    { { 0, 1, 1 }, { 1, 1, 1 }, { 1, 1, 0 }, { 0, 1, 0 } }, /* +y, top */
    { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 1 }, { 0, 0, 1 } }, /* -y, bottom */
    { { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 } }, /* +z */
    { { 1, 0, 0 }, { 0, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 } }, /* -z */
};

static const float k_shade[FACE_COUNT] = { 0.8f, 0.8f, 1.0f, 0.5f, 0.6f, 0.6f };

/* Corner shadow levels 0 to MESH_AO_OPEN: 0.6 up to 1.0 in even steps. */
static const float k_ao[MESH_AO_OPEN + 1] = { 0.6f, 0.6f + 0.4f / 3.0f, 0.6f + 0.8f / 3.0f, 1.0f };

/* Two triangles, both counter-clockwise: corners 0 1 2 and 0 2 3 (cut
 * along 0-2), or 1 2 3 and 1 3 0 (cut along 1-3). */
static const unsigned char k_order[2][MESH_FACE_VERTS] = { { 0, 1, 2, 0, 2, 3 }, { 1, 2, 3, 1, 3, 0 } };

static const unsigned char k_open[4] = { MESH_AO_OPEN, MESH_AO_OPEN, MESH_AO_OPEN, MESH_AO_OPEN };

float mesh_face_shade(int face)
{
    return (face >= 0 && face < FACE_COUNT) ? k_shade[face] : 0.0f;
}

float mesh_ao_scale(int level)
{
    return (level >= 0 && level <= MESH_AO_OPEN) ? k_ao[level] : 1.0f;
}

const signed char *mesh_face_corner(int face, int corner)
{
    if (face < 0 || face >= FACE_COUNT || corner < 0 || corner > 3)
        return k_corners[0][0];
    return k_corners[face][corner];
}

void mesh_face_ao(MeshVertex out[MESH_FACE_VERTS], float x, float y, float z,
                  int face, int tile, const unsigned char ao[4])
{
    float uv[4], shade = k_shade[face];
    const unsigned char *order = k_order[abs(ao[0] - ao[2]) > abs(ao[1] - ao[3])];
    int i;

    blocktex_tile_uv(tile, uv);
    for (i = 0; i < MESH_FACE_VERTS; i++) {
        int c = order[i];
        const signed char *p = k_corners[face][c];
        MeshVertex *v = &out[i];

        v->x = x + p[0];
        v->y = y + p[1];
        v->z = z + p[2];
        v->u = (c == 1 || c == 2) ? uv[2] : uv[0]; /* right corners get the right edge */
        v->v = (c >= 2) ? uv[3] : uv[1];           /* top corners get the top edge */
        v->r = v->g = v->b = shade * mesh_ao_scale(ao[c]);
    }
}

void mesh_face(MeshVertex out[MESH_FACE_VERTS], float x, float y, float z,
               int face, int tile)
{
    mesh_face_ao(out, x, y, z, face, tile, k_open);
}

int mesh_block(MeshVertex *out, float x, float y, float z, int block)
{
    int face;

    if (block_face_tile(block, 0) < 0)
        return 0;
    for (face = 0; face < FACE_COUNT; face++)
        mesh_face(out + face * MESH_FACE_VERTS, x, y, z, face,
                  block_face_tile(block, face));
    return MESH_BLOCK_VERTS;
}
