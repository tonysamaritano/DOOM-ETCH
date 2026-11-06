#ifndef DOOM_ETCH_MOTION_H
#define DOOM_ETCH_MOTION_H

#include <stddef.h>
#include <stdio.h>

#define MOTION_WIDTH_MM 170.15
#define MOTION_HEIGHT_MM 127.6

typedef struct { double x, y; } MotionPoint;
typedef struct { MotionPoint a, b; } MotionSegment;
enum MotionKind { MOTION_START, MOTION_OUTLINE, MOTION_CONNECTOR };

/* Origin is top left. A move ends at points[i] and begins at points[i - 1].
 * kinds[0] is MOTION_START. Initialize owned paths to zero before first use. */
typedef struct {
    MotionPoint *points;
    unsigned char *kinds;
    size_t count;
    unsigned width, height;
    double width_mm, height_mm;
} MotionPath;

int MotionPlanSegments(MotionPath *path, const MotionSegment *segments, size_t count,
                       unsigned width, unsigned height, MotionPoint start);
int MotionValid(const MotionPath *path);
int MotionCopy(MotionPath *destination, const MotionPath *source);
void MotionFree(MotionPath *path);
int MotionWriteSVG(FILE *stream, const MotionPath *path);
int MotionWriteGCode(FILE *stream, const MotionPath *path, double feed_mm_min);

#endif
