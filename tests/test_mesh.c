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
    CHECK_EQ_INT(block_face_tile(BLOCK_AIR, FACE_POS_Y), -1);
    CHECK_EQ_INT(block_face_tile(BLOCK_COUNT, FACE_POS_Y), -1);
    CHECK_EQ_INT(block_face_tile(BLOCK_GRASS, FACE_COUNT), -1);
    CHECK_EQ_INT(block_face_tile(-1, FACE_POS_Y), -1);

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
