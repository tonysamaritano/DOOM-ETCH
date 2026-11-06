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
