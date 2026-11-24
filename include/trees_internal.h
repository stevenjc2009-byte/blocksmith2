#ifndef TREES_INTERNAL_H
#define TREES_INTERNAL_H

/* Shared by source/trees.c, trees_shape.c and trees_plants.c. */

#include "trees.h"

/* Where a column's TerrainColumn comes from: the 16 x 16 columns of the chunk
 * at (x0, z0) when cols is not NULL (much quicker), else terrain_column(). */
typedef struct {
    uint32_t seed;
    const TerrainColumn *cols; /* [z * CHUNK_SIZE + x], or NULL */
    int x0, z0;                /* world column of cols[0] */
} TreeCols;

TerrainColumn tree_col(const TreeCols *s, int x, int z);

/* The acacia's lean (one step on x or z), how many blocks it goes straight up
 * before it, and the cactus's height: trees.c needs them to check the ground
 * round the trunk. */
void tree_acacia_lean(const Tree *t, int *dx, int *dz);
int tree_acacia_straight(const Tree *t);
int tree_cactus_height(const Tree *t);

/* Plants in the canvas, after the trees (trees_plants.c). */
void trees_plants(const StructCanvas *cv, const TreeCols *s);

#endif
