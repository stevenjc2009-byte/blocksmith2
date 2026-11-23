#ifndef GPU_H
#define GPU_H

#include <stdbool.h>
#include <stdint.h>

#include "mat4.h"
#include "mesh.h"

/* Our own PICA200 code for the top screen: register writes through
 * libctru's GPUCMD calls and a picasso shader, no citro3d (kickoff T-5).
 * Draws textured, depth-tested triangles from the block texture picture
 * (blocktex.h).
 *
 * Each frame: gpu_frame_begin(), any number of gpu_draw(), gpu_frame_end(). */

/* Call after gfxInitDefault(). Returns false if memory or the shader
 * could not be set up; call gpu_exit() either way. */
bool gpu_init(void);

/* Start a new frame's command list. */
void gpu_frame_begin(void);

/* Add `count` vertices (whole triangles) to the frame, placed by `mvp`.
 * `verts` must be in linear memory (linearAlloc) with the data cache
 * flushed, and must stay put until gpu_frame_end() returns. */
void gpu_draw(const MeshVertex *verts, int count, const Mat4 *mvp);

/* Like gpu_draw(), but behind everything: no depth test and no depth
 * written, so whatever is drawn after it covers it. */
void gpu_draw_sky(const MeshVertex *verts, int count, const Mat4 *mvp);

/* The colour gpu_frame_end() clears to, 0xRRGGBBAA: sky blue until it is
 * changed (the start screens), the sky's colour of the moment in the game
 * (skydraw.h). */
void gpu_set_clear(uint32_t rgba);

/* The light the draws after it are lit by: 1 full, less darker (day and
 * night, row 3-02). gpu_frame_begin() sets it back to 1. */
void gpu_set_light(float light);

/* Clear to the colour from gpu_set_clear(), run the frame's commands and
 * copy the picture into the top screen's current framebuffer. Call before
 * gfxSwapBuffers(). */
void gpu_frame_end(void);

void gpu_exit(void);

#endif
