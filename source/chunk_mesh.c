#include <stddef.h>

#include "block.h"
#include "chunk_mesh.h"

/* One step out of each face (FACE_* order). */
static const signed char k_step[FACE_COUNT][3] = {
    { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 },
};

/* Is the cell at local (x, y, z) solid? (x, y, z) is at most one step
 * outside the chunk. */
static bool solid_at(const Chunk *c, const Chunk *const nb[NB_COUNT], int x, int y, int z)
{
    const Chunk *o = c;

    if (y < 0)
        return true;
    if (y >= WORLD_HEIGHT)
        return false;
    if (x >= CHUNK_SIZE) {
        o = nb ? nb[NB_POS_X] : NULL;
        x -= CHUNK_SIZE;
    } else if (x < 0) {
        o = nb ? nb[NB_NEG_X] : NULL;
        x += CHUNK_SIZE;
    } else if (z >= CHUNK_SIZE) {
        o = nb ? nb[NB_POS_Z] : NULL;
        z -= CHUNK_SIZE;
    } else if (z < 0) {
        o = nb ? nb[NB_NEG_Z] : NULL;
        z += CHUNK_SIZE;
    }
    return o != NULL && o->blocks[y][z][x] != BLOCK_AIR;
}

bool chunk_face_visible(const Chunk *c, const Chunk *const nb[NB_COUNT],
                        int x, int y, int z, int face)
{
    if (face < 0 || face >= FACE_COUNT)
        return false;
    return !solid_at(c, nb, x + k_step[face][0], y + k_step[face][1], z + k_step[face][2]);
}

int chunk_mesh_count(const Chunk *c, const Chunk *const nb[NB_COUNT])
{
    int x, y, z, f, n = 0;

    for (y = 0; y < WORLD_HEIGHT; y++)
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++) {
                if (c->blocks[y][z][x] == BLOCK_AIR)
                    continue;
                for (f = 0; f < FACE_COUNT; f++)
                    n += chunk_face_visible(c, nb, x, y, z, f);
            }
    return n;
}

int chunk_mesh_build(const Chunk *c, const Chunk *const nb[NB_COUNT], float ox, float oz,
                     MeshVertex *out, int max_verts)
{
    int x, y, z, f, n = 0;

    for (y = 0; y < WORLD_HEIGHT; y++)
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++) {
                int b = c->blocks[y][z][x];

                if (b == BLOCK_AIR)
                    continue;
                for (f = 0; f < FACE_COUNT; f++) {
                    if (!chunk_face_visible(c, nb, x, y, z, f))
                        continue;
                    if (n + MESH_FACE_VERTS > max_verts)
                        return -1;
                    mesh_face(out + n, ox + (float)x, (float)y, oz + (float)z, f,
                              block_face_tile(b, f));
                    n += MESH_FACE_VERTS;
                }
            }
    return n;
}
