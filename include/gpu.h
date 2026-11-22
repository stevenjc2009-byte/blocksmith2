#ifndef GPU_H
#define GPU_H

#include <stdbool.h>

/* Our own PICA200 code for the top screen: register writes through
 * libctru's GPUCMD calls and a picasso shader, no citro3d (kickoff T-5).
 * Roadmap row 1-02: one triangle. */

/* Call after gfxInitDefault(). Returns false if memory or the shader
 * could not be set up. */
bool gpu_init(void);

/* Clear, draw the triangle and copy the picture into the top screen's
 * current framebuffer. Call once a frame, before gfxSwapBuffers(). */
void gpu_draw_top(void);

void gpu_exit(void);

#endif
