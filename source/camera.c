#include <math.h>

#include "camera.h"

#define TWO_PI 6.28318531f

void camera_turn(Camera *cam, float dyaw, float dpitch)
{
    float yaw = fmodf(cam->yaw + dyaw, TWO_PI);
    float pitch = cam->pitch + dpitch;

    if (yaw < 0.0f)
        yaw += TWO_PI;
    if (yaw >= TWO_PI) /* -tiny + 2pi can round up to 2pi */
        yaw = 0.0f;
    if (pitch > CAMERA_PITCH_MAX)
        pitch = CAMERA_PITCH_MAX;
    if (pitch < -CAMERA_PITCH_MAX)
        pitch = -CAMERA_PITCH_MAX;
    cam->yaw = yaw;
    cam->pitch = pitch;
}

void camera_forward(const Camera *cam, float out[3])
{
    float cp = cosf(cam->pitch);

    out[0] = sinf(cam->yaw) * cp;
    out[1] = sinf(cam->pitch);
    out[2] = -cosf(cam->yaw) * cp;
}

/* The camera's own turn is Ry(-yaw) then Rx(pitch); the view undoes it:
 * Rx(-pitch) * Ry(yaw) * T(-eye). */
void camera_view(Mat4 *out, const Camera *cam)
{
    Mat4 m, t;

    mat4_rotate_x(&m, -cam->pitch);
    mat4_rotate_y(&t, cam->yaw);
    mat4_mul(&m, &m, &t);
    mat4_translate(&t, -cam->x, -cam->y, -cam->z);
    mat4_mul(out, &m, &t);
}
