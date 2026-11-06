// SPDX-License-Identifier: GPL-2.0-or-later
#include "timeline.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

_Static_assert(sizeof(MotionTimedSegment) == 32, "Timeline records must occupy 32 bytes");

static int timed_segment(const MotionPath *path, size_t index, double speed,
                          double *elapsed, MotionTimedSegment *segment)
{
    MotionPoint a = path->points[index], b = path->points[index + 1];
    // Normalize before physical scaling to avoid overflowing source-unit products.
    double dx = ((b.x - a.x) / path->width) * path->width_mm;
    double dy = ((b.y - a.y) / path->height) * path->height_mm;
    double next = *elapsed + hypot(dx, dy) / speed;
    if (!isfinite(next) || next > FLT_MAX) return 0;
    *segment = (MotionTimedSegment){
        {(float)(a.x / path->width), (float)(a.y / path->height),
         (float)(b.x / path->width), (float)(b.y / path->height)},
        {(float)*elapsed, (float)next, 0, 0}
    };
    *elapsed = next;
    return 1;
}

int MotionTimelineBuild(const MotionPath *path, double speed,
    MotionTimedSegment **out, size_t *count, double *duration)
{
    if (out) *out = NULL;
    if (count) *count = 0;
    if (duration) *duration = 0;
    if (!out || !count || !duration || !path || !path->count
        || path->count > SIZE_MAX / sizeof(*path->points)
        || path->count - 1 > SIZE_MAX / sizeof(MotionTimedSegment)
        || !isfinite(speed) || speed <= 0 || !MotionValid(path)) return 0;
    size_t length = path->count - 1;
    MotionTimedSegment *segments = length ? calloc(length, sizeof(*segments)) : NULL;
    if (length && !segments) return 0;
    double elapsed = 0;
    for (size_t i = 0; i < length; i++) {
        if (!timed_segment(path, i, speed, &elapsed, &segments[i])) {
            free(segments);
            return 0;
        }
    }
    *out = segments; *count = length; *duration = elapsed;
    return 1;
}
