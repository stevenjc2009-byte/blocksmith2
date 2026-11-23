#include <math.h>
#include <stdint.h>

#include "block.h"
#include "blocktex.h"
#include "cloud.h"

/* A cell is cloud where its own random value, counted twice, plus its four
 * neighbours' is over CLOUD_LEVEL of the most it could be: the neighbours
 * pull it towards their mood, so clouds come in clumps, not a speckle. */
#define CLOUD_LEVEL 0.56f

static const float k_shade[FACE_COUNT] = { 0.9f, 0.9f, 1.0f, 0.8f, 0.9f, 0.9f };

static uint32_t wrap(int i)
{
    i %= CLOUD_PERIOD;
    return (uint32_t)(i < 0 ? i + CLOUD_PERIOD : i);
}

/* 0 up to (not including) 1, fixed for each cell of the pattern. */
static float value(int i, int j)
{
    uint32_t h = wrap(i) * 0x8DA6B343u ^ wrap(j) * 0xD8163841u ^ 0x3C6EF372u;

    h ^= h >> 13;
    h *= 0x5BD1E995u;
    h ^= h >> 15;
    return (float)(h >> 8) / 16777216.0f;
}

bool cloud_at(int i, int j)
{
    float v = 2.0f * value(i, j) + value(i - 1, j) + value(i + 1, j) + value(i, j - 1) +
              value(i, j + 1);

    return v > 6.0f * CLOUD_LEVEL;
}

float cloud_shift(float t)
{
    return t * (float)(CLOUD_PERIOD * CLOUD_CELL);
}

/* One face of the slab whose low corner is (x, CLOUD_Y, z): the face's
 * corners as mesh.c's (mesh_face_corner()), stretched to CLOUD_CELL across
 * and CLOUD_THICK up, the whole tile on it. */
static void slab_face(MeshVertex out[MESH_FACE_VERTS], float x, float z, int face)
{
    static const unsigned char k_order[MESH_FACE_VERTS] = { 0, 1, 2, 0, 2, 3 };
    float uv[4];
    int i;

    blocktex_tile_uv(TILE_CLOUD, uv);
    for (i = 0; i < MESH_FACE_VERTS; i++) {
        int c = k_order[i];
        const signed char *p = mesh_face_corner(face, c);
        MeshVertex *v = &out[i];

        v->x = x + (float)(p[0] * CLOUD_CELL);
        v->y = CLOUD_Y + (float)p[1] * CLOUD_THICK;
        v->z = z + (float)(p[2] * CLOUD_CELL);
        v->u = (c == 0 || c == 3) ? uv[0] : uv[2];
        v->v = (c < 2) ? uv[1] : uv[3];
        v->r = v->g = v->b = k_shade[face];
        v->t = 0.0f;
    }
}

/* The four side faces and the cell next door across each. */
static const struct {
    int face, di, dj;
} k_sides[4] = {
    { FACE_POS_X, 1, 0 }, { FACE_NEG_X, -1, 0 }, { FACE_POS_Z, 0, 1 }, { FACE_NEG_Z, 0, -1 },
};

int cloud_build(float ex, float ez, float t, MeshVertex out[CLOUD_VERTS_MAX])
{
    float shift = cloud_shift(t);
    float px = ex - shift; /* the eye in the pattern, before the drift */
    int i0 = (int)floorf((px - CLOUD_RADIUS) / CLOUD_CELL);
    int i1 = (int)floorf((px + CLOUD_RADIUS) / CLOUD_CELL);
    int j0 = (int)floorf((ez - CLOUD_RADIUS) / CLOUD_CELL);
    int j1 = (int)floorf((ez + CLOUD_RADIUS) / CLOUD_CELL);
    int i, j, k, n = 0;

    for (j = j0; j <= j1; j++) {
        for (i = i0; i <= i1; i++) {
            float x0 = (float)(i * CLOUD_CELL), z0 = (float)(j * CLOUD_CELL);
            /* The nearest point of the cell to the eye, across the ground. */
            float dx = fminf(fmaxf(px, x0), x0 + CLOUD_CELL) - px;
            float dz = fminf(fmaxf(ez, z0), z0 + CLOUD_CELL) - ez;

            if (!cloud_at(i, j) || dx * dx + dz * dz > CLOUD_RADIUS * CLOUD_RADIUS)
                continue;
            if (n + 6 * MESH_FACE_VERTS > CLOUD_VERTS_MAX)
                return n; /* never, by the PC tests; a whole cell or none */
            slab_face(out + n, x0 + shift, z0, FACE_NEG_Y);
            n += MESH_FACE_VERTS;
            slab_face(out + n, x0 + shift, z0, FACE_POS_Y);
            n += MESH_FACE_VERTS;
            for (k = 0; k < 4; k++) {
                if (cloud_at(i + k_sides[k].di, j + k_sides[k].dj))
                    continue;
                slab_face(out + n, x0 + shift, z0, k_sides[k].face);
                n += MESH_FACE_VERTS;
            }
        }
    }
    return n;
}
