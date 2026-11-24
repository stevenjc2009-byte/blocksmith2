#ifndef WATER_FLOW_H
#define WATER_FLOW_H

#include <stdbool.h>
#include <stdint.h>

#include "world.h"

/* Flowing water (roadmap row 4-04): water spreads and runs downhill, by
 * Minecraft Java's rules. Pure C, so the PC tests can run it.
 *
 * Kinds (block.h, water.h): a source (BLOCK_WATER) never moves or runs dry;
 * flowing water BLOCK_FLOW_1 to BLOCK_FLOW_7 carries its level; falling
 * water BLOCK_FALL pours down under water. Water only moves on its turn,
 * and only a cell something woke gets a turn: the generated seas and
 * rivers are still and stay still (nothing wakes them as they are made or
 * loaded). What wakes water:
 * - any block world_set() changes wakes that cell and the six round it,
 *   those that are water (Minecraft's neighbour updates): breaking a block
 *   beside water, putting one into flowing water, and every change the
 *   water makes itself;
 * - a chunk loading wakes every flowing and falling cell in it (water
 *   saved while it ran carries on).
 * A woken cell takes its turn at the next step; steps come every
 * WATER_FLOW_STEP seconds (Minecraft's 5 game ticks), so water runs a cell
 * a step.
 *
 * A turn (Minecraft's FlowingFluid.tick()), for the cell at (x, y, z):
 * 1. A flowing or falling cell works out what it should now be
 *    (water_rules_new() in water_rules.h): a source if two or more sources
 *    sit beside it and a solid block or a source is under it; else falling
 *    if water is above it; else one less than the most water beside it
 *    (water_amount()), or air if that is none. If that is not what it is,
 *    it becomes that (air ends the turn).
 * 2. Then it spreads (water_rules_spread()): down first, as falling water,
 *    into air or a torch under it. If it cannot go down, a source (always)
 *    or water over something not water spreads sideways: its amount less
 *    one (7 from falling water, nothing from level 7) into the sides that
 *    are air or a torch, but only those on the way to the nearest drop: a
 *    cell it could run into with air, a torch or water under it, found up
 *    to WATER_FLOW_SLOPE cells on through cells that are not solid and not
 *    sources. With no drop that near it spreads all four ways. A torch in
 *    the way is washed away.
 * Water spread into a cell gets what that cell works out in step 1, so
 * water between two sources over a floor becomes a source at once.
 *
 * The queue: up to WATER_FLOW_QUEUE cells wait for their turn, in the
 * order they were woken, each once (a cell woken again while it waits
 * keeps its place). It is a fixed ring with a table to find a cell in it:
 * no memory is asked for while the water runs. With the queue full, a wake
 * is turned away and counted in dropped; that changes no block, so no
 * source is ever lost, but the cell it was for waits until something wakes
 * it again. One water_flow_update() works at most WATER_FLOW_CAP cells and
 * looks at most WATER_FLOW_LOOKS queue entries; the rest wait for the next
 * call (water then runs slower, never wrong). A cell whose turn would read
 * a chunk that is not loaded (within WATER_FLOW_REACH blocks) waits in the
 * queue, looked at once a step; a cell whose own chunk is not loaded is
 * dropped, and found again when the chunk loads if it is flowing or
 * falling. */

#define WATER_FLOW_STEP 0.25f   /* seconds between steps */
#define WATER_FLOW_QUEUE 2048   /* cells that can wait for a turn */
#define WATER_FLOW_HASH (2 * WATER_FLOW_QUEUE) /* the table finding them */
#define WATER_FLOW_CAP 64       /* most turns in one water_flow_update() */
#define WATER_FLOW_LOOKS 256    /* most queue entries looked at in one */
#define WATER_FLOW_SLOPE 4      /* how far on water looks for a drop */
#define WATER_FLOW_REACH (WATER_FLOW_SLOPE + 1) /* blocks round a cell its turn reads */
#define WATER_FLOW_CHANGES (WATER_FLOW_CAP * 5) /* a turn changes at most 5 cells */

/* A cell waiting for its turn: due is the step it may have it. */
typedef struct {
    int32_t x, z;
    int16_t y;
    uint32_t due;
} WaterFlowCell;

/* A cell the water changed: torch says a torch was washed away there (its
 * light changes as when one is broken). */
typedef struct {
    int32_t x, z;
    int16_t y;
    bool torch;
} WaterFlowChange;

typedef struct {
    WaterFlowCell queue[WATER_FLOW_QUEUE]; /* a ring, the oldest at head */
    uint16_t hash[WATER_FLOW_HASH]; /* queue index + 1 of each waiting cell; 0 empty */
    int head, count;
    uint32_t step;    /* steps so far */
    float time;       /* seconds into this step */
    unsigned dropped; /* wakes turned away because the queue was full */
    int worked;       /* turns in the last water_flow_update() */
    int looked;       /* queue entries it looked at */
    WaterFlowChange changed[WATER_FLOW_CHANGES]; /* what it changed, in order */
    int changed_n;
    WorldWatch watch; /* what water_flow_attach() gives the world */
} WaterFlow;

/* An empty queue at step 0. */
void water_flow_init(WaterFlow *f);

/* Make f the watcher of w (world_set_watch()), so changes to w wake its
 * water and chunks loading into it are searched for flowing water. f must
 * stay where it is while attached. */
void water_flow_attach(WaterFlow *f, World *w);

/* Queue cell (x, y, z) for a turn at the next step. True if it is now
 * waiting (or already was); false if the queue is full (counted in
 * dropped). */
bool water_flow_add(WaterFlow *f, int x, int y, int z);

/* Wake cell (x, y, z) and the six cells round it: queue those that are
 * water in w. */
void water_flow_wake(WaterFlow *f, const World *w, int x, int y, int z);

/* Queue every flowing and falling cell of loaded chunk (cx, cz). */
void water_flow_found(WaterFlow *f, const World *w, int cx, int cz);

/* Let dt seconds pass: a new step starts each WATER_FLOW_STEP seconds (at
 * most one a call), then the cells whose step has come take their turns,
 * oldest first, within the caps. Records what changed in f->changed (for
 * the meshes, cleared at the start of each call) and returns f->worked. */
int water_flow_update(WaterFlow *f, World *w, float dt);

/* Is a cell waiting whose step has come (water_flow_update() with dt 0
 * would look at it)? */
bool water_flow_due(const WaterFlow *f);

#endif
