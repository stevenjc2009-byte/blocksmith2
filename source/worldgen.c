#include "worldgen.h"

#include "structure.h"
#include "terrain.h"
#include "trees.h"

void world_gen_chunk(Chunk *c, uint32_t seed, int cx, int cz)
{
    TerrainColumn col[CHUNK_SIZE][CHUNK_SIZE];

    terrain_fill_chunk_columns(c, seed, cx, cz, col);
    trees_apply(c, seed, cx, cz, col); /* before the structures: they win, and trees keep clear of them */
    structure_apply(c, seed, cx, cz);
}
