#include "motion.h"
#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t moves[2], pauses, ends, modes[4], sizes, starts, feeds;
    double width, height;
    MotionPoint start, last, boundary;
} CodeReport;
static int near(double a, double b) { return fabs(a - b) < 1e-7; }
static double word(const char *line, char name)
{
    const char *at = strchr(line, name); assert(at);
    char *end;
    double value = strtod(at + 1, &end);
    assert(end > at + 1 && isfinite(value)); return value;
}
static void boundedGridPoint(MotionPoint point, const CodeReport *report)
{
    assert(point.x >= 0 && point.x <= report->width + 1e-8);
    assert(point.y >= 0 && point.y <= report->height + 1e-8);
    assert(fabs(point.x / .05 - round(point.x / .05)) < 1e-6);
    double down = report->height - point.y;
    assert(fabs(down / .05 - round(down / .05)) < 1e-6);
}
