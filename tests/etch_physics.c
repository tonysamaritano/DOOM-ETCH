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
