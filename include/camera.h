#ifndef CAMERA_H
#define CAMERA_H

#include "mat4.h"

/* The player's eye. World axes: +x east, +y up, -z north. */
typedef struct {
    float x, y, z; /* eye position */
    float yaw;     /* radians: 0 faces north (-z), more turns right (towards +x); kept in 0..2pi */
    float pitch;   /* radians: more looks up; kept within +-CAMERA_PITCH_MAX */
} Camera;

/* 89 degrees: never straight up or down, where turning left and right
 * would stop making sense. */
#define CAMERA_PITCH_MAX 1.5533430f

/* Turn by dyaw and dpitch radians, keeping yaw and pitch in range. */
void camera_turn(Camera *cam, float dyaw, float dpitch);

/* The unit vector the camera faces. */
void camera_forward(const Camera *cam, float out[3]);

/* World to camera space (camera at the origin looking down -z, +x right,
 * +y up), ready to go under a projection from mat4_persp_tilt(). */
void camera_view(Mat4 *out, const Camera *cam);

#endif
