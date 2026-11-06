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

static void rectangle(void)
{
    const MotionSegment expected[] = {
        {{0, 1}, {2, 1}}, {{2, 1}, {2, 3}}, {{2, 3}, {0, 3}}, {{0, 3}, {0, 1}}
    };
    assert(R_EtchBegin(4, 4));
    R_EtchFrameBegin(4, 4);
    R_EtchWallBegin();
    R_EtchWallColumn(0, 0, 1, 3, 0, 0);
    R_EtchWallColumn(0, 1, 1, 3, 0, 0);
    R_EtchWallEnd();
    MotionPath path = {0};
    assert(R_EtchBuildMotion(&path));
    outlines(&path, expected, 4);
    MotionFree(&path);
}
static void slopingWall(void)
{
    const MotionSegment expected[] = {
        {{0, 1}, {2, 2}}, {{2, 2}, {2, 4}}, {{2, 4}, {0, 3}}, {{0, 3}, {0, 1}}
    };
    assert(R_EtchBegin(4, 5));
    R_EtchFrameBegin(4, 5);
    R_EtchWallBegin();
    R_EtchWallColumn(0, 0, 1, 3, 0.5, 0.5);
    R_EtchWallColumn(0, 1, 1.5, 3.5, 0.5, 0.5);
    R_EtchWallEnd();
    MotionPath path = {0}; assert(R_EtchBuildMotion(&path));
    outlines(&path, expected, 4);
    MotionFree(&path);
}

static void disjointTiers(void)
{
    const MotionSegment expected[] = {
        {{0, 0}, {1, 0}}, {{1, 0}, {1, 1}}, {{1, 1}, {0, 1}}, {{0, 1}, {0, 0}},
        {{0, 3}, {1, 3}}, {{1, 3}, {1, 4}}, {{1, 4}, {0, 4}}, {{0, 4}, {0, 3}}
    };
    assert(R_EtchBegin(4, 4)); R_EtchFrameBegin(4, 4);
    R_EtchWallBegin();
    R_EtchWallColumn(1, 0, 0, 1, 0, 0);
    R_EtchWallColumn(2, 0, 3, 4, 0, 0);
    R_EtchWallEnd();
    MotionPath path = {0}; assert(R_EtchBuildMotion(&path));
    outlines(&path, expected, 8);
    unsigned connectors = 0;
    for (size_t i = 1; i < path.count; ++i) connectors += path.kinds[i] == MOTION_CONNECTOR;
    assert(connectors == 1);
    MotionFree(&path);
}
static void slopingClip(void)
{
    const MotionSegment expected[] = {
        {{0, 1}, {1, 1.5}}, {{1, 1.5}, {1, 2.75}},
        {{1, 2.75}, {0, 3}}, {{0, 3}, {0, 1}}
    };
    assert(R_EtchBegin(4, 4)); R_EtchFrameBegin(4, 4);
    R_EtchClipPlane(0, 1, 0.5, 3, -0.25);
    R_EtchClipPlane(0, 0, 0, 4, 0); /* Farther geometry cannot widen the opening. */
    R_EtchWallBegin(); R_EtchWallColumn(0, 0, 0, 4, 0, 0); R_EtchWallEnd();
    MotionPath path = {0}; assert(R_EtchBuildMotion(&path));
    outlines(&path, expected, 4);
    MotionFree(&path);
}

static void offscreenAndClosed(void)
{
    const MotionSegment expected[] = {
        {{0, 0}, {1, 0}}, {{1, 0}, {1, 4}}, {{1, 4}, {0, 4}}, {{0, 4}, {0, 0}}
    };
    assert(R_EtchBegin(2, 4)); R_EtchFrameBegin(2, 4);
    R_EtchWallBegin();
    R_EtchWallColumn(-1, 0, 0, 4, 0, 0);
    R_EtchWallColumn(3, 0, 0, 4, 0, 0);
    R_EtchWallColumn(0, -1, 0, 4, 0, 0);
    R_EtchWallColumn(0, 2, 0, 4, 0, 0);
    R_EtchWallColumn(0, 0, -1, 5, -1, 1);
    R_EtchWallEnd();
    MotionPath path = {0}; assert(R_EtchBuildMotion(&path));
    outlines(&path, expected, 4); MotionFree(&path);
    assert(R_EtchBegin(2, 4)); R_EtchFrameBegin(2, 4);
    R_EtchClipPlane(0, 2, 0, 2, 0);
    R_EtchWallBegin(); R_EtchWallColumn(0, 0, 0, 4, 0, 0); R_EtchWallEnd();
    assert(R_EtchBuildMotion(&path));
    assert(MotionValid(&path) && path.count == 1);
    MotionFree(&path);
}
