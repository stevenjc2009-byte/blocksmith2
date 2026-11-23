#include <stddef.h>
#include <string.h>

#include "block.h"
#include "blocktex.h"
#include "light.h"
#include "water.h"
#include "water_mesh.h"

/* What a cell is to a water face beside it (water_mesh.h). */
enum { K_OPEN, K_SOLID, K_WATER };

/* The cells the water looks at, judged once: local x and z from -1 to
 * CHUNK_SIZE (only the four sides are filled past the chunk), y from -1 to
 * WORLD_HEIGHT, at s_kind[y + 1][z + 1][x + 1]. */
#define PAD (CHUNK_SIZE + 2)

static uint8_t s_kind[WORLD_HEIGHT + 2][PAD][PAD];
static uint8_t s_kind_of[256]; /* K_* of every byte */
static bool s_tables;

/* One step out of each face (FACE_* order) in s_kind. */
static const int k_pad_step[FACE_COUNT] = { 1, -1, PAD * PAD, -PAD * PAD, PAD, -PAD };

/* Triangles 0 1 2 and 0 2 3 of a face's corners. */
static const unsigned char k_tri[MESH_FACE_VERTS] = { 0, 1, 2, 0, 2, 3 };

/* The kind of local cell (x, y, z) of neighbour o (K_OPEN if missing). */
static uint8_t kind_in(const Chunk *o, int x, int y, int z)
{
    return o ? s_kind_of[o->blocks[y][z][x]] : K_OPEN;
}

/* Judge rows ylo to yhi of s_kind (-1 <= ylo, yhi <= WORLD_HEIGHT). */
static void fill_kinds(const Chunk *c, const Chunk *const nb[NB_COUNT], int ylo, int yhi)
{
    const Chunk *px = nb ? nb[NB_POS_X] : NULL, *nx = nb ? nb[NB_NEG_X] : NULL;
    const Chunk *pz = nb ? nb[NB_POS_Z] : NULL, *nz = nb ? nb[NB_NEG_Z] : NULL;
    int i, x, y, z;

    if (!s_tables) {
        for (i = 0; i < 256; i++)
            s_kind_of[i] = i == BLOCK_WATER ? K_WATER : block_solid(i) ? K_SOLID : K_OPEN;
        s_tables = true;
    }
    for (y = ylo; y <= yhi; y++) {
        uint8_t (*row)[PAD] = s_kind[y + 1];

        if (y < 0 || y >= WORLD_HEIGHT) {
            memset(row, y < 0 ? K_SOLID : K_OPEN, sizeof s_kind[0]); /* under: solid; over: open */
            continue;
        }
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++)
                row[z + 1][x + 1] = s_kind_of[c->blocks[y][z][x]];
        for (i = 0; i < CHUNK_SIZE; i++) {
            row[i + 1][PAD - 1] = kind_in(px, 0, y, i);
            row[i + 1][0] = kind_in(nx, CHUNK_SIZE - 1, y, i);
            row[PAD - 1][i + 1] = kind_in(pz, i, y, 0);
            row[0][i + 1] = kind_in(nz, i, y, CHUNK_SIZE - 1);
        }
    }
}

/* The layers [*lo, *hi) holding any water; false if none. */
static bool water_layers(const Chunk *c, int *lo, int *hi)
{
    int y;

    for (y = 0; y < WORLD_HEIGHT && !memchr(c->blocks[y], BLOCK_WATER, sizeof c->blocks[y]); y++)
        ;
    if (y == WORLD_HEIGHT)
        return false;
    *lo = y;
    for (y = WORLD_HEIGHT; !memchr(c->blocks[y - 1], BLOCK_WATER, sizeof c->blocks[y - 1]); y--)
        ;
    *hi = y;
    return true;
}

/* One face of the water cell at local (x, y, z), h high. */
static void put_face(MeshVertex *out, float ox, float oz, int x, int y, int z, int f, float h,
                     const float uv[4], float t)
{
    float shade = mesh_face_shade(f);
    int i;

    for (i = 0; i < MESH_FACE_VERTS; i++) {
        int c = k_tri[i];
        const signed char *p = mesh_face_corner(f, c);
        MeshVertex *v = &out[i];

        v->x = ox + (float)x + (float)p[0];
        v->y = (float)y + (p[1] ? h : 0.0f);
        v->z = oz + (float)z + (float)p[2];
        v->u = (c == 1 || c == 2) ? uv[2] : uv[0];
        v->v = c >= 2 ? uv[3] : uv[1];
        v->r = v->g = v->b = shade;
        v->t = t;
    }
}

/* Count the faces (out NULL) or write them; the vertex count, or -1 when
 * they would not fit in max_verts. */
static int walk(const Chunk *c, const Chunk *const nb[NB_COUNT], float ox, float oz,
                MeshVertex *out, int max_verts)
{
    float full[4], low[4];
    int x, y, z, f, n = 0, lo, hi;
    bool lit = false;

    if (!water_layers(c, &lo, &hi))
        return 0;
    fill_kinds(c, nb, lo - 1, hi);
    if (out) {
        blocktex_tile_uv(TILE_WATER, full);
        blocktex_sub_uv(TILE_WATER, 0, BLOCKTEX_TILE - WATER_TOP_TEXELS, BLOCKTEX_TILE,
                        BLOCKTEX_TILE, low); /* a lowered side: the tile's bottom rows */
        lit = light_build(c, nb, lo, hi - 1);
    }
    for (y = lo; y < hi; y++)
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++) {
                const uint8_t *p = &s_kind[y + 1][z + 1][x + 1];
                bool topped;
                float t;

                if (*p != K_WATER)
                    continue;
                topped = p[k_pad_step[FACE_POS_Y]] == K_WATER;
                t = lit ? light_brightness(light_level(x, y, z)) : 0.0f;
                for (f = 0; f < FACE_COUNT; f++) {
                    int k = p[k_pad_step[f]];

                    if (f == FACE_POS_Y ? k == K_WATER : k != K_OPEN)
                        continue;
                    if (out) {
                        if (n + MESH_FACE_VERTS > max_verts)
                            return -1;
                        put_face(out + n, ox, oz, x, y, z, f, topped ? 1.0f : WATER_TOP,
                                 topped || f == FACE_POS_Y || f == FACE_NEG_Y ? full : low, t);
                    }
                    n += MESH_FACE_VERTS;
                }
            }
    return n;
}

int water_mesh_count(const Chunk *c, const Chunk *const nb[NB_COUNT])
{
    return walk(c, nb, 0.0f, 0.0f, NULL, 0) / MESH_FACE_VERTS;
}

int water_mesh_build(const Chunk *c, const Chunk *const nb[NB_COUNT], float ox, float oz,
                     MeshVertex *out, int max_verts)
{
    return walk(c, nb, ox, oz, out, max_verts);
}
