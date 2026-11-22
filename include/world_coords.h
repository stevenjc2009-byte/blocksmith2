#ifndef WORLD_COORDS_H
#define WORLD_COORDS_H

/* World coordinate helpers. Pure C, no 3DS calls, so the PC tests can run them.
 *
 * The world is endless in X and Z (kickoff answer W-1), so block coordinates
 * go negative. A chunk is CHUNK_SIZE blocks wide on X and Z. Block -1 belongs
 * to chunk -1 at local position 15, not to chunk 0: plain C division rounds
 * towards zero and gets this wrong, so these helpers round down instead.
 */

#define CHUNK_SIZE 16
#define WORLD_HEIGHT 128 /* kickoff answer W-2 */

/* Chunk that holds world block coordinate b (on X or Z). */
int block_to_chunk(int b);

/* Position of world block coordinate b inside its chunk, 0 to CHUNK_SIZE - 1. */
int block_to_local(int b);

#endif
