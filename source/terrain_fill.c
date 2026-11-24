#include <string.h>

#include "block.h"
#include "terrain.h"

/* Filling a chunk with the blocks of its columns (terrain.h says how a
 * column is layered). */

void terrain_fill_biomes(Chunk *c, uint32_t seed, int cx, int cz)
{
    TerrainColumn col[CHUNK_SIZE][CHUNK_SIZE];
    int x, z;

    terrain_chunk_columns(seed, cx, cz, col);
    for (z = 0; z < CHUNK_SIZE; z++)
        for (x = 0; x < CHUNK_SIZE; x++)
            c->biome[z][x] = col[z][x].biome;
}

void terrain_fill_chunk(Chunk *c, uint32_t seed, int cx, int cz)
{
    TerrainColumn col[CHUNK_SIZE][CHUNK_SIZE];

    terrain_fill_chunk_columns(c, seed, cx, cz, col);
}

void terrain_fill_chunk_columns(Chunk *c, uint32_t seed, int cx, int cz,
                                TerrainColumn col[CHUNK_SIZE][CHUNK_SIZE])
{
    int x, y, z, low = WORLD_HEIGHT;

    terrain_chunk_columns(seed, cx, cz, col);
    for (z = 0; z < CHUNK_SIZE; z++)
        for (x = 0; x < CHUNK_SIZE; x++) {
            int under_from = col[z][x].height - col[z][x].depth;

            low = under_from < low ? under_from : low;
            c->biome[z][x] = col[z][x].biome;
        }
    /* Stone up to the lowest of the layers under the surface, air above;
     * then each column. */
    memset(c->blocks, BLOCK_STONE, (size_t)low * sizeof c->blocks[0]);
    memset(c->blocks[low], BLOCK_AIR, (size_t)(WORLD_HEIGHT - low) * sizeof c->blocks[0]);
    for (z = 0; z < CHUNK_SIZE; z++)
        for (x = 0; x < CHUNK_SIZE; x++) {
            const TerrainColumn *k = &col[z][x];
            int h = k->height, under_from = h - k->depth;

            for (y = low; y < under_from; y++)
                c->blocks[y][z][x] = BLOCK_STONE;
            if (k->under == BLOCK_TERRACOTTA) {
                for (; y < h; y++)
                    c->blocks[y][z][x] = (uint8_t)terrain_band_block(y);
            } else {
                for (; y < h; y++)
                    c->blocks[y][z][x] = k->under;
            }
            c->blocks[h][z][x] = k->surface;
            for (y = h + 1; y <= TERRAIN_SEA; y++)
                c->blocks[y][z][x] = BLOCK_WATER;
            if (h < TERRAIN_SEA && biome_frozen((Biome)k->biome))
                c->blocks[TERRAIN_SEA][z][x] = BLOCK_ICE;
        }
}
