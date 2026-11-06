#include "r_etch.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static double distance(MotionPoint a, MotionPoint b)
{ return hypot(a.x - b.x, a.y - b.y); }
static int onSide(MotionPoint point, MotionSegment side)
{
    return fabs(distance(side.a, point) + distance(point, side.b)
                - distance(side.a, side.b)) < 1e-8;
}
static void outlines(const MotionPath *path, const MotionSegment *sides, size_t count)
{
    double covered[16] = {0};
    assert(MotionValid(path) && count <= 16);
    assert(path->width_mm == 170.15 && path->height_mm == 127.6);
    for (size_t i = 1; i < path->count; ++i) {
        if (path->kinds[i] == MOTION_CONNECTOR) continue;
        assert(path->kinds[i] == MOTION_OUTLINE);
        MotionPoint a = path->points[i - 1], b = path->points[i];
        size_t side;
        for (side = 0; side < count; ++side)
            if (onSide(a, sides[side]) && onSide(b, sides[side])) break;
        assert(side < count);
        covered[side] += distance(a, b);
    }
    for (size_t side = 0; side < count; ++side)
        assert(fabs(covered[side] - 2 * distance(sides[side].a, sides[side].b)) < 1e-8);
}
