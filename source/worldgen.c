#include "worldgen.h"

#include "structure.h"
#include "terrain.h"

void world_gen_chunk(Chunk *c, uint32_t seed, int cx, int cz)
{
    terrain_fill_chunk(c, seed, cx, cz);
    structure_apply(c, seed, cx, cz);
}
