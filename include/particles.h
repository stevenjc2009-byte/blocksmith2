#ifndef PARTICLES_H
#define PARTICLES_H

#include <stdint.h>

#include "camera.h"
#include "mesh.h"
#include "world.h"

/* Break particles (roadmap row 3-05, kickoff I-7): when a block breaks,
 * small bits of it fly out, fall, land on the ground and are gone a moment
 * later. Plain C, no 3DS calls, so the PC tests run it; hand.c puts them on
 * the screen.
 *
 * A break throws PARTICLES_PER_BREAK bits, one from each cell of a
 * PARTICLES_GRID x PARTICLES_GRID x PARTICLES_GRID grid filling the block
 * (a torch: filling its thin stick, mesh.h). Each starts at its cell's
 * middle and flies out from the block's middle, PARTICLES_SPREAD blocks a
 * second for each block it starts from the middle, plus up to
 * PARTICLES_JITTER either way on each axis at random and a hop up of
 * PARTICLES_HOP_MIN to PARTICLES_HOP_MAX. It falls at PARTICLES_GRAVITY,
 * stops against the side or bottom of a solid block, lands on the top of
 * one and slides to a stop there, and lives PARTICLES_LIFE_MIN to
 * PARTICLES_LIFE_MAX seconds. Each shows a PARTICLES_TEXELS square piece,
 * picked at random, of the block's side tile (a torch: of its stick), on a
 * square PARTICLES_SIZE blocks across that always faces the camera, with
 * the torch light of the cell it broke from. The randomness is a fixed
 * sequence, so the same breaks always throw the same bits.
 *
 * There is room for PARTICLES_MAX bits at once; a break with no room left
 * takes the place of the oldest. */

#define PARTICLES_GRID 3
#define PARTICLES_PER_BREAK (PARTICLES_GRID * PARTICLES_GRID * PARTICLES_GRID)
#define PARTICLES_MAX 128
#define PARTICLES_SPREAD 3.0f    /* blocks a second, per block from the middle */
#define PARTICLES_JITTER 0.4f    /* blocks a second, either way */
#define PARTICLES_HOP_MIN 1.5f   /* blocks a second up */
#define PARTICLES_HOP_MAX 3.0f
#define PARTICLES_GRAVITY 14.0f  /* blocks a second, each second */
#define PARTICLES_GROUND_STOP 8.0f /* on the ground: this share of the speed lost a second */
#define PARTICLES_LIFE_MIN 0.6f  /* seconds */
#define PARTICLES_LIFE_MAX 1.2f
#define PARTICLES_SIZE 0.2f      /* blocks across */
#define PARTICLES_TEXELS 4       /* the piece of the tile shown, texels across */
#define PARTICLES_SHADE 0.9f     /* brightness, like a block's side */
#define PARTICLES_VERTS_MAX (PARTICLES_MAX * MESH_FACE_VERTS)

typedef struct {
    float x, y, z;    /* the middle of its square */
    float vx, vy, vz; /* blocks a second */
    float uv[4];      /* the piece it shows (blocktex_tile_uv() order) */
    float t;          /* torch light where it broke from (mesh.h) */
    float life;       /* seconds left; 0 or less: no bit here */
} Particle;

typedef struct {
    Particle p[PARTICLES_MAX];
    uint32_t rng;     /* the random sequence */
    int next;         /* where the next bit goes: the oldest */
} Particles;

/* No bits, the random sequence at its start. */
void particles_init(Particles *ps);

/* Block `block` (BLOCK_*) at (x, y, z) has just broken: throw its bits,
 * each with torch light t. Nothing for air or a value that is no block. */
void particles_burst(Particles *ps, int x, int y, int z, int block, float t);

/* Move every bit on by dt seconds against the blocks of w, and end the ones
 * whose time is up. */
void particles_step(Particles *ps, const World *w, float dt);

/* How many bits there are now. */
int particles_live(const Particles *ps);

/* Every bit as a square facing the camera (two triangles,
 * counter-clockwise seen from the camera), PARTICLES_SHADE brightness.
 * Returns the number of vertices written: MESH_FACE_VERTS a bit. */
int particles_build(const Particles *ps, const Camera *cam,
                    MeshVertex out[PARTICLES_VERTS_MAX]);

#endif
