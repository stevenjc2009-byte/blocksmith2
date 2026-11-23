#ifndef STREAM_H
#define STREAM_H

#include <stdbool.h>

#include "world.h"

/* Which chunks are kept round the player (roadmap row 2-01). Pure C, so the
 * PC tests can run it.
 *
 * The player stands in chunk (pcx, pcz). A chunk's ring is how far it is
 * from there in whole chunks, on x or z, whichever is more: the player's
 * own chunk is ring 0, the eight round it ring 1, and so on.
 * - Chunks up to ring STREAM_VIEW (a 7 x 7 square) are drawn.
 * - One ring further, ring STREAM_KEEP (9 x 9), is loaded too, so every
 *   drawn chunk has all four neighbours and its mesh hides the faces
 *   between them from the start (no rebuild when a neighbour arrives).
 * - Everything further away is put away (forgotten until saving, row 2-04).
 *
 * New work comes as jobs, one at a time, nearest first: load a chunk (make
 * its ground), or build a chunk's mesh once it and its four neighbours are
 * loaded. The order is: load ring 0, load ring 1, mesh ring 0, load ring 2,
 * mesh ring 1, ... load ring STREAM_KEEP, mesh ring STREAM_VIEW. Within a
 * ring the chunk nearest the player's chunk (straight-line distance between
 * chunk positions) goes first, ties in reading order (north to south, west
 * to east). */

#define STREAM_VIEW 3
#define STREAM_KEEP (STREAM_VIEW + 1)

#if WORLD_SLOTS < 2 * STREAM_KEEP + 1
#error "the world holds too few chunks for the square round the player"
#endif

typedef enum { STREAM_NONE, STREAM_LOAD, STREAM_MESH } StreamJobKind;

typedef struct {
    StreamJobKind kind;
    int cx, cz; /* the chunk to load or mesh */
} StreamJob;

/* Chunk (cx, cz)'s ring round the player's chunk (pcx, pcz). */
int stream_ring(int cx, int cz, int pcx, int pcz);

/* Put away every loaded chunk further than ring STREAM_KEEP. */
void stream_put_away(World *w, int pcx, int pcz);

/* Should the mesh in slot [j][i] stay? Only while the slot holds a loaded
 * chunk no further than ring STREAM_VIEW. */
bool stream_keep_mesh(const World *w, int i, int j, int pcx, int pcz);

/* The next job, or false (kind STREAM_NONE) when everything round the
 * player is loaded and meshed. meshed[j][i] says slot [j][i]'s chunk has its
 * mesh built; whoever runs the jobs clears it whenever that slot's chunk is
 * loaded, put away or leaves ring STREAM_VIEW. */
bool stream_next(const World *w, const bool meshed[WORLD_SLOTS][WORLD_SLOTS], int pcx, int pcz,
                 StreamJob *job);

#endif
