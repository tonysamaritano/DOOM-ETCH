#include "timeline.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

_Static_assert(sizeof(MotionTimedSegment) == 32, "GPU timeline record must occupy 32 bytes");
_Static_assert(offsetof(MotionTimedSegment, ends) == 0, "Endpoint floats come first");
_Static_assert(offsetof(MotionTimedSegment, timing) == 16, "Timing floats follow endpoints");
static MotionPoint hundredPoints[] = {{0, 0}, {100, 0}};
static unsigned char hundredKinds[] = {MOTION_START, MOTION_OUTLINE};
static MotionPath hundred(void)
{
    return (MotionPath){.points = hundredPoints, .kinds = hundredKinds, .count = 2,
        .width = 100, .height = 50, .width_mm = 100, .height_mm = 50};
}
static void exactHundredMillimetres(void)
{
    MotionPath path = hundred();
    MotionTimedSegment *records = NULL; size_t count = 0; double seconds = 0;
    assert(MotionTimelineBuild(&path, 50, &records, &count, &seconds));
    assert(records && count == 1 && seconds == 2);
    const float ends[] = {0, 0, 1, 0}, timing[] = {0, 2, 0, 0};
    for (int i = 0; i < 4; ++i) {
        assert(records[0].ends[i] == ends[i]);
        assert(records[0].timing[i] == timing[i]);
    }
    free(records);
}

static void connectorsAndDiagonal(void)
{
    MotionPoint points[] = {{0, 0}, {30, 0}, {30, 40}, {0, 0}};
    unsigned char kinds[] = {MOTION_START, MOTION_CONNECTOR, MOTION_OUTLINE, MOTION_OUTLINE};
    MotionPath path = {.points = points, .kinds = kinds, .count = 4,
        .width = 100, .height = 100, .width_mm = 100, .height_mm = 100};
    MotionTimedSegment *records = NULL; size_t count = 0; double seconds = 0;
    assert(MotionTimelineBuild(&path, 10, &records, &count, &seconds));
    assert(count == 3 && seconds == 12); /* 30 + 40 + 50 millimetres. */
    const double start[] = {0, 3, 7}, end[] = {3, 7, 12};
    for (size_t i = 0; i < count; ++i) {
        assert(fabs(records[i].ends[0] - points[i].x / 100) < 1e-6);
        assert(fabs(records[i].ends[1] - points[i].y / 100) < 1e-6);
        assert(fabs(records[i].ends[2] - points[i + 1].x / 100) < 1e-6);
        assert(fabs(records[i].ends[3] - points[i + 1].y / 100) < 1e-6);
        assert(records[i].timing[0] == start[i] && records[i].timing[1] == end[i]);
        assert(records[i].timing[2] == 0 && records[i].timing[3] == 0);
        if (i) assert(records[i - 1].timing[1] == records[i].timing[0]);
    }
    assert(kinds[1] == MOTION_CONNECTOR && points[2].y == 40);
    free(records);
}
