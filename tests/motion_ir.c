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

static void duplicateAndStraightEdges(void)
{
    MotionSegment repeated[5];
    memcpy(repeated, fixture, sizeof(fixture));
    repeated[3] = (MotionSegment){{2, 0}, {0, 0}};
    repeated[4] = (MotionSegment){{5, 5}, {5, 5}};
    MotionPath baseline = {0}, duplicate = {0};
    assert(MotionPlanSegments(&baseline, fixture, 3, 10, 10, (MotionPoint){0, 0}));
    assert(MotionPlanSegments(&duplicate, repeated, 5, 10, 10, (MotionPoint){0, 0}));
    assert(baseline.count == duplicate.count);
    for (size_t i = 0; i < baseline.count; ++i) {
        pointEquals(duplicate.points[i], baseline.points[i]);
        assert(duplicate.kinds[i] == baseline.kinds[i]);
    }
    MotionSegment straight[] = {{{0, 0}, {2, 0}}, {{2, 0}, {4, 0}}};
    assert(MotionPlanSegments(&duplicate, straight, 2, 10, 10, (MotionPoint){0, 0}));
    assert(duplicate.count == 3);
    pointEquals(duplicate.points[0], (MotionPoint){0, 0});
    pointEquals(duplicate.points[1], (MotionPoint){4, 0});
    pointEquals(duplicate.points[2], (MotionPoint){0, 0});
    assert(duplicate.kinds[1] == MOTION_OUTLINE && duplicate.kinds[2] == MOTION_OUTLINE);
    MotionFree(&baseline);
    MotionFree(&duplicate);
}

static void copyAndFree(void)
{
    MotionPath source = {0}, copy = {0};
    assert(MotionPlanSegments(&source, fixture, 3, 10, 10, (MotionPoint){0, 0}));
    assert(MotionCopy(&copy, &source));
    assert(copy.points != source.points && copy.kinds != source.kinds);
    assert(copy.count == source.count && copy.width == source.width && copy.height == source.height);
    assert(copy.width_mm == source.width_mm && copy.height_mm == source.height_mm);
    for (size_t i = 0; i < source.count; ++i) {
        pointEquals(copy.points[i], source.points[i]);
        assert(copy.kinds[i] == source.kinds[i]);
    }
    MotionPoint *owned = copy.points;
    assert(MotionCopy(&copy, &copy) && copy.points == owned);
    source.points[1].x = 1;
    source.kinds[1] = MOTION_CONNECTOR;
    pointEquals(copy.points[1], (MotionPoint){2, 0});
    assert(copy.kinds[1] == MOTION_OUTLINE);
    MotionFree(&source);
    assert(MotionValid(&copy));
    MotionFree(&copy);
    assert(!copy.points && !copy.kinds && !copy.count && !copy.width && !copy.height);
    assert(copy.width_mm == 0 && copy.height_mm == 0);
    MotionFree(&copy);
    MotionFree(NULL);
}

static void validation(void)
{
    assert(!MotionValid(NULL));
    MotionPath path = {0};
    assert(!MotionValid(&path));
    assert(MotionPlanSegments(&path, NULL, 0, 10, 10, (MotionPoint){10, 10}));
    assert(MotionValid(&path) && path.count == 1 && path.kinds[0] == MOTION_START);
    pointEquals(path.points[0], (MotionPoint){10, 10});
    MotionPath invalid = path;
    invalid.width = 0; assert(!MotionValid(&invalid));
    invalid = path; invalid.width_mm = NAN; assert(!MotionValid(&invalid));
    invalid = path; invalid.height_mm = -1; assert(!MotionValid(&invalid));
    path.points[0].x = 10.01; assert(!MotionValid(&path));
    path.points[0].x = NAN; assert(!MotionValid(&path));
    path.points[0] = (MotionPoint){10, 10};
    path.kinds[0] = MOTION_OUTLINE; assert(!MotionValid(&path));
    path.kinds[0] = MOTION_START; assert(MotionValid(&path));
    MotionFree(&path);
    MotionSegment point = {{5, 5}, {5, 5}};
    assert(MotionPlanSegments(&path, &point, 1, 10, 10, (MotionPoint){10, 10}));
    assert(MotionValid(&path) && path.count == 1);
    pointEquals(path.points[0], (MotionPoint){10, 10});
    MotionFree(&path);
    assert(MotionPlanSegments(&path, fixture, 3, 10, 10, (MotionPoint){0, 0}));
    path.kinds[1] = 99; assert(!MotionValid(&path));
    path.kinds[1] = MOTION_START; assert(!MotionValid(&path));
    path.kinds[1] = MOTION_OUTLINE; assert(MotionValid(&path));
    MotionFree(&path);
}

static void failedPlansPreserveOutput(void)
{
    MotionPath path = {0}, saved = {0};
    assert(MotionPlanSegments(&path, fixture, 3, 10, 10, (MotionPoint){0, 0}));
    assert(MotionCopy(&saved, &path));
    MotionPoint *points = path.points;
    unsigned char *kinds = path.kinds;
    const MotionSegment bad[] = {
        {{-1, 0}, {0, 0}}, {{0, 0}, {11, 0}}, {{0, NAN}, {0, 0}},
        {{0, 0}, {0, INFINITY}}
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(*bad); ++i)
        assert(!MotionPlanSegments(&path, &bad[i], 1, 10, 10, (MotionPoint){0, 0}));
    assert(!MotionPlanSegments(&path, NULL, 1, 10, 10, (MotionPoint){0, 0}));
    assert(!MotionPlanSegments(&path, fixture, 3, 0, 10, (MotionPoint){0, 0}));
    assert(!MotionPlanSegments(&path, fixture, 3, 10, 10, (MotionPoint){-1, 0}));
    assert(!MotionPlanSegments(&path, fixture, 3, 10, 10, (MotionPoint){0, 11}));
    assert(!MotionPlanSegments(&path, fixture, 3, 10, 10, (MotionPoint){NAN, 0}));
    assert(path.points == points && path.kinds == kinds && path.count == saved.count);
    assert(path.width == saved.width && path.height == saved.height);
    assert(path.width_mm == saved.width_mm && path.height_mm == saved.height_mm);
    for (size_t i = 0; i < path.count; ++i) {
        pointEquals(path.points[i], saved.points[i]);
        assert(path.kinds[i] == saved.kinds[i]);
    }
    assert(MotionValid(&path));
    MotionFree(&saved);
    MotionFree(&path);
}

static void failedCopyPreservesOutput(void)
{
    MotionPath path = {0}, invalid = {0};
    assert(MotionPlanSegments(&path, fixture, 3, 10, 10, (MotionPoint){0, 0}));
    MotionPoint *points = path.points;
    unsigned char *kinds = path.kinds;
    assert(!MotionCopy(&path, &invalid));
    assert(!MotionCopy(&path, NULL));
    assert(!MotionCopy(NULL, &path));
    assert(path.points == points && path.kinds == kinds && path.count == 8);
    assert(MotionValid(&path) && near(lengthMM(&path), 289.25));
    MotionFree(&path);
}

void MotionWriterTests(void);
void MotionTimelineTests(void);

int main(void)
{
    MotionWriterTests();
    MotionTimelineTests();
    disconnectedRoute();
    duplicateAndStraightEdges();
    copyAndFree();
    validation();
    failedPlansPreserveOutput();
    failedCopyPreservesOutput();
    return 0;
}
