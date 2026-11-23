#ifndef GPU_STATE_H
#define GPU_STATE_H

/* The register writes behind gpu.c, one function per part of the PICA200
 * pipeline. Each adds commands to libctru's current GPUCMD buffer.
 * 3DS only: not part of the PC tests. */

#include <3ds.h>

#include "mat4.h"

/* The top screen's buffer is stored turned a quarter turn: 240 wide,
 * 400 tall. */
#define GPU_TOP_W 240
#define GPU_TOP_H 400

/* Colour buffer (RGBA8) and depth buffer (24-bit depth, 8-bit stencil),
 * both GPU_TOP_W x GPU_TOP_H, in VRAM. */
void gpu_state_framebuffer(void *color, void *depth);

void gpu_state_viewport(void);

/* Back faces culled, depth test on (nearer wins), no blending. */
void gpu_state_fragment_ops(void);

/* Texture unit 0 reads `tex` (tiled RGBA8, w x h, in linear memory),
 * nearest texel, clamped at the edges. */
void gpu_state_texture(const void *tex, int w, int h);

/* Colour = texture colour times vertex colour. */
void gpu_state_texenv(void);

/* Vertices come from `verts`: position (3 floats), texture coordinates
 * (2 floats), brightness (3 floats), `stride` bytes apart. */
void gpu_state_vertex_input(const void *verts, unsigned stride);

/* Load a 4x4 matrix into the float uniform at register `reg`. */
void gpu_state_uniform(int reg, const Mat4 *m);

/* Draw `count` vertices as separate triangles. */
void gpu_state_draw_triangles(int count);

/* Write the finished picture out of the GPU's caches. Last thing in a frame. */
void gpu_state_flush(void);

#endif
