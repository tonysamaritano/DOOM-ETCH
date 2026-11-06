#include "silhouette.h"
#include "hatch.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char palette[768], light[256], translation[256];
static Silhouette ring(void)
{
    unsigned char patch[2048] = {32, 0, 32, 0};
    size_t at = 136;
    for (int x = 0; x < 32; ++x) {
        for (int byte = 0; byte < 4; ++byte)
            patch[8 + x * 4 + byte] = (unsigned char)(at >> (8 * byte));
        int posts = x >= 12 && x < 20 ? 2 : 1;
        for (int run = 0; run < posts; ++run) {
            int length = posts == 1 ? 32 : 12;
            patch[at++] = (unsigned char)(run ? 20 : 0);
            patch[at++] = (unsigned char)length; patch[at++] = 0;
            at += (size_t)length; patch[at++] = 0; /* Zero-valued opaque texels. */
        }
        patch[at++] = 255;
    }
    assert(at <= sizeof(patch));
    Silhouette shape = {0}; assert(SilhouetteDecode(&shape, patch, at));
    for (int i = 0; i < 256; ++i) {
        light[i] = translation[i] = (unsigned char)i;
        palette[i * 3] = palette[i * 3 + 1] = palette[i * 3 + 2] = (unsigned char)i;
    }
    return shape;
}

static void project(const Silhouette *shape, double sx, SilRects *fill, SilLines *outline)
{
    short top[320], bottom[320];
    for (int x = 0; x < 320; ++x) { top[x] = -1; bottom[x] = 200; }
    assert(SilProject(shape, sx > 0 ? 20 : 52, 10, sx, 1,
                      0, 319, top, bottom, 320, 200, fill, outline));
}
static int inside(const SilRects *fill, double x, double y)
{
    for (size_t i = 0; i < fill->count; ++i) {
        SilRect r = fill->data[i];
        if (x >= r.x0 - 1e-8 && x <= r.x1 + 1e-8 &&
            y >= r.y0 - 1e-8 && y <= r.y1 + 1e-8) return 1;
    }
    return 0;
}
static void respectsOpacity(const SilLines *strokes, const SilRects *fill)
{
    assert(strokes->count);
    for (size_t i = 0; i < strokes->count; ++i) {
        MotionSegment line = strokes->data[i];
        for (int sample = 0; sample <= 32; ++sample) {
            double t = sample / 32.0;
            assert(inside(fill, line.a.x + (line.b.x - line.a.x) * t,
                          line.a.y + (line.b.y - line.a.y) * t));
        }
    }
}

static int compareDouble(const void *left, const void *right)
{
    double a = *(const double *)left, b = *(const double *)right;
    return (a > b) - (a < b);
}
static void physicalSpacing(const SilLines *lines, double minimum)
{
    double intercept[3][2048], slopes[3] = {0}; size_t count[3] = {0};
    for (size_t i = 0; i < lines->count; ++i) {
        MotionSegment line = lines->data[i];
        double dx = (line.b.x - line.a.x) * (170.15 / 320);
        double dy = (line.b.y - line.a.y) * (127.6 / 200);
        assert(fabs(dx) > 1e-9);
        int family = fabs(dy) < 1e-9 ? 0 : dy / dx > 0 ? 1 : 2;
        double slope = dy / dx;
        if (count[family]) assert(fabs(slope - slopes[family]) < 1e-8);
        slopes[family] = slope;
        assert(count[family] < 2048);
        intercept[family][count[family]++] = line.a.y * (127.6 / 200)
                                           - slope * line.a.x * (170.15 / 320);
    }
    for (int family = 0; family < 3; ++family) {
        qsort(intercept[family], count[family], sizeof(double), compareDouble);
        unsigned distinct = 0;
        for (size_t i = 1; i < count[family]; ++i) {
            double gap = intercept[family][i] - intercept[family][i - 1];
            if (gap < 1e-8) continue; /* Fragmented strokes can share one hatch row. */
            assert(gap / hypot(1, slopes[family]) >= minimum - 1e-8); ++distinct;
        }
        if (count[family]) assert(distinct > 0);
    }
}
