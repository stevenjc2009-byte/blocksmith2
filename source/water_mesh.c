#include <stddef.h>
#include <string.h>

#include "biome.h"
#include "block.h"
#include "blocktex.h"
#include "light.h"
#include "water.h"
#include "water_mesh.h"

/* What a cell is to a water face beside it (water_mesh.h): open, solid or
 * water, water being K_WATER plus its level (water_level(): K_WATER a
 * source, then flowing levels 1 to 7, K_FALL falling). */
enum { K_OPEN, K_SOLID, K_WATER, K_FALL = K_WATER + WATER_FALLING, K_KINDS };

/* The cells the water looks at, judged once: local x and z from -1 to
 * CHUNK_SIZE (past the chunk: the four sides and, for the corners' heights,
 * the four corners), y from -1 to WORLD_HEIGHT, at
 * s_kind[y + 1][z + 1][x + 1]. */
#define PAD (CHUNK_SIZE + 2)

static uint8_t s_kind[WORLD_HEIGHT + 2][PAD][PAD];
static uint8_t s_kind_of[256]; /* K_* of every byte */
static float s_height_of[K_KINDS]; /* water_height() of each water kind */
static float s_tint[BIOME_COUNT][3]; /* biome_water_rgb() of every biome (row 4-02) */
static bool s_tables;
static bool s_flowing; /* fill_kinds() met flowing or falling water */
static float s_full[4], s_low[4]; /* the whole tile; a lowered side's rows */

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
    const Chunk *pp = nb ? nb[NB_POS_X_POS_Z] : NULL, *np = nb ? nb[NB_NEG_X_POS_Z] : NULL;
    const Chunk *pn = nb ? nb[NB_POS_X_NEG_Z] : NULL, *nn = nb ? nb[NB_NEG_X_NEG_Z] : NULL;
    int i, x, y, z;

    if (!s_tables) {
        for (i = 0; i < 256; i++)
            s_kind_of[i] = block_is_water(i) ? K_WATER + water_level(i)
                           : block_solid(i)  ? K_SOLID
                                             : K_OPEN;
        for (i = K_WATER; i < K_KINDS; i++)
            s_height_of[i] = water_height(water_block(i - K_WATER));
        for (i = 0; i < BIOME_COUNT; i++)
            biome_water_rgb(i, s_tint[i]);
        s_tables = true;
    }
    s_flowing = false;
    for (y = ylo; y <= yhi; y++) {
        uint8_t (*row)[PAD] = s_kind[y + 1];

        if (y < 0 || y >= WORLD_HEIGHT) {
            memset(row, y < 0 ? K_SOLID : K_OPEN, sizeof s_kind[0]); /* under: solid; over: open */
            continue;
        }
        s_flowing |= water_row_flowing(c->blocks[y][0], (int)sizeof c->blocks[y]);
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++)
                row[z + 1][x + 1] = s_kind_of[c->blocks[y][z][x]];
        for (i = 0; i < CHUNK_SIZE; i++) {
            row[i + 1][PAD - 1] = kind_in(px, 0, y, i);
            row[i + 1][0] = kind_in(nx, CHUNK_SIZE - 1, y, i);
            row[PAD - 1][i + 1] = kind_in(pz, i, y, 0);
            row[0][i + 1] = kind_in(nz, i, y, CHUNK_SIZE - 1);
        }
        row[PAD - 1][PAD - 1] = kind_in(pp, 0, y, 0);
        row[PAD - 1][0] = kind_in(np, CHUNK_SIZE - 1, y, 0);
        row[0][PAD - 1] = kind_in(pn, 0, y, CHUNK_SIZE - 1);
        row[0][0] = kind_in(nn, CHUNK_SIZE - 1, y, CHUNK_SIZE - 1);
        for (i = 0; i < PAD && !s_flowing; i++) /* the rim, from the neighbours */
            s_flowing = row[0][i] > K_WATER || row[PAD - 1][i] > K_WATER ||
                        row[i][0] > K_WATER || row[i][PAD - 1] > K_WATER;
    }
}

/* Row 4-04: the height up layer y of top corner (X, Z) (local, 0 to
 * CHUNK_SIZE) of a water cell beside it with no water above (water_mesh.h),
 * from the four cells round the corner, in the same order in every chunk
 * so next-door chunks get the very same number. */
static float corner_height(int y, int X, int Z)
{
    static const int k_round[4][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };
    float sum = 0.0f;
    int i, n = 0;
    bool still = true, topped = false;

    for (i = 0; i < 4; i++) {
        int k = s_kind[y + 1][Z + k_round[i][1]][X + k_round[i][0]];
        int up = s_kind[y + 2][Z + k_round[i][1]][X + k_round[i][0]];

        if (up >= K_WATER) {
            still &= up == K_WATER;
            topped |= k >= K_WATER;
        }
        if (k >= K_WATER) {
            still &= k == K_WATER;
            sum += s_height_of[k];
            n++;
        }
    }
    if (still)
        return WATER_TOP; /* only still water round it: as row 4-01 */
    return topped ? 1.0f : sum / (float)n;
}

/* The layers [*lo, *hi) holding any water; false if none. */
static bool water_layers(const Chunk *c, int *lo, int *hi)
{
    int y;

    for (y = 0; y < WORLD_HEIGHT && !water_row_has(c->blocks[y][0], (int)sizeof c->blocks[y]); y++)
        ;
    if (y == WORLD_HEIGHT)
        return false;
    *lo = y;
    for (y = WORLD_HEIGHT; !water_row_has(c->blocks[y - 1][0], (int)sizeof c->blocks[y - 1]); y--)
        ;
    *hi = y;
    return true;
}

/* One face of the water cell at local (x, y, z), its top corners h[dx +
 * 2 * dz] high (dx, dz 0 or 1 across the cell). A lowered side (low: a
 * side of a cell with no water above) shows the tile's rows up to each top
 * corner's height, so the ripples are not squeezed. Its colours are the
 * face's shade times its column's biome water colour (rgb). */
static void put_face(MeshVertex *out, float ox, float oz, int x, int y, int z, int f,
                     const float h[4], bool low, float t, const float rgb[3])
{
    const float *uv = low ? s_low : s_full;
    float shade = mesh_face_shade(f);
    int i;

    for (i = 0; i < MESH_FACE_VERTS; i++) {
        int c = k_tri[i];
        const signed char *p = mesh_face_corner(f, c);
        float hc = h[p[0] + 2 * p[2]];
        MeshVertex *v = &out[i];

        v->x = ox + (float)x + (float)p[0];
        v->y = (float)y + (p[1] ? hc : 0.0f);
        v->z = oz + (float)z + (float)p[2];
        v->u = (c == 1 || c == 2) ? uv[2] : uv[0];
        if (c < 2)
            v->v = uv[1];
        else if (!low || hc == WATER_TOP)
            v->v = uv[3];
        else
            v->v = s_full[3] - (1.0f - hc) * ((float)BLOCKTEX_TILE / (float)BLOCKTEX_SIZE);
        v->r = shade * rgb[0];
        v->g = shade * rgb[1];
        v->b = shade * rgb[2];
        v->t = t;
    }
}

/* The heights of the top corners of the water cell at local (x, y, z)
 * (put_face()): a cell with water above fills its cell; with no flowing
 * water near, still water is WATER_TOP high (row 4-01); else each corner
 * from the cells round it (corner_height()). */
static void cell_heights(int y, int x, int z, bool topped, float h[4])
{
    int i;

    for (i = 0; i < 4; i++)
        h[i] = topped ? 1.0f : !s_flowing ? WATER_TOP : corner_height(y, x + (i & 1), z + (i >> 1));
}

/* Count the faces (out NULL) or write them; the vertex count, or -1 when
 * they would not fit in max_verts. */
static int walk(const Chunk *c, const Chunk *const nb[NB_COUNT], float ox, float oz,
                MeshVertex *out, int max_verts)
{
    int x, y, z, f, n = 0, lo, hi;
    bool lit = false;

    if (!water_layers(c, &lo, &hi))
        return 0;
    fill_kinds(c, nb, lo - 1, hi);
    if (out) {
        blocktex_tile_uv(TILE_WATER, s_full);
        blocktex_sub_uv(TILE_WATER, 0, BLOCKTEX_TILE - WATER_TOP_TEXELS, BLOCKTEX_TILE,
                        BLOCKTEX_TILE, s_low); /* a lowered side: the tile's bottom rows */
        lit = light_build(c, nb, lo, hi - 1);
    }
    for (y = lo; y < hi; y++)
        for (z = 0; z < CHUNK_SIZE; z++)
            for (x = 0; x < CHUNK_SIZE; x++) {
                const uint8_t *p = &s_kind[y + 1][z + 1][x + 1];
                bool topped, ready = false;
                float t = 0.0f, h[4];

                if (*p < K_WATER)
                    continue;
                topped = p[k_pad_step[FACE_POS_Y]] >= K_WATER;
                for (f = 0; f < FACE_COUNT; f++) {
                    int k = p[k_pad_step[f]];

                    if (f == FACE_POS_Y ? k >= K_WATER : k != K_OPEN)
                        continue;
                    if (out) {
                        if (n + MESH_FACE_VERTS > max_verts)
                            return -1;
                        if (!ready) { /* once a cell, at its first face */
                            cell_heights(y, x, z, topped, h);
                            t = lit ? light_brightness(light_level(x, y, z)) : 0.0f;
                            ready = true;
                        }
                        put_face(out + n, ox, oz, x, y, z, f, h,
                                 !topped && f != FACE_POS_Y && f != FACE_NEG_Y, t,
                                 s_tint[c->biome[z][x] < BIOME_COUNT ? c->biome[z][x] : 0]);
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
