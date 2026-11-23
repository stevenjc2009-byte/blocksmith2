#include <string.h>

#include "block.h"
#include "chunk.h"

static int inside(int x, int y, int z)
{
    return x >= 0 && x < CHUNK_SIZE && y >= 0 && y < WORLD_HEIGHT && z >= 0 && z < CHUNK_SIZE;
}

void chunk_clear(Chunk *c)
{
    memset(c->blocks, BLOCK_AIR, sizeof c->blocks);
    memset(c->biome, 0, sizeof c->biome);
}

int chunk_get(const Chunk *c, int x, int y, int z)
{
    return inside(x, y, z) ? c->blocks[y][z][x] : BLOCK_AIR;
}

void chunk_set(Chunk *c, int x, int y, int z, int block)
{
    if (inside(x, y, z) && block >= 0 && block < BLOCK_COUNT)
        c->blocks[y][z][x] = (uint8_t)block;
}
