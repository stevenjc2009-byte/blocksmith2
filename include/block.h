#ifndef BLOCK_H
#define BLOCK_H

#include <stdbool.h>

/* Block kinds and which texture tile each face of a block uses. */

enum {
    BLOCK_AIR,
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_TORCH, /* row 3-03: a small stick that lights the cells round it (light.h) */
    BLOCK_SAND,  /* row 4-01: beaches, river beds and the sea floor */
    BLOCK_WATER, /* row 4-01: still water in seas and rivers; not solid, see-through.
                    Row 4-04: a source, the water that never runs dry (water_flow.h) */
    /* Row 4-02 (biomes). Ids are stored in saved worlds: only ever add at the end. */
    BLOCK_SNOWY_GRASS,       /* snow on top, dirt sides with a white fringe */
    BLOCK_SNOW,              /* a whole block of snow: mountain tops, cold beaches */
    BLOCK_ICE,               /* solid and opaque for now: frozen seas and rivers */
    BLOCK_SANDSTONE,         /* under desert sand */
    BLOCK_RED_SAND,          /* badlands */
    BLOCK_TERRACOTTA,        /* badlands bands: plain orange-brown... */
    BLOCK_TERRACOTTA_WHITE,  /* ...pale... */
    BLOCK_TERRACOTTA_YELLOW, /* ...yellow... */
    BLOCK_TERRACOTTA_RED,    /* ...red... */
    BLOCK_TERRACOTTA_BROWN,  /* ...and dark brown */
    BLOCK_GRAVEL,            /* cold sea floors */
    BLOCK_PLANKS,           /* row 4-05: oak planks (villages); solid, breakable like stone */
    BLOCK_LOG,              /* row 4-05: oak log, bark sides and rings top and bottom (trees too, row 4-03) */
    BLOCK_COBBLESTONE,      /* row 4-05: village foundations, wells and ruins */
    BLOCK_MOSSY_COBBLESTONE, /* row 4-05: ruins */
    /* Row 4-04: flowing water (water_flow.h), its level in its id: level 1
     * runs beside a source, each step out one level more, level 7 the
     * thinnest. Falling water pours down from water above it, full height.
     * All water, like BLOCK_WATER (block_is_water()). These eight must stay
     * together and in this order (water.h reads the level from the id).
     * Saved ids: they come after every earlier block, never between. */
    BLOCK_FLOW_1,
    BLOCK_FLOW_2,
    BLOCK_FLOW_3,
    BLOCK_FLOW_4,
    BLOCK_FLOW_5,
    BLOCK_FLOW_6,
    BLOCK_FLOW_7,
    BLOCK_FALL,
    /* Row 4-03 (trees and plants): logs (bark sides, rings top and bottom), then
     * leaves (solid opaque cubes, gaps drawn dark, every face tinted by the biome's
     * foliage colour; birch and spruce a fixed one), then the cactus. All solid. */
    BLOCK_BIRCH_LOG,
    BLOCK_SPRUCE_LOG,
    BLOCK_DARK_OAK_LOG,
    BLOCK_JUNGLE_LOG,
    BLOCK_ACACIA_LOG,
    BLOCK_OAK_LEAVES,
    BLOCK_BIRCH_LEAVES,
    BLOCK_SPRUCE_LEAVES,
    BLOCK_DARK_OAK_LEAVES,
    BLOCK_JUNGLE_LEAVES,
    BLOCK_ACACIA_LEAVES,
    BLOCK_CACTUS,
    /* Plants: two crossed quads with a cut-out texture, not solid (block_is_plant()).
     * They must stay the LAST ids: block_solid() and the mesh count on that. */
    BLOCK_SHORT_GRASS, /* tinted like grass */
    BLOCK_FERN,        /* tinted like grass */
    BLOCK_DANDELION,
    BLOCK_POPPY,
    BLOCK_OXEYE_DAISY,
    BLOCK_BLUE_ORCHID,
    BLOCK_DEAD_BUSH,
    BLOCK_COUNT
};

/* The six faces of a block, by the way they face. */
enum {
    FACE_POS_X,
    FACE_NEG_X,
    FACE_POS_Y, /* top */
    FACE_NEG_Y, /* bottom */
    FACE_POS_Z,
    FACE_NEG_Z,
    FACE_COUNT
};

/* The TILE_* (blocktex.h) for one face of a block, or -1 for air or a
 * value out of range. */
int block_face_tile(int block, int face);

/* A plant (row 4-03): short grass up to the dead bush. Not solid, drawn as
 * crossed quads, gone when the block under it goes. */
static inline bool block_is_plant(int block)
{
    return block >= BLOCK_SHORT_GRASS && block < BLOCK_COUNT;
}

/* Is the block a solid cube: something the player cannot walk through, that
 * hides the faces of the blocks beside it and stops light? Every block is
 * except air, a torch, water and the plants (and anything not a block). */
bool block_solid(int block);

/* Is the block water of any kind: still (BLOCK_WATER), flowing or falling
 * (row 4-04)? Everything that treats water as water asks this, never
 * BLOCK_WATER alone. */
bool block_is_water(int block);

#endif
