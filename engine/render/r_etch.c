// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdlib.h>
#include <string.h>
#include "r_etch.h"

enum { CAPTURE_WIDTH = 320, CAPTURE_HEIGHT = 200 };
static int cells[CAPTURE_WIDTH * CAPTURE_HEIGHT];
static int active;
static int width, height;
int R_EtchBegin(int w, int h)
{
    if (active || w <= 0 || w > CAPTURE_WIDTH || h <= 0 || h > CAPTURE_HEIGHT)
        return 0;
    width = w; height = h;
    memset(cells, 0, (size_t)width * height * sizeof(*cells));
    active = 1;
    return 1;
}

int R_EtchBuildMotion(MotionPath *path)
{
    if (!active) return 0;
    active = 0;
    if (!path) return 0;
    size_t count = boundary_segments(NULL);
    // At most (width+1)*height + (height+1)*width segments fit this map.
    MotionSegment *segments = count ? malloc(count * sizeof(*segments)) : NULL;
    if (count && !segments) return 0;
    boundary_segments(segments);
    int success = MotionPlanSegments(path, segments, count,
        (unsigned)width, (unsigned)height, (MotionPoint){0, 0});
    free(segments);
    return success;
}
