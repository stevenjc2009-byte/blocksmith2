#ifndef MAT4_H
#define MAT4_H

#include <stdint.h>

/* A 4x4 matrix, row-major: m[row][column]. The vertex shader multiplies a
 * position by it one row at a time (dp4), so row 0 gives clip x.
 *
 * World and view space are right-handed: x right, y up, and the camera
 * looks down -z. */
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

/* Perspective projection for the TOP screen, turned the same way as
 * mat4_ortho_tilt. fovy is the vertical field of view in radians; aspect
 * is width / height (400 / 240 for the top screen). A point z_near in front
 * of the camera lands on clip depth -1, one z_far in front on 0. */
void mat4_persp_tilt(Mat4 *out, float fovy, float aspect, float z_near, float z_far);

void mat4_identity(Mat4 *out);

/* out = a * b: applying out to a point applies b first, then a.
 * out may be the same matrix as a or b. */
void mat4_mul(Mat4 *out, const Mat4 *a, const Mat4 *b);

void mat4_translate(Mat4 *out, float x, float y, float z);

/* Turns about the x / y axis by the given angle in radians,
 * counter-clockwise when looking from the positive end of the axis. */
void mat4_rotate_x(Mat4 *out, float radians);
void mat4_rotate_y(Mat4 *out, float radians);

/* out = m * v. v and out are (x, y, z, w). */
void mat4_mul_vec4(const Mat4 *m, const float v[4], float out[4]);

/* The 16 words the GPU expects for this matrix as four float32 shader
 * uniforms: one row at a time, each row sent as w, z, y, x. */
void mat4_uniform_words(const Mat4 *m, uint32_t out[16]);

#endif
