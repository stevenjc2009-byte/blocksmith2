#include <stddef.h>
#include <string.h>

#include "block.h"
#include "chunk_mesh.h"
#include "light.h"

/* One step out of each face (FACE_* order). */
static const signed char k_step[FACE_COUNT][3] = {
    { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 },
};

/* The neighbour (NB_*) one chunk over by [dz + 1][dx + 1]; the middle is
 * the chunk itself (never looked up). */
static const signed char k_nb_at[3][3] = {
    { NB_NEG_X_NEG_Z, NB_NEG_Z, NB_POS_X_NEG_Z },
    { NB_NEG_X, -1, NB_POS_X },
    { NB_NEG_X_POS_Z, NB_POS_Z, NB_POS_X_POS_Z },
};

/* Is the cell at local (x, y, z) solid (block_solid())? x and z are each at
 * most one step outside the chunk. */
static bool solid_at(const Chunk *c, const Chunk *const nb[NB_COUNT], int x, int y, int z)
{
    const Chunk *o = c;
    int dx = x < 0 ? -1 : x >= CHUNK_SIZE ? 1 : 0;
    int dz = z < 0 ? -1 : z >= CHUNK_SIZE ? 1 : 0;

    if (y < 0)
        return true;
    if (y >= WORLD_HEIGHT)
        return false;
    if (dx != 0 || dz != 0) {
        o = nb ? nb[k_nb_at[dz + 1][dx + 1]] : NULL;
        x -= dx * CHUNK_SIZE;
        z -= dz * CHUNK_SIZE;
    }
    return o != NULL && block_solid(o->blocks[y][z][x]);
}

/* The two axes (0 x, 1 y, 2 z) across each face, FACE_* order. */
static const unsigned char k_across[FACE_COUNT][2] = {
    { 1, 2 }, { 1, 2 }, { 0, 2 }, { 0, 2 }, { 0, 1 }, { 0, 1 },
};

/* The corner shadow levels and torch light (chunk_mesh.h) of face f of the
 * block at local (x, y, z), corners in mesh_face_corner() order; light only
 * when lit (light_build() found a torch), else every corner gets 0. */
static void face_ao(const Chunk *c, const Chunk *const nb[NB_COUNT], int x, int y, int z, int f,
                    bool lit, unsigned char ao[4], float t[4])
{
    int out[3] = { x + k_step[f][0], y + k_step[f][1], z + k_step[f][2] };
    int a = k_across[f][0], b = k_across[f][1], i, j, k;
    bool solid[3][3]; /* the layer outside the face, [b step + 1][a step + 1] */
    float br[3][3];   /* its cells' torch brightness, when lit */

    for (j = 0; j < 3; j++)
        for (i = 0; i < 3; i++) {
            int p[3] = { out[0], out[1], out[2] };

            p[a] += i - 1;
            p[b] += j - 1;
            solid[j][i] = (i != 1 || j != 1) && solid_at(c, nb, p[0], p[1], p[2]);
            br[j][i] = lit ? light_brightness(light_level(p[0], p[1], p[2])) : 0.0f;
        }
    for (k = 0; k < 4; k++) {
        const signed char *q = mesh_face_corner(f, k);
        int da = q[a] ? 2 : 0, db = q[b] ? 2 : 0;
        int s1 = solid[1][da], s2 = solid[db][1], cr = solid[db][da];

        float sum = br[1][1];
        int n = 1;

        ao[k] = (unsigned char)((s1 && s2) ? 0 : MESH_AO_OPEN - s1 - s2 - cr);
        if (!s1) {
            sum += br[1][da];
            n++;
        }
        if (!s2) {
            sum += br[db][1];
            n++;
        }
        if (!(s1 && s2) && !cr) {
            sum += br[db][da];
            n++;
        }
        t[k] = sum / (float)n;
    }
}

bool chunk_face_visible(const Chunk *c, const Chunk *const nb[NB_COUNT],
                        int x, int y, int z, int face)
{
    if (face < 0 || face >= FACE_COUNT)
        return false;
    return !solid_at(c, nb, x + k_step[face][0], y + k_step[face][1], z + k_step[face][2]);
}

_Static_assert(BLOCK_AIR == 0, "solid_top() compares layers against zero bytes");
_Static_assert(BLOCK_COUNT == 5, "first_air_layer() knows air and the torch are the blocks that are not solid");

/* The first layer, from the bottom, with anything not solid in it (air or a
 * torch; WORLD_HEIGHT if none; 0 for a missing chunk, which is all air). */
static int first_air_layer(const Chunk *c)
{
    int y;

    if (c == NULL)
        return 0;
    for (y = 0; y < WORLD_HEIGHT; y++)
        if (memchr(c->blocks[y], BLOCK_AIR, sizeof c->blocks[y]) != NULL ||
            memchr(c->blocks[y], BLOCK_TORCH, sizeof c->blocks[y]) != NULL)
            break;
    return y;
}

/* One above the highest layer with any block in it (0 if none). */
static int solid_top(const Chunk *c)
{
    static const uint8_t k_air[CHUNK_SIZE][CHUNK_SIZE];
    int y;

    for (y = WORLD_HEIGHT; y > 0; y--)
        if (memcmp(c->blocks[y - 1], k_air, sizeof k_air) != 0)
            break;
    return y;
}

/* The layers [*lo, *hi) that can hold a seen face, so a mesh need not walk
 * all WORLD_HEIGHT of them (row 2-01: one mesh a frame must fit in a
 * 30-a-second frame). Above *hi there are no blocks. Below *lo every block
 * is buried: the layers under it, at it and over it in this chunk are full,
 * and so is its layer in all four neighbours (a missing neighbour is air,
 * which stops the skipping). */
static void face_layers(const Chunk *c, const Chunk *const nb[NB_COUNT], int *lo, int *hi)
{
    int low = first_air_layer(c) - 1, i;

    for (i = 0; i < NB_SIDES; i++) {
        int f = first_air_layer(nb ? nb[i] : NULL);

        if (f < low)
            low = f;
    }
    *lo = low < 0 ? 0 : low;
    *hi = solid_top(c);
}

int chunk_mesh_count(const Chunk *c, const Chunk *const nb[NB_COUNT])
{
    int x, y, z, f, n = 0, lo, hi;

    face_layers(c, nb, &lo, &hi);
    for (y = lo; y < hi; y++)
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++) {
                int b = c->blocks[y][z][x];

                if (b == BLOCK_AIR)
                    continue;
                if (b == BLOCK_TORCH) {
                    n += MESH_TORCH_FACES;
                    continue;
                }
                for (f = 0; f < FACE_COUNT; f++)
                    n += chunk_face_visible(c, nb, x, y, z, f);
            }
    return n;
}

int chunk_mesh_build(const Chunk *c, const Chunk *const nb[NB_COUNT], float ox, float oz,
                     MeshVertex *out, int max_verts)
{
    int x, y, z, f, n = 0, lo, hi;
    bool lit;

    face_layers(c, nb, &lo, &hi);
    /* The cells the faces look into: one layer under lo up to hi. */
    lit = lo < hi && light_build(c, nb, lo - 1, hi);
    for (y = lo; y < hi; y++)
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++) {
                int b = c->blocks[y][z][x];

                if (b == BLOCK_AIR)
                    continue;
                if (b == BLOCK_TORCH) {
                    if (n + MESH_TORCH_VERTS > max_verts)
                        return -1;
                    n += mesh_torch(out + n, ox + (float)x, (float)y, oz + (float)z);
                    continue;
                }
                for (f = 0; f < FACE_COUNT; f++) {
                    unsigned char ao[4];
                    float t[4];

                    if (!chunk_face_visible(c, nb, x, y, z, f))
                        continue;
                    if (n + MESH_FACE_VERTS > max_verts)
                        return -1;
                    face_ao(c, nb, x, y, z, f, lit, ao, t);
                    mesh_face_lit(out + n, ox + (float)x, (float)y, oz + (float)z, f,
                                  block_face_tile(b, f), ao, t);
                    n += MESH_FACE_VERTS;
                }
            }
    return n;
}
