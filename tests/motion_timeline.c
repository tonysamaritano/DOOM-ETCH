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
