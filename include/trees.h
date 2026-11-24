#ifndef TREES_H
#define TREES_H

#include <stdbool.h>
#include <stdint.h>

#include "chunk.h"
#include "structure.h"
#include "terrain.h"

/* Trees and plants (roadmap row 4-03). Pure C, whole numbers only, so the
 * PC tests and the 3DS grow exactly the same ones from the same seed.
 *
 * What grows where follows Minecraft 1.17, by the biome of the trunk's
 * column: plains rare oaks; forest oak and birch; birch forest birch; dark
 * forest dark oak (2 x 2 trunk, wide canopy) and some oak; taiga and snowy
 * taiga spruce; snowy plains very rare spruce; jungle tall jungle trees (some
 * 2 x 2) and bushes; savanna rare acacia; swamp sparse oak; desert cactus.
 * Nothing on beaches, rivers, the sea or its ice, nor on bare mountain
 * stone and snow (a mountain with grass keeps a few spruce and oak).
 * Short grass, ferns, flowers and dead bushes grow on the ground between.
 *
 * A tree is a pure function of the seed and its trunk's column, so chunks
 * made in any order agree, canopies across a chunk border included:
 * - Candidate trunks sit on a jittered grid, one per TREES_CELL x TREES_CELL
 *   columns. A candidate is a tree when its column's biome and ground allow
 *   one (a dice roll against the biome's density, raised and lowered by a
 *   slow grove map), and when no other such candidate within
 *   TREES_MIN_DIST columns (square distance) has a higher priority.
 * - A chunk looks at every candidate whose tree can reach it (TREES_REACH
 *   columns) and writes only the blocks inside itself, in a fixed global
 *   order (cells row by row): first the dirt under every trunk, then every
 *   log, then every leaf. Logs and leaves only ever fill air, so a canopy
 *   never replaces a solid block, and where canopies meet the first tree in
 *   the order keeps the cell.
 * - No tree stands where a village or ruin is planned (the plan box or
 *   footprint grown by TREES_REACH) nor within TREES_REACH of the spawn
 *   column, so no leaf ever lands in a structure (structures are applied
 *   after, and win anyway) and the spawn is never under a canopy.
 * - Plants are placed last, one dice roll per column, on bare grass (sand
 *   for the desert ones) with air above, and never within one column of a
 *   village box or ruin footprint. */

#define TREES_CELL 4      /* candidate grid spacing */
#define TREES_MIN_DIST 3  /* no two trees closer than this (square distance) */
#define TREES_REACH 6     /* a tree's blocks lie at most this far from its column on x and z */

typedef enum {
    TREE_NONE,
    TREE_OAK,
    TREE_BIRCH,
    TREE_SPRUCE,
    TREE_DARK_OAK,    /* 2 x 2 trunk */
    TREE_JUNGLE,      /* tall, 1 x 1 trunk */
    TREE_JUNGLE_GIANT, /* 2 x 2 trunk */
    TREE_JUNGLE_BUSH, /* one log and a ball of oak leaves */
    TREE_ACACIA,
    TREE_CACTUS,
    TREE_KIND_COUNT
} TreeKind;

typedef struct {
    int kind;      /* TREE_*; TREE_NONE: no tree here */
    int x, z;      /* the trunk's column (the north-west one of four for the 2 x 2 kinds) */
    int y;         /* the highest ground block's y under the trunk */
    int gy[4];     /* the ground block's y under each trunk column, [iz * 2 + ix] (1 x 1: [0]) */
    uint32_t salt; /* decides the height and the leaf gaps */
    uint32_t prio; /* which of two candidates too close together stays */
} Tree;

/* Every tree whose trunk column lies in x0 .. x0 + w - 1, z0 .. z0 + d - 1,
 * at most max of them, in the order they are written (cells row by row);
 * returns how many. */
int trees_in_area(uint32_t seed, int x0, int z0, int w, int d, Tree *out, int max);

/* Grow trees, then plants, in the canvas, over the bare ground of terrain.h
 * (a chunk's blocks or any larger box). */
void trees_apply_canvas(const StructCanvas *cv, uint32_t seed);

/* The same for chunk (cx, cz) after terrain_fill_chunk_columns() made it;
 * cols are its columns (terrain_fill_chunk_columns()), which saves finding them again. */
void trees_apply(Chunk *c, uint32_t seed, int cx, int cz, const TerrainColumn cols[CHUNK_SIZE][CHUNK_SIZE]);

/* Is column (x, z) within `grow` columns of the spawn column, or of a
 * planned village's box or ruin's footprint? Trees use TREES_REACH, plants 1. */
bool trees_blocked(uint32_t seed, int x, int z, int grow);

/* The BLOCK_* of the plant that a column of this biome with this ground block
 * grows for dice roll `roll` (0 to 255), or BLOCK_AIR for none. */
int plant_for(int biome, int ground, unsigned roll);

/* Where a tree's blocks go: pass 0 the dirt under the trunk, pass 1 the logs
 * (and cactus), pass 2 the leaves. put() decides what it may overwrite:
 * tree_shape() never reads the world. */
typedef void (*TreeSink)(void *ctx, int x, int y, int z, int block);
void tree_shape(const Tree *t, int pass, TreeSink put, void *ctx);

#endif
