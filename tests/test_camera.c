#include <math.h>

#include "camera.h"
#include "test.h"

#define TOL 1e-4
#define PI 3.14159265f
#define DEG (PI / 180.0f)

static void check_turn(float yaw0, float pitch0, float dyaw, float dpitch,
                       float want_yaw, float want_pitch)
{
    Camera c = { 0 };

    c.yaw = yaw0;
    c.pitch = pitch0;
    camera_turn(&c, dyaw, dpitch);
    CHECK_NEAR(c.yaw, want_yaw, TOL);
    CHECK_NEAR(c.pitch, want_pitch, TOL);
}

static void check_forward(float yaw, float pitch, float fx, float fy, float fz)
{
    Camera c = { 0 };
    float f[3];

    c.yaw = yaw;
    c.pitch = pitch;
    camera_forward(&c, f);
    CHECK_NEAR(f[0], fx, TOL);
    CHECK_NEAR(f[1], fy, TOL);
    CHECK_NEAR(f[2], fz, TOL);
}

/* Where the view matrix puts a world point, in camera space. */
static void view_point(const Camera *c, float x, float y, float z, float out[4])
{
    Mat4 v;
    float p[4] = { x, y, z, 1.0f };

    camera_view(&v, c);
    mat4_mul_vec4(&v, p, out);
}

static void check_view(float yaw, float pitch)
{
    Camera c = { 2.5f, 3.0f, -1.5f, 0.0f, 0.0f };
    float f[3], o[4];
    float rx = cosf(yaw), rz = sinf(yaw); /* the camera's right, level */

    c.yaw = yaw;
    c.pitch = pitch;
    camera_forward(&c, f);

    /* The eye goes to the origin. */
    view_point(&c, c.x, c.y, c.z, o);
    CHECK_NEAR(o[0], 0.0, TOL);
    CHECK_NEAR(o[1], 0.0, TOL);
    CHECK_NEAR(o[2], 0.0, TOL);
    CHECK_NEAR(o[3], 1.0, TOL);

    /* A point 5 ahead lands 5 down the -z axis (straight ahead on screen). */
    view_point(&c, c.x + 5 * f[0], c.y + 5 * f[1], c.z + 5 * f[2], o);
    CHECK_NEAR(o[0], 0.0, TOL);
    CHECK_NEAR(o[1], 0.0, TOL);
    CHECK_NEAR(o[2], -5.0, TOL);

    /* A point 2 to the camera's right lands on +x: right on screen. */
    view_point(&c, c.x + 2 * rx, c.y, c.z + 2 * rz, o);
    CHECK_NEAR(o[0], 2.0, TOL);
    CHECK_NEAR(o[1], 0.0, TOL);
    CHECK_NEAR(o[2], 0.0, TOL);
}

void test_camera(void)
{
    Camera c = { 0 };
    float o[4];

    /* Turning: yaw stays in 0..2pi, pitch stops short of straight up/down. */
    check_turn(0, 0, 30 * DEG, 0, 30 * DEG, 0);
    check_turn(350 * DEG, 0, 20 * DEG, 0, 10 * DEG, 0);
    check_turn(10 * DEG, 0, -20 * DEG, 0, 350 * DEG, 0);
    check_turn(0, 0, 4 * PI + 1.0f, 0, 1.0f, 0);
    check_turn(0, 0, 0, 30 * DEG, 0, 30 * DEG);
    check_turn(0, 80 * DEG, 0, 30 * DEG, 0, CAMERA_PITCH_MAX);
    check_turn(0, -80 * DEG, 0, -30 * DEG, 0, -CAMERA_PITCH_MAX);
    CHECK_NEAR(CAMERA_PITCH_MAX, 89 * DEG, 1e-5);

    /* Facing: yaw 0 is north (-z), turning right goes east (+x). */
    check_forward(0, 0, 0, 0, -1);
    check_forward(90 * DEG, 0, 1, 0, 0);
    check_forward(180 * DEG, 0, 0, 0, 1);
    check_forward(270 * DEG, 0, -1, 0, 0);
    check_forward(0, 30 * DEG, 0, sinf(30 * DEG), -cosf(30 * DEG));
    check_forward(90 * DEG, -45 * DEG, cosf(45 * DEG), -sinf(45 * DEG), 0);

    check_view(0, 0);
    check_view(90 * DEG, 0);
    check_view(200 * DEG, 25 * DEG);
    check_view(300 * DEG, -60 * DEG);

    /* Which way: facing east, a block to the east is ahead (-z), a block to
     * the south is on the right (+x), and looking up puts a level point
     * below the middle of the screen (-y). */
    c.yaw = 90 * DEG;
    view_point(&c, 4, 0, 0, o);
    CHECK_NEAR(o[2], -4.0, TOL);
    view_point(&c, 0, 0, 4, o);
    CHECK_NEAR(o[0], 4.0, TOL);
    c.yaw = 0;
    c.pitch = 30 * DEG;
    view_point(&c, 0, 0, -4, o);
    CHECK_EQ_INT(o[1] < 0, 1);
    CHECK_EQ_INT(o[2] < 0, 1);
}
