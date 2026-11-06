#ifndef DOOM_MACHINE_TEST_H
#define DOOM_MACHINE_TEST_H
#include "motion.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void machine_near(double actual, double expected)
{
    if (!isfinite(actual) || fabs(actual - expected) > 1e-9) {
        fprintf(stderr, "machine: %.17g != expected %.17g\n", actual, expected);
        assert(0);
    }
}
static inline void machine_point(MotionPoint actual, double x, double y)
{
    machine_near(actual.x, x); machine_near(actual.y, y);
}
#endif
