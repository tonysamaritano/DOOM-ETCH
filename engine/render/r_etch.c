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
    int success = 0;
    if (path && !failed && !wall_open)
        success = MotionPlanSegments(path, walls, wall_count,
            (unsigned)width, (unsigned)height, (MotionPoint){0, 0});
    free(walls);
    walls = NULL;
    wall_count = wall_capacity = 0;
    return success;
}

void R_EtchFrameBegin(int w, int h)
{
    if (!active) return;
    failed = 0; wall_open = 0; wall_count = 0;
    if (w <= 0 || w > CAPTURE_WIDTH || h <= 0 || h > CAPTURE_HEIGHT) {
        failed = 1;
        return;
    }
    width = w; height = h;
    memset(samples, 0, sizeof(samples));
    for (int x = 0; x < width; x++) {
        clip_top[x] = clip_top_step[x] = clip_bottom_step[x] = 0;
        clip_bottom[x] = height;
    }
}

void R_EtchWallBegin(void)
{
    if (!active || failed) return;
    if (wall_open) { failed = 1; return; }
    memset(samples, 0, sizeof(samples));
    wall_open = 1;
}

static double clamp(double value, double low, double high)
{
    return fmin(high, fmax(low, value));
}

void R_EtchClipPlane(int x, double top, double topstep, double bottom, double bottomstep)
{
    if (!active || failed || x < 0 || x >= width) return;
    if (!isfinite(top) || !isfinite(bottom) || !isfinite(topstep) || !isfinite(bottomstep)
        || !isfinite(top + topstep) || !isfinite(bottom + bottomstep)) {
        failed = 1;
        return;
    }
    if (top >= clip_top[x]) {
        if (top <= clip_bottom[x]) {
            clip_top[x] = top; clip_top_step[x] = topstep;
        } else {
            clip_top[x] = clip_bottom[x]; clip_top_step[x] = clip_bottom_step[x];
        }
    }
    if (bottom <= clip_bottom[x]) {
        if (bottom >= clip_top[x]) {
            clip_bottom[x] = bottom; clip_bottom_step[x] = bottomstep;
        } else {
            clip_bottom[x] = clip_top[x]; clip_bottom_step[x] = clip_top_step[x];
        }
    }
}
