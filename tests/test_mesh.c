#include <math.h>

#include "block.h"
#include "blocktex.h"
#include "mesh.h"
#include "test.h"

#define TOL 1e-5

static const float k_out[FACE_COUNT][3] = {
    { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 },
};

/* Cross product of the two edges leaving corner 0 of a triangle. */
static void tri_cross(const MeshVertex *v, float c[3])
{
    float a[3] = { v[1].x - v[0].x, v[1].y - v[0].y, v[1].z - v[0].z };
    float b[3] = { v[2].x - v[0].x, v[2].y - v[0].y, v[2].z - v[0].z };

    c[0] = a[1] * b[2] - a[2] * b[1];
    c[1] = a[2] * b[0] - a[0] * b[2];
    c[2] = a[0] * b[1] - a[1] * b[0];
}

static void check_face(int face)
{
    const float bx = 3.0f, by = -2.0f, bz = 7.0f;
    MeshVertex v[MESH_FACE_VERTS];
    float uv[4], area = 0.0f;
    float shade = mesh_face_shade(face);
    int i, k, fails_before = g_fails;

    mesh_face(v, bx, by, bz, face, TILE_GRASS_SIDE);
    blocktex_tile_uv(TILE_GRASS_SIDE, uv);

    for (k = 0; k < 2; k++) {
        const MeshVertex *t = &v[k * 3];
        float c[3], len, du1, dv1, du2, dv2;

        /* Right-hand rule: the triangle faces outward when its corners go
         * counter-clockwise as seen from outside. */
        tri_cross(t, c);
        len = sqrtf(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
        CHECK_NEAR(c[0] / len, k_out[face][0], TOL);
        CHECK_NEAR(c[1] / len, k_out[face][1], TOL);
        CHECK_NEAR(c[2] / len, k_out[face][2], TOL);
        area += 0.5f * len;

        /* Counter-clockwise in texture space too, so the tile is not
         * mirrored. */
        du1 = t[1].u - t[0].u;
        dv1 = t[1].v - t[0].v;
        du2 = t[2].u - t[0].u;
        dv2 = t[2].v - t[0].v;
        CHECK_EQ_INT(du1 * dv2 - dv1 * du2 > 0.0f, 1);
    }
    /* The two triangles cover the whole square. */
    CHECK_NEAR(area, 1.0, TOL);

    for (i = 0; i < MESH_FACE_VERTS; i++) {
        float lx = v[i].x - bx, ly = v[i].y - by, lz = v[i].z - bz;

        /* Corners of the unit box only. */
        CHECK_EQ_INT((lx == 0.0f || lx == 1.0f) && (ly == 0.0f || ly == 1.0f) &&
                     (lz == 0.0f || lz == 1.0f), 1);
        /* On the face's own side of the box. */
        if (face == FACE_POS_X) CHECK_NEAR(lx, 1.0, TOL);
        if (face == FACE_NEG_X) CHECK_NEAR(lx, 0.0, TOL);
        if (face == FACE_POS_Y) CHECK_NEAR(ly, 1.0, TOL);
        if (face == FACE_NEG_Y) CHECK_NEAR(ly, 0.0, TOL);
        if (face == FACE_POS_Z) CHECK_NEAR(lz, 1.0, TOL);
        if (face == FACE_NEG_Z) CHECK_NEAR(lz, 0.0, TOL);
        /* Texture corners are the tile's corners. */
        CHECK_EQ_INT(v[i].u == uv[0] || v[i].u == uv[2], 1);
        CHECK_EQ_INT(v[i].v == uv[1] || v[i].v == uv[3], 1);
        /* Side faces stand the tile upright. */
        if (face != FACE_POS_Y && face != FACE_NEG_Y)
            CHECK_NEAR(v[i].v, ly == 1.0f ? uv[3] : uv[1], TOL);
        CHECK_NEAR(v[i].r, shade, TOL);
        CHECK_NEAR(v[i].g, shade, TOL);
        CHECK_NEAR(v[i].b, shade, TOL);
    }

    if (g_fails != fails_before)
        printf("     (face %d)\n", face);
}

/* Row 3-03: each corner carries its torch light, and a torch is a thin
 * stick (five faces round a 2/16 square, 10/16 tall) that glows fully. */
static void test_torch_mesh(void)
{
    static const unsigned char ao[4] = { 3, 2, 1, 0 };
    static const float lit[4] = { 0.1f, 0.3f, 0.6f, 0.9f };
    static const int faces[MESH_TORCH_FACES] = { FACE_POS_X, FACE_NEG_X, FACE_POS_Y, FACE_POS_Z,
                                                 FACE_NEG_Z };
    const float h = MESH_TORCH_HALF, top_y = 4.0f + MESH_TORCH_HEIGHT;
    MeshVertex v[MESH_BLOCK_VERTS];
    float side[4], top[4];
    int f, i, k, match, bad;

    CHECK_EQ_INT((int)sizeof(MeshVertex), 36);
    for (f = 0; f < FACE_COUNT; f++) {
        mesh_face_lit(v, 3, 4, 5, f, TILE_STONE, ao, lit);
        match = 0;
        for (i = 0; i < MESH_FACE_VERTS; i++)
            for (k = 0; k < 4; k++) {
                const signed char *q = mesh_face_corner(f, k);

                if (fabsf(v[i].x - (3 + q[0])) < TOL && fabsf(v[i].y - (4 + q[1])) < TOL &&
                    fabsf(v[i].z - (5 + q[2])) < TOL) {
                    CHECK_NEAR(v[i].t, lit[k], TOL);
                    CHECK_NEAR(v[i].r, mesh_face_shade(f) * mesh_ao_scale(ao[k]), TOL);
                    match++;
                }
            }
        CHECK_EQ_INT(match, MESH_FACE_VERTS);
        mesh_face_ao(v, 3, 4, 5, f, TILE_STONE, ao);
        for (i = 0, bad = 0; i < MESH_FACE_VERTS; i++)
            bad += v[i].t != 0.0f;
        mesh_face(v, 3, 4, 5, f, TILE_STONE);
        for (i = 0; i < MESH_FACE_VERTS; i++)
            bad += v[i].t != 0.0f;
        CHECK_EQ_INT(bad, 0);
    }

    CHECK_EQ_INT(MESH_TORCH_VERTS, 30);
    CHECK_EQ_INT(mesh_block(v, 3, 4, 5, BLOCK_TORCH), MESH_TORCH_VERTS);
    CHECK_EQ_INT(mesh_torch(v, 3, 4, 5), MESH_TORCH_VERTS);
    blocktex_sub_uv(TILE_TORCH, BLOCKTEX_TORCH_X0, BLOCKTEX_TORCH_Y0, BLOCKTEX_TORCH_X1,
                    BLOCKTEX_TORCH_Y1, side);
    blocktex_sub_uv(TILE_TORCH, BLOCKTEX_TORCH_X0, BLOCKTEX_TORCH_Y0, BLOCKTEX_TORCH_X1,
                    BLOCKTEX_TORCH_FLAME_Y1, top);
    for (k = 0; k < MESH_TORCH_FACES; k++) {
        const float *uv = faces[k] == FACE_POS_Y ? top : side;
        const float plane[FACE_COUNT] = { 3.5f + h, 3.5f - h, top_y, 0, 5.5f + h, 5.5f - h };
        int axis = faces[k] / 2;

        f = faces[k];
        bad = 0;
        for (i = 0; i < MESH_FACE_VERTS; i++) {
            const MeshVertex *p = &v[k * MESH_FACE_VERTS + i];
            float at[3] = { p->x, p->y, p->z };

            bad += p->x < 3.5f - h - TOL || p->x > 3.5f + h + TOL;
            bad += p->z < 5.5f - h - TOL || p->z > 5.5f + h + TOL;
            bad += p->y < 4.0f - TOL || p->y > top_y + TOL;
            bad += fabsf(at[axis] - plane[f]) > TOL;
            bad += fabsf(p->t - 1.0f) > TOL || fabsf(p->r - mesh_face_shade(f)) > TOL;
            bad += p->u < uv[0] - TOL || p->u > uv[2] + TOL || p->v < uv[1] - TOL ||
                   p->v > uv[3] + TOL;
        }
        CHECK_EQ_INT(bad, 0);
        for (i = 0; i < MESH_FACE_VERTS; i += 3) {
            float c[3];

            tri_cross(&v[k * MESH_FACE_VERTS + i], c);
            CHECK_EQ_INT(c[0] * k_out[f][0] + c[1] * k_out[f][1] + c[2] * k_out[f][2] > 0.0f, 1);
        }
    }
}

void test_mesh(void)
{
    MeshVertex v[MESH_BLOCK_VERTS];
    float uv[4];
    int f, i;

    CHECK_NEAR(mesh_face_shade(FACE_POS_Y), 1.0, TOL);
    CHECK_NEAR(mesh_face_shade(FACE_POS_X), 0.8, TOL);
    CHECK_NEAR(mesh_face_shade(FACE_NEG_X), 0.8, TOL);
    CHECK_NEAR(mesh_face_shade(FACE_POS_Z), 0.6, TOL);
    CHECK_NEAR(mesh_face_shade(FACE_NEG_Z), 0.6, TOL);
    CHECK_NEAR(mesh_face_shade(FACE_NEG_Y), 0.5, TOL);

    for (f = 0; f < FACE_COUNT; f++)
        check_face(f);

    /* Grass: grass on top, dirt underneath, grass-edged dirt round the sides. */
    CHECK_EQ_INT(block_face_tile(BLOCK_GRASS, FACE_POS_Y), TILE_GRASS_TOP);
    CHECK_EQ_INT(block_face_tile(BLOCK_GRASS, FACE_NEG_Y), TILE_DIRT);
    CHECK_EQ_INT(block_face_tile(BLOCK_GRASS, FACE_POS_X), TILE_GRASS_SIDE);
    CHECK_EQ_INT(block_face_tile(BLOCK_GRASS, FACE_NEG_Z), TILE_GRASS_SIDE);
    CHECK_EQ_INT(block_face_tile(BLOCK_DIRT, FACE_POS_Y), TILE_DIRT);
    CHECK_EQ_INT(block_face_tile(BLOCK_STONE, FACE_NEG_X), TILE_STONE);
    for (f = 0; f < FACE_COUNT; f++) { /* row 4-01: one tile all round */
        CHECK_EQ_INT(block_face_tile(BLOCK_SAND, f), TILE_SAND);
        CHECK_EQ_INT(block_face_tile(BLOCK_WATER, f), TILE_WATER);
    }
    CHECK_EQ_INT(block_face_tile(BLOCK_AIR, FACE_POS_Y), -1);
    CHECK_EQ_INT(block_face_tile(BLOCK_COUNT, FACE_POS_Y), -1);
    CHECK_EQ_INT(block_face_tile(BLOCK_GRASS, FACE_COUNT), -1);
    CHECK_EQ_INT(block_face_tile(-1, FACE_POS_Y), -1);

    test_torch_mesh();
    CHECK_EQ_INT(mesh_block(v, 0, 0, 0, BLOCK_AIR), 0);
    CHECK_EQ_INT(mesh_block(v, 0, 0, 0, BLOCK_COUNT), 0);
    CHECK_EQ_INT(mesh_block(v, 0, 0, 0, BLOCK_GRASS), MESH_BLOCK_VERTS);
    /* Face f is vertices f*6 .. f*6+5, each on its own tile. */
    for (f = 0; f < FACE_COUNT; f++) {
        blocktex_tile_uv(block_face_tile(BLOCK_GRASS, f), uv);
        for (i = 0; i < MESH_FACE_VERTS; i++) {
            const MeshVertex *p = &v[f * MESH_FACE_VERTS + i];
            CHECK_EQ_INT(p->u >= uv[0] && p->u <= uv[2] && p->v >= uv[1] && p->v <= uv[3], 1);
        }
    }
}
