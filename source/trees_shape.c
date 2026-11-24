#include <stdlib.h>

#include "block.h"
#include "terrain_noise.h"
#include "trees.h"

/* The blocks of each kind of tree (see trees.h). Heights are chosen from the
 * tree's salt; everything here is a pure function of the Tree, and never
 * reads the world. Every block lies within TREES_REACH of the trunk's
 * column (trees_shape_reach() in the tests checks it). */

typedef struct {
    TreeSink put;
    void *ctx;
} Emit;

/* Leaves with the corners left out how: */
enum {
    CORNERS_ALL,     /* a full square */
    CORNERS_SOME,    /* each corner kept by a coin toss */
    CORNERS_NONE,    /* corners left out: a rounded disc */
    CORNERS_DIAMOND, /* only cells with |dx| + |dz| <= r + r / 2 */
};

/* A layer of leaves at height y: the square of radius r round the columns
 * x0 .. x1, z0 .. z1 (the trunk's), corners as `mode` says. */
static void leaf_layer(const Emit *e, int x0, int z0, int x1, int z1, int y, int r, int mode,
                       uint32_t salt, int block)
{
    int x, z;

    for (z = z0 - r; z <= z1 + r; z++)
        for (x = x0 - r; x <= x1 + r; x++) {
            int ox = x < x0 ? x0 - x : x > x1 ? x - x1 : 0; /* how far out of the trunk's box */
            int oz = z < z0 ? z0 - z : z > z1 ? z - z1 : 0;
            bool corner = ox == r && oz == r && r > 0;

            if (mode == CORNERS_DIAMOND && ox + oz > r + r / 2)
                continue;
            if (corner && mode == CORNERS_NONE)
                continue;
            if (corner && mode == CORNERS_SOME && (noise_hash(salt, x * 31 + y, z) & 1))
                continue;
            e->put(e->ctx, x, y, z, block);
        }
}

static void logs(const Emit *e, int x0, int z0, int x1, int z1, int y0, int y1, int block)
{
    int x, y, z;

    for (y = y0; y <= y1; y++)
        for (z = z0; z <= z1; z++)
            for (x = x0; x <= x1; x++)
                e->put(e->ctx, x, y, z, block);
}

/* The oak shape: a trunk th tall, two layers of radius 2 round its top end,
 * one of radius 1, and a cross on top. */
static void broadleaf(const Emit *e, const Tree *t, int th, int log, int leaves, int pass)
{
    int ty = t->y + th;

    if (pass == 1) {
        logs(e, t->x, t->z, t->x, t->z, t->y + 1, ty, log);
        return;
    }
    leaf_layer(e, t->x, t->z, t->x, t->z, ty - 2, 2, CORNERS_SOME, t->salt, leaves);
    leaf_layer(e, t->x, t->z, t->x, t->z, ty - 1, 2, CORNERS_SOME, t->salt + 1, leaves);
    leaf_layer(e, t->x, t->z, t->x, t->z, ty, 1, CORNERS_SOME, t->salt + 2, leaves);
    leaf_layer(e, t->x, t->z, t->x, t->z, ty + 1, 1, CORNERS_NONE, t->salt + 3, leaves);
}

/* The spruce: a tall trunk, leaves in a cone from just over its top down to
 * 2 blocks off the ground, wider every other layer. */
static void spruce(const Emit *e, const Tree *t, int pass)
{
    static const unsigned char k_radius[10] = { 0, 1, 2, 1, 2, 3, 2, 3, 3, 3 };
    int th = 6 + (int)(t->salt % 4), k, y;

    if (pass == 1) {
        logs(e, t->x, t->z, t->x, t->z, t->y + 1, t->y + th, BLOCK_SPRUCE_LOG);
        return;
    }
    for (k = 0, y = t->y + th + 1; y >= t->y + 3 && k < 10; k++, y--)
        leaf_layer(e, t->x, t->z, t->x, t->z, y, k_radius[k], CORNERS_DIAMOND, t->salt, BLOCK_SPRUCE_LEAVES);
}

/* The 2 x 2 trunk kinds' dirt: each column's ground and any gap up to the
 * highest. */
static void dirt_2x2(const Emit *e, const Tree *t)
{
    int i, y;

    for (i = 0; i < 4; i++) {
        int x = t->x + (i & 1), z = t->z + (i >> 1);

        e->put(e->ctx, x, t->gy[i], z, BLOCK_DIRT);
        for (y = t->gy[i] + 1; y <= t->y; y++)
            e->put(e->ctx, x, y, z, BLOCK_DIRT);
    }
}

/* The dark oak: a thick 2 x 2 trunk, a wide heavy canopy. */
static void dark_oak(const Emit *e, const Tree *t, int pass)
{
    int th = 6 + (int)(t->salt % 3), ty = t->y + th;

    if (pass == 1) {
        logs(e, t->x, t->z, t->x + 1, t->z + 1, t->y + 1, ty, BLOCK_DARK_OAK_LOG);
        return;
    }
    leaf_layer(e, t->x, t->z, t->x + 1, t->z + 1, ty - 1, 2, CORNERS_SOME, t->salt, BLOCK_DARK_OAK_LEAVES);
    leaf_layer(e, t->x, t->z, t->x + 1, t->z + 1, ty, 3, CORNERS_SOME, t->salt + 1, BLOCK_DARK_OAK_LEAVES);
    leaf_layer(e, t->x, t->z, t->x + 1, t->z + 1, ty + 1, 2, CORNERS_SOME, t->salt + 2, BLOCK_DARK_OAK_LEAVES);
    leaf_layer(e, t->x, t->z, t->x + 1, t->z + 1, ty + 2, 1, CORNERS_NONE, t->salt + 3, BLOCK_DARK_OAK_LEAVES);
}

/* The giant jungle tree: a very tall 2 x 2 trunk with a crown on top. */
static void jungle_giant(const Emit *e, const Tree *t, int pass)
{
    int th = 10 + (int)(t->salt % 5), ty = t->y + th;

    if (pass == 1) {
        logs(e, t->x, t->z, t->x + 1, t->z + 1, t->y + 1, ty, BLOCK_JUNGLE_LOG);
        return;
    }
    leaf_layer(e, t->x, t->z, t->x + 1, t->z + 1, ty - 1, 2, CORNERS_SOME, t->salt, BLOCK_JUNGLE_LEAVES);
    leaf_layer(e, t->x, t->z, t->x + 1, t->z + 1, ty, 3, CORNERS_SOME, t->salt + 1, BLOCK_JUNGLE_LEAVES);
    leaf_layer(e, t->x, t->z, t->x + 1, t->z + 1, ty + 1, 2, CORNERS_SOME, t->salt + 2, BLOCK_JUNGLE_LEAVES);
    leaf_layer(e, t->x, t->z, t->x + 1, t->z + 1, ty + 2, 1, CORNERS_NONE, t->salt + 3, BLOCK_JUNGLE_LEAVES);
}

/* The jungle bush: one log with a ball of oak leaves round it. */
static void jungle_bush(const Emit *e, const Tree *t, int pass)
{
    if (pass == 1) {
        e->put(e->ctx, t->x, t->y + 1, t->z, BLOCK_JUNGLE_LOG);
        return;
    }
    leaf_layer(e, t->x, t->z, t->x, t->z, t->y + 1, 2, CORNERS_NONE, t->salt, BLOCK_OAK_LEAVES);
    leaf_layer(e, t->x, t->z, t->x, t->z, t->y + 2, 1, CORNERS_ALL, t->salt + 1, BLOCK_OAK_LEAVES);
    leaf_layer(e, t->x, t->z, t->x, t->z, t->y + 3, 0, CORNERS_ALL, t->salt + 2, BLOCK_OAK_LEAVES);
}

/* The way the acacia's trunk leans: one step on x or z. */
void tree_acacia_lean(const Tree *t, int *dx, int *dz)
{
    static const signed char k_step[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    int d = (int)((t->salt >> 8) & 3);

    *dx = k_step[d][0];
    *dz = k_step[d][1];
}

/* How many cells the acacia's trunk goes straight up before it leans. */
int tree_acacia_straight(const Tree *t)
{
    return 3 + (int)(t->salt % 2);
}

/* The acacia: straight up, then one step sideways and up two more; a flat
 * crown over the leaning part. */
static void acacia(const Emit *e, const Tree *t, int pass)
{
    int dx, dz, s = tree_acacia_straight(t), ty = t->y + s + 2, x, z;

    tree_acacia_lean(t, &dx, &dz);
    x = t->x + dx;
    z = t->z + dz;
    if (pass == 1) {
        logs(e, t->x, t->z, t->x, t->z, t->y + 1, t->y + s, BLOCK_ACACIA_LOG);
        logs(e, x, z, x, z, t->y + s, ty, BLOCK_ACACIA_LOG);
        return;
    }
    leaf_layer(e, x, z, x, z, ty, 3, CORNERS_NONE, t->salt, BLOCK_ACACIA_LEAVES);
    leaf_layer(e, x, z, x, z, ty + 1, 2, CORNERS_NONE, t->salt + 1, BLOCK_ACACIA_LEAVES);
}

/* The cactus is 1 to 3 blocks tall. */
int tree_cactus_height(const Tree *t)
{
    return 1 + (int)(t->salt % 3);
}

void tree_shape(const Tree *t, int pass, TreeSink put, void *ctx)
{
    Emit e;

    e.put = put;
    e.ctx = ctx;
    if (pass == 0) { /* the ground under the trunk becomes dirt (not under a cactus) */
        if (t->kind == TREE_DARK_OAK || t->kind == TREE_JUNGLE_GIANT)
            dirt_2x2(&e, t);
        else if (t->kind != TREE_CACTUS)
            put(ctx, t->x, t->y, t->z, BLOCK_DIRT);
        return;
    }
    switch (t->kind) {
    case TREE_OAK:
        broadleaf(&e, t, 4 + (int)(t->salt % 3), BLOCK_LOG, BLOCK_OAK_LEAVES, pass);
        break;
    case TREE_BIRCH:
        broadleaf(&e, t, 5 + (int)(t->salt % 3), BLOCK_BIRCH_LOG, BLOCK_BIRCH_LEAVES, pass);
        break;
    case TREE_JUNGLE:
        broadleaf(&e, t, 7 + (int)(t->salt % 4), BLOCK_JUNGLE_LOG, BLOCK_JUNGLE_LEAVES, pass);
        break;
    case TREE_SPRUCE:
        spruce(&e, t, pass);
        break;
    case TREE_DARK_OAK:
        dark_oak(&e, t, pass);
        break;
    case TREE_JUNGLE_GIANT:
        jungle_giant(&e, t, pass);
        break;
    case TREE_JUNGLE_BUSH:
        jungle_bush(&e, t, pass);
        break;
    case TREE_ACACIA:
        acacia(&e, t, pass);
        break;
    case TREE_CACTUS:
        if (pass == 1)
            logs(&e, t->x, t->z, t->x, t->z, t->y + 1, t->y + tree_cactus_height(t), BLOCK_CACTUS);
        break;
    default:
        break;
    }
}
