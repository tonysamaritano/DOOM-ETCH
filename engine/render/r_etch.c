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

void R_EtchColumn(int x, int top, int bottom, int surface)
{
    if (!active || x < 0 || x >= width || surface <= 0) return;
    if (top < 0) top = 0;
    if (bottom >= height) bottom = height - 1;
    // BSP rendering reaches the nearest visible wall first.
    for (int y = top; y <= bottom; y++)
        if (!cells[y * width + x]) cells[y * width + x] = surface;
}

static int surface_at(int x, int y)
{
    if (x < 0 || x >= width || y < 0 || y >= height) return 0;
    return cells[y * width + x];
}

/* Count first, then fill the same bounded boundary list without reallocations. */
static size_t boundary_segments(MotionSegment *segments)
{
    size_t count = 0;
    for (int y = 0; y <= height; y++) {
        for (int x = 0; x <= width; x++) {
            if (x < width && surface_at(x, y - 1) != surface_at(x, y)) {
                if (segments)
                    segments[count] = (MotionSegment){{x, y}, {x + 1, y}};
                count++;
            }
            if (y < height && surface_at(x - 1, y) != surface_at(x, y)) {
                if (segments)
                    segments[count] = (MotionSegment){{x, y}, {x, y + 1}};
                count++;
            }
        }
    }
    return count;
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
