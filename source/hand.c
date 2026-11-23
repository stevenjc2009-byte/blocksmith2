#include <3ds.h>
#include <math.h>

#include "block.h"
#include "gpu.h"
#include "hand.h"
#include "held.h"
#include "light.h"
#include "meshes.h"
#include "particles.h"
#include "place.h"
#include "raycast.h"

static Particles s_bits;
static Held s_held;
static bool s_started;
/* The bits' and the held block's vertices, in linear memory for the GPU.
 * Written each frame before their draw: the last frame's gpu_frame_end()
 * has waited for the GPU, so it is no longer reading them. Made on first
 * use; with no memory for them nothing is drawn but breaking still works. */
static MeshVertex *s_bit_verts;  /* PARTICLES_VERTS_MAX */
static MeshVertex *s_held_verts; /* MESH_BLOCK_VERTS */
/* The held block's torch light, and what it was worked out for. */
static float s_lit;
static int s_lit_at[3];
static unsigned s_lit_torch_edits;
static float s_lit_age = HAND_LIGHT_REFRESH; /* past the refresh: work it out */

static void start(void)
{
    if (s_started)
        return;
    particles_init(&s_bits);
    held_init(&s_held);
    s_started = true;
}

/* The block under the crosshair, if one is in reach. */
static bool looked_at(const World *w, const Camera *cam, RayHit *hit)
{
    float f[3];

    camera_forward(cam, f);
    return raycast(w, cam->x, cam->y, cam->z, f[0], f[1], f[2], RAY_REACH, hit);
}

bool hand_break(World *w, const Camera *cam)
{
    RayHit hit;
    int b, above;
    float t;

    start();
    held_swing(&s_held);
    if (!looked_at(w, cam, &hit))
        return true;
    b = world_get(w, hit.x, hit.y, hit.z);
    above = world_get(w, hit.x, hit.y + 1, hit.z);
    if (!world_break(w, hit.x, hit.y, hit.z))
        return true;
    t = light_brightness(light_world_level(w, hit.x, hit.y, hit.z));
    particles_burst(&s_bits, hit.x, hit.y, hit.z, b, b == BLOCK_TORCH ? 1.0f : t);
    if (above == BLOCK_TORCH && world_get(w, hit.x, hit.y + 1, hit.z) != BLOCK_TORCH)
        particles_burst(&s_bits, hit.x, hit.y + 1, hit.z, BLOCK_TORCH, 1.0f);
    s_lit_age = HAND_LIGHT_REFRESH;
    return meshes_rebuild_around(w, hit.x, hit.y, hit.z);
}

bool hand_place(World *w, const Camera *cam, const Player *player, int b)
{
    RayHit hit;
    int x, y, z;

    start();
    held_swing(&s_held);
    if (!looked_at(w, cam, &hit) || !place_block(w, player, &hit, b, &x, &y, &z))
        return true;
    s_lit_age = HAND_LIGHT_REFRESH;
    return meshes_rebuild_around(w, x, y, z);
}

void hand_step(const World *w, float dt)
{
    start();
    held_step(&s_held, dt);
    particles_step(&s_bits, w, dt);
    s_lit_age += dt;
}

/* The held block's torch light: the light of the eye's cell. */
static float eye_light(const World *w, const Camera *cam)
{
    int x = (int)floorf(cam->x), y = (int)floorf(cam->y), z = (int)floorf(cam->z);

    if (s_lit_age >= HAND_LIGHT_REFRESH || w->torch_edits != s_lit_torch_edits ||
        x != s_lit_at[0] || y != s_lit_at[1] || z != s_lit_at[2]) {
        s_lit = light_brightness(light_world_level(w, x, y, z));
        s_lit_at[0] = x;
        s_lit_at[1] = y;
        s_lit_at[2] = z;
        s_lit_torch_edits = w->torch_edits;
        s_lit_age = 0.0f;
    }
    return s_lit;
}

void hand_draw_bits(const Camera *cam, const Mat4 *mvp)
{
    int n;

    start();
    if (!s_bit_verts)
        s_bit_verts = linearAlloc(PARTICLES_VERTS_MAX * sizeof *s_bit_verts);
    if (s_bit_verts) {
        n = particles_build(&s_bits, cam, s_bit_verts);
        if (n > 0) { /* only the vertices written this frame */
            GSPGPU_FlushDataCache(s_bit_verts, n * sizeof *s_bit_verts);
            gpu_draw(s_bit_verts, n, mvp);
        }
    }
}

void hand_draw_held(const World *w, const Camera *cam, const Mat4 *proj, int held)
{
    Mat4 place, m;
    int n;

    start();
    if (!s_held_verts)
        s_held_verts = linearAlloc(MESH_BLOCK_VERTS * sizeof *s_held_verts);
    if (s_held_verts) {
        n = held_build(held, eye_light(w, cam), s_held_verts);
        if (n > 0) {
            GSPGPU_FlushDataCache(s_held_verts, n * sizeof *s_held_verts);
            held_place(&place, &s_held);
            mat4_mul(&m, proj, &place);
            gpu_set_fog(0.0f, 0.0f, 0.0f, 0.0f); /* its place is not in the world */
            gpu_draw_front(s_held_verts, n, &m);
        }
    }
}

void hand_exit(void)
{
    if (s_bit_verts)
        linearFree(s_bit_verts);
    if (s_held_verts)
        linearFree(s_held_verts);
    s_bit_verts = s_held_verts = NULL;
}
