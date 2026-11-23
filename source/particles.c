#include <math.h>
#include <string.h>

#include "block.h"
#include "blocktex.h"
#include "particles.h"

#define RNG_START 0x2F6B3A91u
#define STEP_MAX 0.5f   /* blocks a bit may move in one go, so none passes through a block */
#define EPS 0.001f

/* The next number of the random sequence, 0 to just under 1 (xorshift32). */
static float rnd(Particles *ps)
{
    uint32_t r = ps->rng;

    r ^= r << 13;
    r ^= r >> 17;
    r ^= r << 5;
    ps->rng = r;
    return (float)(r >> 8) / 16777216.0f;
}

void particles_init(Particles *ps)
{
    memset(ps, 0, sizeof *ps);
    ps->rng = RNG_START;
}

void particles_burst(Particles *ps, int x, int y, int z, int block, float t)
{
    float x0 = 0.0f, w = 1.0f, h = 1.0f; /* the box the bits fill, in the cell */
    int tile, tx0 = 0, ty0 = 0, tx1 = BLOCKTEX_TILE, ty1 = BLOCKTEX_TILE, pw, ph, i, j, k;

    if (block <= BLOCK_AIR || block >= BLOCK_COUNT)
        return;
    if (block == BLOCK_TORCH) {
        x0 = 0.5f - MESH_TORCH_HALF;
        w = 2.0f * MESH_TORCH_HALF;
        h = MESH_TORCH_HEIGHT;
        tile = TILE_TORCH;
        tx0 = BLOCKTEX_TORCH_X0;
        tx1 = BLOCKTEX_TORCH_X1;
        ty0 = BLOCKTEX_TORCH_Y0;
        ty1 = BLOCKTEX_TORCH_Y1;
    } else {
        tile = block_face_tile(block, FACE_POS_X);
    }
    pw = tx1 - tx0 < PARTICLES_TEXELS ? tx1 - tx0 : PARTICLES_TEXELS;
    ph = ty1 - ty0 < PARTICLES_TEXELS ? ty1 - ty0 : PARTICLES_TEXELS;
    for (j = 0; j < PARTICLES_GRID; j++)
        for (k = 0; k < PARTICLES_GRID; k++)
            for (i = 0; i < PARTICLES_GRID; i++) {
                Particle *p = &ps->p[ps->next];
                float fx = ((float)i + 0.5f) / PARTICLES_GRID - 0.5f; /* from the middle */
                float fy = ((float)j + 0.5f) / PARTICLES_GRID - 0.5f;
                float fz = ((float)k + 0.5f) / PARTICLES_GRID - 0.5f;
                int px, py;

                p->x = (float)x + x0 + (fx + 0.5f) * w;
                p->y = (float)y + (fy + 0.5f) * h;
                p->z = (float)z + x0 + (fz + 0.5f) * w;
                p->vx = fx * w * PARTICLES_SPREAD + (rnd(ps) * 2.0f - 1.0f) * PARTICLES_JITTER;
                p->vy = fy * h * PARTICLES_SPREAD + (rnd(ps) * 2.0f - 1.0f) * PARTICLES_JITTER +
                        PARTICLES_HOP_MIN + rnd(ps) * (PARTICLES_HOP_MAX - PARTICLES_HOP_MIN);
                p->vz = fz * w * PARTICLES_SPREAD + (rnd(ps) * 2.0f - 1.0f) * PARTICLES_JITTER;
                px = tx0 + (int)(rnd(ps) * (float)(tx1 - tx0 - pw + 1));
                py = ty0 + (int)(rnd(ps) * (float)(ty1 - ty0 - ph + 1));
                blocktex_sub_uv(tile, px, py, px + pw, py + ph, p->uv);
                p->t = t;
                p->life = PARTICLES_LIFE_MIN + rnd(ps) * (PARTICLES_LIFE_MAX - PARTICLES_LIFE_MIN);
                ps->next = (ps->next + 1) % PARTICLES_MAX;
            }
}

static int solid_at(const World *w, float x, float y, float z)
{
    return block_solid(world_get(w, (int)floorf(x), (int)floorf(y), (int)floorf(z)));
}

/* A solid block level with the foot or the top of a bit at height y, at
 * (x, z). */
static int blocked(const World *w, float x, float y, float z)
{
    const float r = PARTICLES_SIZE * 0.5f;

    return solid_at(w, x, y - r + EPS, z) || solid_at(w, x, y + r - EPS, z);
}

/* One short move of bit p: x, then z, then y, each stopped by a solid
 * block: its side by the square's edge, a ceiling by its top, the ground by
 * its foot (the bottom of its square). */
static void move(Particle *p, const World *w, float dt)
{
    const float r = PARTICLES_SIZE * 0.5f;
    float nx, nz, ny;

    p->vy -= PARTICLES_GRAVITY * dt;
    nx = p->x + p->vx * dt;
    if (blocked(w, nx + (p->vx > 0.0f ? r : -r), p->y, p->z))
        p->vx = 0.0f;
    else
        p->x = nx;
    nz = p->z + p->vz * dt;
    if (blocked(w, p->x, p->y, nz + (p->vz > 0.0f ? r : -r)))
        p->vz = 0.0f;
    else
        p->z = nz;
    ny = p->y + p->vy * dt;
    if (p->vy > 0.0f) {
        if (solid_at(w, p->x, ny + r, p->z)) /* its top hit a ceiling */
            p->vy = 0.0f;
        else
            p->y = ny;
    } else if (solid_at(w, p->x, ny - r, p->z)) {
        /* Landed on the top of a block: slide to a stop. */
        float keep = 1.0f - PARTICLES_GROUND_STOP * dt;

        p->y = floorf(ny - r) + 1.0f + r;
        keep = keep < 0.0f ? 0.0f : keep;
        p->vx *= keep;
        p->vz *= keep;
        p->vy = 0.0f;
    } else {
        p->y = ny;
    }
}

void particles_step(Particles *ps, const World *w, float dt)
{
    int k;

    for (k = 0; k < PARTICLES_MAX; k++) {
        Particle *p = &ps->p[k];
        float fast, sub;
        int n, s;

        if (p->life <= 0.0f)
            continue;
        p->life -= dt;
        if (p->life <= 0.0f) {
            p->life = 0.0f;
            continue;
        }
        /* Short enough moves that none passes through a block. */
        fast = fabsf(p->vx);
        fast = fabsf(p->vz) > fast ? fabsf(p->vz) : fast;
        fast = fabsf(p->vy) + PARTICLES_GRAVITY * dt > fast ? fabsf(p->vy) + PARTICLES_GRAVITY * dt : fast;
        n = (int)ceilf(fast * dt / STEP_MAX);
        n = n < 1 ? 1 : n;
        sub = dt / (float)n;
        for (s = 0; s < n; s++)
            move(p, w, sub);
    }
}

int particles_live(const Particles *ps)
{
    int k, n = 0;

    for (k = 0; k < PARTICLES_MAX; k++)
        n += ps->p[k].life > 0.0f;
    return n;
}

int particles_build(const Particles *ps, const Camera *cam, MeshVertex out[PARTICLES_VERTS_MAX])
{
    static const signed char k_corner[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
    static const unsigned char k_order[MESH_FACE_VERTS] = { 0, 1, 2, 0, 2, 3 };
    const float s = PARTICLES_SIZE * 0.5f;
    float f[3], rt[3], up[3];
    int k, n = 0;

    camera_forward(cam, f);
    rt[0] = cosf(cam->yaw);
    rt[1] = 0.0f;
    rt[2] = sinf(cam->yaw);
    up[0] = rt[1] * f[2] - rt[2] * f[1];
    up[1] = rt[2] * f[0] - rt[0] * f[2];
    up[2] = rt[0] * f[1] - rt[1] * f[0];
    for (k = 0; k < PARTICLES_MAX; k++) {
        const Particle *p = &ps->p[k];
        int v;

        if (p->life <= 0.0f)
            continue;
        for (v = 0; v < MESH_FACE_VERTS; v++) {
            const signed char *c = k_corner[k_order[v]];
            MeshVertex *o = &out[n + v];

            o->x = p->x + (c[0] * rt[0] + c[1] * up[0]) * s;
            o->y = p->y + (c[0] * rt[1] + c[1] * up[1]) * s;
            o->z = p->z + (c[0] * rt[2] + c[1] * up[2]) * s;
            o->u = c[0] < 0 ? p->uv[0] : p->uv[2];
            o->v = c[1] < 0 ? p->uv[1] : p->uv[3];
            o->r = o->g = o->b = PARTICLES_SHADE;
            o->t = p->t;
        }
        n += MESH_FACE_VERTS;
    }
    return n;
}
