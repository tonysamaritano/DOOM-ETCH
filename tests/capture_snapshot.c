#include "r_etch.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>

static void rectangle(double top, double bottom)
{
    assert(R_EtchBegin(4, 4));
    R_EtchWallBegin();
    R_EtchWallColumn(0, 0, top, bottom, 0, 0);
    R_EtchWallColumn(0, 1, top, bottom, 0, 0);
    R_EtchWallEnd();
}
static void assertSamePath(const MotionPath *a, const MotionPath *b)
{
    assert(MotionValid(a) && MotionValid(b));
    assert(a->count == b->count && a->width == b->width && a->height == b->height);
    assert(a->width_mm == b->width_mm && a->height_mm == b->height_mm);
    for (size_t i = 0; i < a->count; ++i) {
        assert(a->points[i].x == b->points[i].x && a->points[i].y == b->points[i].y);
        assert(a->kinds[i] == b->kinds[i]);
    }
}
static MotionPath plan(const EtchSnapshot *snapshot)
{
    MotionPath path = {0};
    assert(MotionPlanSegments(&path, snapshot->segments, snapshot->count,
                              snapshot->width, snapshot->height, (MotionPoint){0, 0}));
    return path;
}

static void ownershipAndDirectParity(void)
{
    EtchSnapshot first = {0}, second = {0};
    rectangle(1, 3);
    assert(R_EtchTakeSnapshot(&first));
    assert(first.segments && first.count && first.width == 4 && first.height == 4);
    MotionSegment saved = first.segments[0];
    MotionSegment *owned = first.segments;
    rectangle(0, 1);
    assert(R_EtchTakeSnapshot(&second));
    assert(second.segments && second.segments != owned);
    assert(first.segments == owned && first.segments[0].a.x == saved.a.x);
    assert(first.segments[0].a.y == saved.a.y && first.segments[0].b.x == saved.b.x);
    assert(first.segments[0].b.y == saved.b.y);
    MotionPath snapshotPath = plan(&first);
    free(first.segments); /* Planned geometry no longer depends on captured storage. */
    rectangle(1, 3);
    MotionPath direct = {0}; assert(R_EtchBuildMotion(&direct));
    assertSamePath(&snapshotPath, &direct);
    MotionPath other = plan(&second);
    assert(MotionValid(&other));
    free(second.segments);
    MotionFree(&snapshotPath); MotionFree(&direct); MotionFree(&other);
}
