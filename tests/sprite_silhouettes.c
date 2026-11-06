#include "silhouette.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void little32(unsigned char *bytes, unsigned value)
{
    for (int i = 0; i < 4; ++i) bytes[i] = (unsigned char)(value >> (8 * i));
}
static size_t post(unsigned char *patch, size_t at, int top, int length)
{
    patch[at++] = (unsigned char)top;
    patch[at++] = (unsigned char)length;
    patch[at++] = 0;
    for (int i = 0; i < length; ++i) patch[at++] = 0; /* Palette zero is opaque. */
    patch[at++] = 0;
    return at;
}
static size_t ringPatch(unsigned char patch[128])
{
    memset(patch, 0, 128); patch[0] = patch[2] = 3;
    size_t at = 20;
    for (int x = 0; x < 3; ++x) {
        little32(patch + 8 + x * 4, (unsigned)at);
        if (x == 1) { at = post(patch, at, 0, 1); at = post(patch, at, 2, 1); }
        else at = post(patch, at, 0, 3);
        patch[at++] = 255;
    }
    return at;
}

static double area(const SilRects *rectangles)
{
    double sum = 0;
    for (size_t i = 0; i < rectangles->count; ++i) {
        SilRect r = rectangles->data[i];
        assert(r.x1 > r.x0 && r.y1 > r.y0);
        sum += (r.x1 - r.x0) * (r.y1 - r.y0);
    }
    return sum;
}
static int filled(const SilRects *rectangles, double x, double y)
{
    for (size_t i = 0; i < rectangles->count; ++i) {
        SilRect r = rectangles->data[i];
        if (x > r.x0 && x < r.x1 && y > r.y0 && y < r.y1) return 1;
    }
    return 0;
}
static double lineLength(MotionPoint a, MotionPoint b)
{ return hypot(a.x - b.x, a.y - b.y); }
static int onLine(MotionPoint point, MotionSegment line)
{
    return fabs(lineLength(line.a, point) + lineLength(point, line.b)
                - lineLength(line.a, line.b)) < 1e-8;
}
static int hasLineAt(const SilLines *lines, double x, double y)
{
    for (size_t i = 0; i < lines->count; ++i)
        if (onLine((MotionPoint){x, y}, lines->data[i])) return 1;
    return 0;
}
