#ifndef MAT4_H
#define MAT4_H

#include <stdint.h>

/* A 4x4 matrix, row-major: m[row][column]. The vertex shader multiplies a
 * position by it one row at a time (dp4), so row 0 gives clip x. */
typedef struct {
    float m[4][4];
} Mat4;

/* Orthographic projection for the TOP screen.
 * The top screen's buffer is stored turned a quarter turn (240 wide, 400
 * tall), so screen y feeds clip x (bottom -1, top +1) and screen x feeds
 * clip y (left +1, right -1).
 * Depth: z_near maps to clip z -1 and z_far to clip z 0, the PICA200's range. */
void mat4_ortho_tilt(Mat4 *out, float left, float right, float bottom, float top,
                     float z_near, float z_far);

/* out = m * v. v and out are (x, y, z, w). */
void mat4_mul_vec4(const Mat4 *m, const float v[4], float out[4]);

/* The 16 words the GPU expects for this matrix as four float32 shader
 * uniforms: one row at a time, each row sent as w, z, y, x. */
void mat4_uniform_words(const Mat4 *m, uint32_t out[16]);

#endif
