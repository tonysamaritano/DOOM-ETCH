#include "motion.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const MotionSegment fixture[] = {
    {{0, 0}, {2, 0}}, {{2, 0}, {2, 2}}, {{6, 0}, {8, 0}}
};
static int near(double a, double b) { return fabs(a - b) < 1e-9; }
static void pointEquals(MotionPoint actual, MotionPoint expected)
{
    assert(near(actual.x, expected.x) && near(actual.y, expected.y));
}
static double lengthMM(const MotionPath *path)
{
    double length = 0;
    for (size_t i = 1; i < path->count; ++i) {
        double dx = (path->points[i].x - path->points[i - 1].x) * path->width_mm / path->width;
        double dy = (path->points[i].y - path->points[i - 1].y) * path->height_mm / path->height;
        length += hypot(dx, dy);
    }
    return length;
}

static void disconnectedRoute(void)
{
    MotionPath path = {0};
    assert(MotionPlanSegments(&path, fixture, 3, 10, 10, (MotionPoint){0, 0}));
    assert(MotionValid(&path));
    assert(path.width == 10 && path.height == 10);
    assert(near(path.width_mm, 170.15) && near(path.height_mm, 127.6));
    const MotionPoint expected[] = {
        {0, 0}, {2, 0}, {2, 2}, {2, 0}, {0, 0}, {6, 0}, {8, 0}, {6, 0}
    };
    assert(path.count == sizeof(expected) / sizeof(*expected));
    for (size_t i = 0; i < path.count; ++i) {
        pointEquals(path.points[i], expected[i]);
        int kind = i == 0 ? MOTION_START : i == 5 ? MOTION_CONNECTOR : MOTION_OUTLINE;
        assert(path.kinds[i] == kind);
    }
    assert(near(lengthMM(&path), 289.25));
    printf("fixture: %zu moves, %.9f mm\n", path.count - 1, lengthMM(&path));
    MotionFree(&path);
}
