#include "etch_sim.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void near(double actual, double expected)
{
    if (fabs(actual - expected) > 1e-8) {
        fprintf(stderr, "physics: %.17g != analytic %.17g\n", actual, expected);
        assert(0);
    }
}

static void point(MotionPoint actual, double x, double y)
{
    near(actual.x, x);
    near(actual.y, y);
}

static void defaults(void)
{
    EtchConfig c = EtchAssumedDefaults();
    near(c.width_mm, 170.15); near(c.height_mm, 127.6);
    near(c.x.speed_mm_s, 60); near(c.y.speed_mm_s, 60);
    near(c.x.acceleration_mm_s2, 250);
    near(c.y.acceleration_mm_s2, 250);
    near(c.x.backlash_mm, .15); near(c.y.backlash_mm, .15);
    near(c.x.step_mm, .05); near(c.y.step_mm, .05);
    near(c.erase_seconds, 2.5); near(c.ghost_fraction, .03);
}

/* Rest-to-rest line motion: feed limits speed, never acceleration. */
static double profile(double length, double speed, double acceleration)
{
    return length < speed * speed / acceleration
        ? 2 * sqrt(length / acceleration)
        : length / speed + speed / acceleration;
}

static double run(EtchConfig c, MotionPoint end, double feed)
{
    MotionPoint p[] = {{0, 0}, end};
    EtchDevice *d = EtchCreate(c, p[0]);
    assert(d);
    assert(feed ? EtchDeviceSubmit(d, p, 2, feed)
                : EtchDeviceSubmitHardware(d, p, 2));
    EtchStatus s = EtchDeviceGetStatus(d);
    double duration = s.remaining_s;
    assert(s.count == 1 && s.completed == 0 && s.drawing);
    assert(EtchDeviceTick(d, duration + 1));
    s = EtchDeviceGetStatus(d);
    near(s.elapsed_s, duration); near(s.remaining_s, 0);
    point(s.cursor_mm, end.x, end.y);
    point(s.motor_velocity_mm_s, 0, 0);
    assert(s.completed == 1 && EtchDeviceReady(d));
    EtchDestroy(d);
    return duration;
}

static void analytical_profiles(void)
{
    EtchConfig c = EtchAssumedDefaults();
    near(run(c, (MotionPoint){100, 0}, 0), 143.0 / 75);
    near(run(c, (MotionPoint){.1, 0}, 0), .04);
    near(run(c, (MotionPoint){100, 100}, 0), 143.0 / 75);
    near(run(c, (MotionPoint){100, 100}, 60), 134 * sqrt(2) / 75);
    c.y.speed_mm_s = 5; c.y.acceleration_mm_s2 = 10;
    near(run(c, (MotionPoint){100, 0}, 0), 143.0 / 75);
    near(run(c, (MotionPoint){0, 10}, 0), 2.5);
    near(run(c, (MotionPoint){10, 10}, 0),
         profile(sqrt(200), 5 * sqrt(2), 10 * sqrt(2)));
}
