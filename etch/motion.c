#include "motion.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int in_bounds(MotionPoint point, unsigned width, unsigned height)
{
    return isfinite(point.x) && isfinite(point.y) && point.x >= 0 && point.y >= 0
        && point.x <= width && point.y <= height;
}

int MotionValid(const MotionPath *path)
{
    if (!path || !path->count || !path->points || !path->kinds ||
        !path->width || !path->height || path->kinds[0] != MOTION_START ||
        !isfinite(path->width_mm) || !isfinite(path->height_mm) ||
        path->width_mm <= 0 || path->height_mm <= 0)
        return 0;
    for (size_t i = 0; i < path->count; i++) {
        if (!in_bounds(path->points[i], path->width, path->height))
            return 0;
        if (i && path->kinds[i] != MOTION_OUTLINE && path->kinds[i] != MOTION_CONNECTOR)
            return 0;
    }
    return 1;
}

void MotionFree(MotionPath *path)
{
    if (path) {
        free(path->points);
        free(path->kinds);
        *path = (MotionPath){0};
    }
}
