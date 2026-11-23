#include <math.h>
#include <string.h>

#include "block.h"
#include "blocktex.h"
#include "camera.h"
#include "mesh.h"
#include "particles.h"
#include "test.h"

/* Row 3-05: the break bits (particles.h). */

#define TOL 1e-4
#define R (PARTICLES_SIZE * 0.5f)

static World s_w;
static Particles s_ps, s_ps2;
static MeshVertex s_out[PARTICLES_VERTS_MAX];

/* Live bits with their middle inside the box (x0, y0, z0) to (x1, y1, z1). */
static int count_in(const Particles *ps, float x0, float y0, float z0, float x1, float y1,
                    float z1)
{
    int k, n = 0;

    for (k = 0; k < PARTICLES_MAX; k++) {
        const Particle *p = &ps->p[k];

        n += p->life > 0.0f && p->x >= x0 && p->x <= x1 && p->y >= y0 && p->y <= y1 &&
             p->z >= z0 && p->z <= z1;
    }
    return n;
}

/* A piece inside the part `in` of the picture (blocktex uv order), as wide
 * and tall as `size`. */
static int piece_ok(const float uv[4], const float in[4], const float size[4])
{
    return uv[0] >= in[0] - TOL && uv[2] <= in[2] + TOL && uv[1] >= in[1] - TOL &&
           uv[3] <= in[3] + TOL && fabsf((uv[2] - uv[0]) - (size[2] - size[0])) < TOL &&
           fabsf((uv[3] - uv[1]) - (size[3] - size[1])) < TOL;
}

/* A grass block's bits: one from each cell of the grid inside the block,
 * flying out from its middle and up, each a 4 x 4 piece of the side tile
 * (not the top), with the light given and a life in range. */
static void test_burst(void)
{
    const int side = block_face_tile(BLOCK_GRASS, FACE_POS_X);
    float in[4], size[4], top[4];
    int k, outside = 0, wrong_way = 0, down = 0, bad_uv = 0, on_top = 0, same = 0, bad_t = 0,
           bad_life = 0;

    particles_init(&s_ps);
    CHECK_EQ_INT(particles_live(&s_ps), 0);
    particles_burst(&s_ps, 3, 60, -5, BLOCK_GRASS, 0.5f);
    CHECK_EQ_INT(particles_live(&s_ps), PARTICLES_PER_BREAK);
    CHECK_EQ_INT(PARTICLES_PER_BREAK, 27);
    blocktex_sub_uv(side, 0, 0, BLOCKTEX_TILE, BLOCKTEX_TILE, in);
    blocktex_sub_uv(side, 0, 0, PARTICLES_TEXELS, PARTICLES_TEXELS, size);
    blocktex_tile_uv(block_face_tile(BLOCK_GRASS, FACE_POS_Y), top);
    CHECK_EQ_INT(side != block_face_tile(BLOCK_GRASS, FACE_POS_Y), 1);
    for (k = 0; k < PARTICLES_PER_BREAK; k++) {
        const Particle *p = &s_ps.p[k];
        float dx = p->x - 3.5f, dz = p->z + 4.5f;

        outside += !(p->x > 3.0f + R && p->x < 4.0f - R && p->y > 60.0f + R &&
                     p->y < 61.0f - R && p->z > -5.0f + R && p->z < -4.0f - R);
        wrong_way += (fabsf(dx) > 0.1f && dx * p->vx <= 0.0f) +
                     (fabsf(dz) > 0.1f && dz * p->vz <= 0.0f);
        down += p->vy <= 0.0f;
        bad_uv += !piece_ok(p->uv, in, size);
        on_top += p->uv[0] >= top[0] && p->uv[2] <= top[2] && p->uv[1] >= top[1] &&
                  p->uv[3] <= top[3];
        same += fabsf(p->uv[0] - s_ps.p[0].uv[0]) < TOL && fabsf(p->uv[1] - s_ps.p[0].uv[1]) < TOL;
        bad_t += p->t != 0.5f;
        bad_life += p->life < PARTICLES_LIFE_MIN || p->life > PARTICLES_LIFE_MAX;
    }
    CHECK_EQ_INT(outside, 0);
    CHECK_EQ_INT(wrong_way, 0);
    CHECK_EQ_INT(down, 0);
    CHECK_EQ_INT(bad_uv, 0);
    CHECK_EQ_INT(on_top, 0);
    CHECK_EQ_INT(same < 10, 1); /* the pieces differ */
    CHECK_EQ_INT(bad_t, 0);
    CHECK_EQ_INT(bad_life, 0);
}

/* A torch's bits fill its thin stick and show pieces of the stick. */
static void test_torch_burst(void)
{
    const float lo = 0.5f - MESH_TORCH_HALF, hi = 0.5f + MESH_TORCH_HALF;
    float in[4], size[4];
    int k, bad_uv = 0;

    particles_init(&s_ps);
    particles_burst(&s_ps, -2, 70, 9, BLOCK_TORCH, 1.0f);
    CHECK_EQ_INT(particles_live(&s_ps), PARTICLES_PER_BREAK);
    CHECK_EQ_INT(count_in(&s_ps, -2.0f + lo, 70.0f, 9.0f + lo, -2.0f + hi,
                          70.0f + MESH_TORCH_HEIGHT, 9.0f + hi), PARTICLES_PER_BREAK);
    blocktex_sub_uv(TILE_TORCH, BLOCKTEX_TORCH_X0, BLOCKTEX_TORCH_Y0, BLOCKTEX_TORCH_X1,
                    BLOCKTEX_TORCH_Y1, in);
    blocktex_sub_uv(TILE_TORCH, 0, 0, BLOCKTEX_TORCH_X1 - BLOCKTEX_TORCH_X0, PARTICLES_TEXELS,
                    size);
    for (k = 0; k < PARTICLES_PER_BREAK; k++)
        bad_uv += !piece_ok(s_ps.p[k].uv, in, size);
    CHECK_EQ_INT(bad_uv, 0);

    /* Air and values that are no block throw nothing. */
    particles_init(&s_ps);
    particles_burst(&s_ps, 0, 60, 0, BLOCK_AIR, 1.0f);
    particles_burst(&s_ps, 0, 60, 0, BLOCK_COUNT, 1.0f);
    particles_burst(&s_ps, 0, 60, 0, -3, 1.0f);
    CHECK_EQ_INT(particles_live(&s_ps), 0);
}

/* With no room left a break takes the oldest bits' places. */
static void test_ring(void)
{
    int k;

    particles_init(&s_ps);
    for (k = 0; k < 5; k++)
        particles_burst(&s_ps, 10 * k, 60, 0, BLOCK_DIRT, 0.0f);
    CHECK_EQ_INT(particles_live(&s_ps), PARTICLES_MAX);
    CHECK_EQ_INT(count_in(&s_ps, 0, 60, 0, 1, 61, 1),
                 PARTICLES_PER_BREAK - (5 * PARTICLES_PER_BREAK - PARTICLES_MAX));
    for (k = 1; k < 5; k++)
        CHECK_EQ_INT(count_in(&s_ps, 10.0f * k, 60, 0, 10.0f * k + 1, 61, 1), PARTICLES_PER_BREAK);
}

/* Bits never last past PARTICLES_LIFE_MAX, and none ends before
 * PARTICLES_LIFE_MIN. */
static void test_life(void)
{
    int s;

    test_world_cleared(&s_w);
    particles_init(&s_ps);
    particles_burst(&s_ps, 0, 60, 0, BLOCK_STONE, 0.0f);
    for (s = 0; s < 17; s++) /* 0.567 s */
        particles_step(&s_ps, &s_w, 1.0f / 30.0f);
    CHECK_EQ_INT(particles_live(&s_ps), PARTICLES_PER_BREAK);
    for (; s < 37; s++) /* 1.233 s */
        particles_step(&s_ps, &s_w, 1.0f / 30.0f);
    CHECK_EQ_INT(particles_live(&s_ps), 0);
}

/* Burst block `block` at (0, y, 0) with long lives and step `steps` times
 * by dt, checking after each step that no bit's middle column is in a
 * solid block at its foot or top. Returns the highest top and the furthest
 * +x edge any bit reached, and the furthest any got from x 0.5 or z 0.5. */
static int fly(int y, int steps, float dt, float *top, float *east, float *far)
{
    int k, s, inside = 0;

    particles_init(&s_ps);
    particles_burst(&s_ps, 0, y, 0, BLOCK_STONE, 0.0f);
    for (k = 0; k < PARTICLES_MAX; k++)
        if (s_ps.p[k].life > 0.0f)
            s_ps.p[k].life = 100.0f;
    *top = *east = *far = -1000.0f;
    for (s = 0; s < steps; s++) {
        particles_step(&s_ps, &s_w, dt);
        for (k = 0; k < PARTICLES_PER_BREAK; k++) {
            const Particle *p = &s_ps.p[k];
            int fx = (int)floorf(p->x), fz = (int)floorf(p->z);

            inside += block_solid(world_get(&s_w, fx, (int)floorf(p->y - R + 1e-3f), fz)) ||
                      block_solid(world_get(&s_w, fx, (int)floorf(p->y + R - 1e-3f), fz));
            *top = p->y + R > *top ? p->y + R : *top;
            *east = p->x + R > *east ? p->x + R : *east;
            *far = fabsf(p->x - 0.5f) > *far ? fabsf(p->x - 0.5f) : *far;
            *far = fabsf(p->z - 0.5f) > *far ? fabsf(p->z - 0.5f) : *far;
        }
    }
    return inside;
}

/* Lying still on the ground: every bit's foot on top of the floor at y 59,
 * not moving. */
static int resting_wrong(void)
{
    int k, bad = 0;

    for (k = 0; k < PARTICLES_PER_BREAK; k++) {
        const Particle *p = &s_ps.p[k];

        bad += fabsf(p->y - (60.0f + R)) > TOL || p->vy != 0.0f || fabsf(p->vx) > 0.01f ||
               fabsf(p->vz) > 0.01f;
    }
    return bad;
}

/* Falling, landing and walls: the bits hop up and out, fall, land on the
 * floor and slide to a stop; a wall and a ceiling stop them; even a long
 * fall in big steps never passes through the floor. */
static void test_fall(void)
{
    float top, east, far;
    int x, y, z;

    test_world_cleared(&s_w);
    for (z = -8; z <= 8; z++)
        for (x = -8; x <= 8; x++)
            world_set(&s_w, x, 59, z, BLOCK_STONE);
    CHECK_EQ_INT(fly(60, 90, 1.0f / 30.0f, &top, &east, &far), 0);
    CHECK_EQ_INT(resting_wrong(), 0);
    CHECK_EQ_INT(top > 61.2f, 1);  /* they hop up out of the block */
    CHECK_EQ_INT(far > 0.8f, 1);   /* and out past its sides */
    CHECK_EQ_INT(far < 4.0f, 1);   /* but not far */
    printf("     (bits: highest top %.2f, furthest %.2f from the middle)\n", top, far);

    /* A wall at x 1, next to the broken block, and a ceiling at y 61 over
     * it and the rest. */
    for (z = -8; z <= 8; z++) {
        for (y = 60; y <= 61; y++)
            world_set(&s_w, 1, y, z, BLOCK_STONE);
        for (x = -8; x <= 0; x++)
            world_set(&s_w, x, 61, z, BLOCK_STONE);
    }
    CHECK_EQ_INT(fly(60, 90, 1.0f / 30.0f, &top, &east, &far), 0);
    CHECK_EQ_INT(top <= 61.0f + TOL, 1);
    CHECK_EQ_INT(east <= 1.0f + TOL, 1);
    CHECK_EQ_INT(east > 1.0f - 0.01f, 1); /* some reached the wall */
    CHECK_EQ_INT(resting_wrong(), 0);

    /* A 20-block fall in tenth-of-a-second steps (main.c's longest). */
    test_world_cleared(&s_w);
    for (z = -8; z <= 8; z++)
        for (x = -8; x <= 8; x++)
            world_set(&s_w, x, 59, z, BLOCK_STONE);
    CHECK_EQ_INT(fly(80, 40, 0.1f, &top, &east, &far), 0);
    CHECK_EQ_INT(resting_wrong(), 0);
}

/* The same breaks and steps always give the same bits. */
static void test_same_every_time(void)
{
    int s;

    test_world_cleared(&s_w);
    particles_init(&s_ps);
    particles_init(&s_ps2);
    for (s = 0; s < 20; s++) {
        if (s % 4 == 0) {
            particles_burst(&s_ps, s, 60, -s, BLOCK_DIRT, 0.1f * s);
            particles_burst(&s_ps2, s, 60, -s, BLOCK_DIRT, 0.1f * s);
        }
        particles_step(&s_ps, &s_w, 1.0f / 30.0f);
        particles_step(&s_ps2, &s_w, 1.0f / 30.0f);
    }
    CHECK_EQ_INT(memcmp(&s_ps, &s_ps2, sizeof s_ps), 0);
}

static float dot3(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void sub3(const MeshVertex *a, const MeshVertex *b, float out[3])
{
    out[0] = a->x - b->x;
    out[1] = a->y - b->y;
    out[2] = a->z - b->z;
}

/* Each live bit: a square PARTICLES_SIZE across round its middle, square
 * to the camera's look, counter-clockwise seen from the camera, its piece
 * upright (bottom-left corner at the piece's bottom left), shaded and lit. */
static void test_build(void)
{
    static const int k_corner[4] = { 0, 1, 2, 5 }; /* (-1,-1) (1,-1) (1,1) (-1,1) */
    Camera cam;
    float f[3];
    int n, q = 0, k, bad_ccw = 0, bad_size = 0, bad_mid = 0, bad_square = 0, bad_uv = 0,
           bad_up = 0, bad_look = 0;

    particles_init(&s_ps);
    particles_burst(&s_ps, 3, 60, -5, BLOCK_GRASS, 0.25f);
    s_ps.p[5].life = 0.0f; /* gone: not drawn */
    memset(&cam, 0, sizeof cam);
    cam.x = 0.5f;
    cam.y = 61.6f;
    cam.z = 2.0f;
    cam.yaw = 0.43f;
    cam.pitch = -0.2f;
    camera_forward(&cam, f);
    n = particles_build(&s_ps, &cam, s_out);
    CHECK_EQ_INT(n, (PARTICLES_PER_BREAK - 1) * MESH_FACE_VERTS);
    for (k = 0; k < PARTICLES_PER_BREAK; k++) {
        const Particle *p = &s_ps.p[k];
        const MeshVertex *v = &s_out[q * MESH_FACE_VERTS];
        float e1[3], e2[3], e3[3], e4[3], nrm[3], eye[3], mx, my, mz;
        int c;

        if (p->life <= 0.0f)
            continue;
        q++;
        sub3(&v[1], &v[0], e1);
        sub3(&v[2], &v[1], e2);
        sub3(&v[4], &v[3], e3);
        sub3(&v[5], &v[4], e4);
        nrm[0] = e1[1] * e2[2] - e1[2] * e2[1];
        nrm[1] = e1[2] * e2[0] - e1[0] * e2[2];
        nrm[2] = e1[0] * e2[1] - e1[1] * e2[0];
        eye[0] = cam.x - v[0].x;
        eye[1] = cam.y - v[0].y;
        eye[2] = cam.z - v[0].z;
        bad_ccw += dot3(nrm, eye) <= 0.0f ||
                   (e3[1] * e4[2] - e3[2] * e4[1]) * eye[0] +
                   (e3[2] * e4[0] - e3[0] * e4[2]) * eye[1] +
                   (e3[0] * e4[1] - e3[1] * e4[0]) * eye[2] <= 0.0f;
        bad_size += fabsf(sqrtf(dot3(e1, e1)) - PARTICLES_SIZE) > TOL ||
                    fabsf(sqrtf(dot3(e2, e2)) - PARTICLES_SIZE) > TOL;
        bad_square += fabsf(dot3(e1, e2)) > TOL;
        bad_look += fabsf(dot3(e1, f)) > TOL || fabsf(dot3(e2, f)) > TOL;
        bad_up += e2[1] <= 0.0f; /* (1,-1) to (1,1) goes up */
        mx = (v[0].x + v[2].x) * 0.5f;
        my = (v[0].y + v[2].y) * 0.5f;
        mz = (v[0].z + v[2].z) * 0.5f;
        bad_mid += fabsf(mx - p->x) > TOL || fabsf(my - p->y) > TOL || fabsf(mz - p->z) > TOL;
        for (c = 0; c < 4; c++) {
            const MeshVertex *o = &v[k_corner[c]];
            float u = (c == 0 || c == 3) ? p->uv[0] : p->uv[2];
            float w = c < 2 ? p->uv[1] : p->uv[3];

            bad_uv += o->u != u || o->v != w || o->r != PARTICLES_SHADE ||
                      o->g != PARTICLES_SHADE || o->b != PARTICLES_SHADE || o->t != 0.25f;
        }
        bad_uv += memcmp(&v[3], &v[0], sizeof *v) != 0 || memcmp(&v[4], &v[2], sizeof *v) != 0;
    }
    CHECK_EQ_INT(bad_ccw, 0);
    CHECK_EQ_INT(bad_size, 0);
    CHECK_EQ_INT(bad_square, 0);
    CHECK_EQ_INT(bad_look, 0);
    CHECK_EQ_INT(bad_up, 0);
    CHECK_EQ_INT(bad_mid, 0);
    CHECK_EQ_INT(bad_uv, 0);
    particles_init(&s_ps);
    CHECK_EQ_INT(particles_build(&s_ps, &cam, s_out), 0);
}

void test_particles(void)
{
    test_burst();
    test_torch_burst();
    test_ring();
    test_life();
    test_fall();
    test_same_every_time();
    test_build();
}
