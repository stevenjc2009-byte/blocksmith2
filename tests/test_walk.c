#include <math.h>

#include "test.h"
#include "walk.h"

#define TOL 1e-4
#define PI 3.14159265f

static void vel(int px, int py, float yaw, double want_x, double want_z)
{
    float vx = 99.0f, vz = 99.0f;

    walk_velocity(px, py, yaw, &vx, &vz);
    CHECK_NEAR(vx, want_x, TOL);
    CHECK_NEAR(vz, want_z, TOL);
}

void test_walk(void)
{
    const double s = WALK_SPEED, d = WALK_SPEED / sqrt(2.0);
    double part;

    /* Nothing pushed, or only a small push: standing still. */
    vel(0, 0, 0.0f, 0, 0);
    vel(WALK_PAD_DEAD, 0, 0.0f, 0, 0);
    vel(0, -WALK_PAD_DEAD, 0.0f, 0, 0);
    vel(10, 10, 1.0f, 0, 0); /* 14.1 long */

    /* Facing north (yaw 0, -z): up walks north, down south, right east,
     * left west. */
    vel(0, WALK_PAD_FULL, 0.0f, 0, -s);
    vel(0, -WALK_PAD_FULL, 0.0f, 0, s);
    vel(WALK_PAD_FULL, 0, 0.0f, s, 0);
    vel(-WALK_PAD_FULL, 0, 0.0f, -s, 0);
    /* Facing east: up walks east, right steps south. */
    vel(0, WALK_PAD_FULL, PI / 2, s, 0);
    vel(WALK_PAD_FULL, 0, PI / 2, 0, s);
    /* Facing south: up walks south. Facing west: up walks west. */
    vel(0, WALK_PAD_FULL, PI, 0, s);
    vel(0, WALK_PAD_FULL, 3 * PI / 2, -s, 0);

    /* Past full is still full speed; so is a full diagonal, not faster. */
    vel(0, 156, 0.0f, 0, -s);
    vel(WALK_PAD_FULL, WALK_PAD_FULL, 0.0f, d, -d);
    vel(-156, -156, 0.0f, -d, d);

    /* Part pushes: speed by how far, direction kept. */
    vel(0, 96, 0.0f, 0, -s * (96 - WALK_PAD_DEAD) / (WALK_PAD_FULL - WALK_PAD_DEAD));
    part = s * (100.0 - WALK_PAD_DEAD) / (WALK_PAD_FULL - WALK_PAD_DEAD);
    vel(60, 80, 0.0f, 0.6 * part, -0.8 * part);
}
