#include "r_etch.h"
#include "motion.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int samePoint(MotionPoint a, MotionPoint b)
{
    return fabs(a.x - b.x) < 1e-9 && fabs(a.y - b.y) < 1e-9;
}
static void checkBoundary(const MotionPath *path, const MotionSegment *edges, size_t count)
{
    unsigned visits[32] = {0};
    assert(count <= 32 && MotionValid(path));
    assert(path->width_mm == 170.15 && path->height_mm == 127.6);
    assert(samePoint(path->points[0], (MotionPoint){0, 0}));
    for (size_t i = 1; i < path->count; ++i) {
        if (path->kinds[i] == MOTION_CONNECTOR) continue;
        assert(path->kinds[i] == MOTION_OUTLINE);
        MotionPoint a = path->points[i - 1], b = path->points[i];
        double dx = b.x - a.x, dy = b.y - a.y;
        assert(dx == 0 || dy == 0);
        int steps = (int)lround(fabs(dx) + fabs(dy));
        assert(steps > 0 && fabs(steps - fabs(dx) - fabs(dy)) < 1e-9);
        MotionPoint delta = {dx / steps, dy / steps};
        for (int step = 0; step < steps; ++step) {
            MotionPoint next = {a.x + delta.x, a.y + delta.y};
            size_t edge;
            for (edge = 0; edge < count; ++edge)
                if ((samePoint(a, edges[edge].a) && samePoint(next, edges[edge].b)) ||
                    (samePoint(a, edges[edge].b) && samePoint(next, edges[edge].a))) break;
            assert(edge < count); ++visits[edge]; a = next;
        }
    }
    for (size_t edge = 0; edge < count; ++edge) assert(visits[edge] == 2);
}






int main(void)
{
    outerCell();
    adjacentSurfaces();
    nearWallWins();
    clippedBands();
    captureLifecycle();
    puts("Wall bands: boundaries, seams, clipping and capture lifecycle passed");
    return 0;
}
