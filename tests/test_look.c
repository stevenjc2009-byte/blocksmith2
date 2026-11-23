#include "look.h"
#include "test.h"

#define TOL 1e-5
#define FULL (LOOK_DEG_PER_S * 3.14159265f / 180.0f)

static void rates(LookInput in, float want_yaw, float want_pitch)
{
    float yaw = 99.0f, pitch = 99.0f;

    look_rates(&in, &yaw, &pitch);
    CHECK_NEAR(yaw, want_yaw, TOL);
    CHECK_NEAR(pitch, want_pitch, TOL);
}

void test_look(void)
{
    const LookInput none = { 0 };
    LookInput in;

    rates(none, 0.0f, 0.0f);

    /* A B X Y as a diamond: A right, Y left, X up, B down. */
    in = none; in.right = true; rates(in, FULL, 0.0f);
    in = none; in.left = true;  rates(in, -FULL, 0.0f);
    in = none; in.up = true;    rates(in, 0.0f, FULL);
    in = none; in.down = true;  rates(in, 0.0f, -FULL);
    in = none; in.right = in.left = true; rates(in, 0.0f, 0.0f);
    in = none; in.up = in.down = true;    rates(in, 0.0f, 0.0f);
    in = none; in.right = in.up = true;   rates(in, FULL, FULL);

    /* C-stick: right and up are positive. Small pushes are ignored. */
    in = none; in.cx = LOOK_STICK_DEAD;  rates(in, 0.0f, 0.0f);
    in = none; in.cx = -LOOK_STICK_DEAD; rates(in, 0.0f, 0.0f);
    in = none; in.cy = LOOK_STICK_DEAD;  rates(in, 0.0f, 0.0f);
    in = none; in.cx = LOOK_STICK_FULL;  rates(in, FULL, 0.0f);
    in = none; in.cx = -LOOK_STICK_FULL; rates(in, -FULL, 0.0f);
    in = none; in.cy = LOOK_STICK_FULL;  rates(in, 0.0f, FULL);
    in = none; in.cy = -LOOK_STICK_FULL; rates(in, 0.0f, -FULL);
    /* Beyond full is still full speed. */
    in = none; in.cx = 300;  rates(in, FULL, 0.0f);
    in = none; in.cy = -300; rates(in, 0.0f, -FULL);
    /* Halfway between the dead zone and full is half speed. */
    in = none;
    in.cx = (LOOK_STICK_DEAD + LOOK_STICK_FULL) / 2;
    rates(in, FULL * (float)(in.cx - LOOK_STICK_DEAD) / (LOOK_STICK_FULL - LOOK_STICK_DEAD), 0.0f);
    {
        float yaw, pitch;
        look_rates(&in, &yaw, &pitch);
        CHECK_NEAR(yaw / FULL, 0.5, 0.01);
    }
    /* Stick and button together never go past full speed. */
    in = none; in.right = true; in.cx = LOOK_STICK_FULL; rates(in, FULL, 0.0f);
    /* ... and pushing against each other cancels. */
    in = none; in.left = true; in.cx = LOOK_STICK_FULL; rates(in, 0.0f, 0.0f);
}
