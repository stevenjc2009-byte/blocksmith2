#ifndef FRAME_H
#define FRAME_H

#include <3ds.h>

#include "fps.h"

/* The end of each frame, on the new-world screen and while playing. 3DS
 * calls, so not in the PC tests. */

/* Hand the frame over, then wait for screen refreshes until the second one
 * since the last frame's wait: 30 frames a second. */
void frame_end(FpsCounter *fps, u64 *paced);

/* The crosshair, drawn by the CPU straight into the top screen's picture
 * after the GPU has copied the frame there. The 3DS keeps that picture
 * standing on its side: gfxGetFramebuffer() calls the top screen 240 wide
 * and 400 tall, so they are passed swapped. Its cache lines are thrown away
 * first (the GPU wrote the memory behind the CPU's back, and an old line
 * written back would put last frame's pixels over this one), then written
 * out. */
void frame_crosshair(void);

#endif
