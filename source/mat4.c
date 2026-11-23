#include <math.h>
#include <string.h>

#include "mat4.h"

void mat4_ortho_tilt(Mat4 *out, float left, float right, float bottom, float top,
                     float z_near, float z_far)
{
    memset(out, 0, sizeof *out);

    /* Clip x comes from screen y: bottom -> -1, top -> +1. */
    out->m[0][1] = 2.0f / (top - bottom);
    out->m[0][3] = (bottom + top) / (bottom - top);

    /* Clip y comes from screen x: left -> +1, right -> -1. */
    out->m[1][0] = 2.0f / (left - right);
    out->m[1][3] = (left + right) / (right - left);

    /* Clip z: z_near -> -1, z_far -> 0. */
    out->m[2][2] = 1.0f / (z_far - z_near);
    out->m[2][3] = -z_far / (z_far - z_near);

    out->m[3][3] = 1.0f;
}

void mat4_persp_tilt(Mat4 *out, float fovy, float aspect, float z_near, float z_far)
{
    float f = 1.0f / tanf(fovy * 0.5f);

    memset(out, 0, sizeof *out);

    /* Clip x from view y (up is +1), clip y from view x (right is -1). */
    out->m[0][1] = f;
    out->m[1][0] = -f / aspect;

    /* w = -z (distance in front of the camera). Clip z / w runs from -1 at
     * z_near to 0 at z_far. */
    out->m[2][2] = z_near / (z_near - z_far);
    out->m[2][3] = z_near * z_far / (z_near - z_far);
    out->m[3][2] = -1.0f;
}

void mat4_identity(Mat4 *out)
{
    memset(out, 0, sizeof *out);
    out->m[0][0] = out->m[1][1] = out->m[2][2] = out->m[3][3] = 1.0f;
}

void mat4_mul(Mat4 *out, const Mat4 *a, const Mat4 *b)
{
    Mat4 t;
    int r, c, k;

    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++) {
            float s = 0.0f;
            for (k = 0; k < 4; k++)
                s += a->m[r][k] * b->m[k][c];
            t.m[r][c] = s;
        }
    *out = t;
}

void mat4_translate(Mat4 *out, float x, float y, float z)
{
    mat4_identity(out);
    out->m[0][3] = x;
    out->m[1][3] = y;
    out->m[2][3] = z;
}

void mat4_rotate_x(Mat4 *out, float radians)
{
    float c = cosf(radians), s = sinf(radians);

    mat4_identity(out);
    out->m[1][1] = c;
    out->m[1][2] = -s;
    out->m[2][1] = s;
    out->m[2][2] = c;
}

void mat4_rotate_y(Mat4 *out, float radians)
{
    float c = cosf(radians), s = sinf(radians);

    mat4_identity(out);
    out->m[0][0] = c;
    out->m[0][2] = s;
    out->m[2][0] = -s;
    out->m[2][2] = c;
}

void mat4_mul_vec4(const Mat4 *m, const float v[4], float out[4])
{
    int r;

    for (r = 0; r < 4; r++)
        out[r] = m->m[r][0] * v[0] + m->m[r][1] * v[1] +
                 m->m[r][2] * v[2] + m->m[r][3] * v[3];
}

void mat4_uniform_words(const Mat4 *m, uint32_t out[16])
{
    int r, c;

    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++)
            memcpy(&out[r * 4 + (3 - c)], &m->m[r][c], sizeof out[0]);
}
