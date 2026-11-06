// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "r_etch.h"

enum { CAPTURE_WIDTH = 320, CAPTURE_HEIGHT = 200 };
enum { MAX_SEGMENTS = 400000 };
typedef struct {
    double top, bottom, top_end, bottom_end;
    int valid;
} WallSample;
static WallSample samples[3][CAPTURE_WIDTH];
static double clip_top[CAPTURE_WIDTH], clip_bottom[CAPTURE_WIDTH];
static double clip_top_step[CAPTURE_WIDTH], clip_bottom_step[CAPTURE_WIDTH];
static MotionSegment *walls;
static size_t wall_count, wall_capacity;
static int failed, wall_open;
static int active;
static int width, height;
int R_EtchBegin(int w, int h)
{
    if (active || w <= 0 || w > CAPTURE_WIDTH || h <= 0 || h > CAPTURE_HEIGHT)
        return 0;
    width = w; height = h;
    active = 1;
    R_EtchFrameBegin(w, h);
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
