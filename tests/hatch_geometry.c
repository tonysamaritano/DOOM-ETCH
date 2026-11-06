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

static void patternsAndPhysicalSpacing(void)
{
    Silhouette shape = ring();
    SilRects fill = {0}; SilLines boundary = {0};
    project(&shape, 0.5, &fill, &boundary); /* Narrow X scale exposes pixel-spacing errors. */
    for (int pattern = 0; pattern < HATCH_PATTERN_COUNT; ++pattern) {
        SilLines ink = {0};
        assert(SilHatch(&shape, &fill, 20, 10, 0.5, 1,
                        light, palette, translation, pattern, &ink));
        respectsOpacity(&ink, &fill); physicalSpacing(&ink, 1.914);
        unsigned positive = 0, negative = 0;
        for (size_t i = 0; i < ink.count; ++i) {
            MotionSegment line = ink.data[i];
            double slope = (line.b.y - line.a.y) / (line.b.x - line.a.x);
            positive += slope > 1e-8; negative += slope < -1e-8;
            if (pattern == HATCH_HORIZONTAL) assert(fabs(slope) < 1e-8);
        }
        if (pattern == HATCH_DIAGONAL) assert(positive && !negative);
        if (pattern == HATCH_CROSS) assert(positive && negative);
        SilLinesFree(&ink);
    }
    SilRectsFree(&fill); SilLinesFree(&boundary); SilhouetteFree(&shape);
}

static size_t hatchCount(const Silhouette *shape, const SilRects *fill)
{
    SilLines ink = {0};
    assert(SilHatch(shape, fill, 20, 10, 1, 1, light, palette, translation,
                    HATCH_HORIZONTAL, &ink));
    size_t count = ink.count; SilLinesFree(&ink); return count;
}
static void mappedBrightness(void)
{
    Silhouette shape = ring(); SilRects fill = {0}; SilLines boundary = {0};
    project(&shape, 1, &fill, &boundary);
    assert(hatchCount(&shape, &fill) > 0);
    light[0] = 255; assert(hatchCount(&shape, &fill) == 0);
    light[0] = 0; translation[0] = 50; light[50] = 255;
    assert(hatchCount(&shape, &fill) == 0); /* Translation precedes colormap lighting. */
    translation[0] = 0; assert(hatchCount(&shape, &fill) > 0);
    assert(shape.texels[0] == 0 && shape.texels[16 * 32 + 16] == 256);
    SilRectsFree(&fill); SilLinesFree(&boundary); SilhouetteFree(&shape);
}
static double averageX(const SilLines *ink)
{
    assert(ink->count); double sum = 0;
    for (size_t i = 0; i < ink->count; ++i)
        sum += (ink->data[i].a.x + ink->data[i].b.x) * 0.5;
    return sum / ink->count;
}

static void flippedTextureAndDenseSpacing(void)
{
    Silhouette shape = ring();
    for (unsigned y = 0; y < 32; ++y)
        for (unsigned x = 16; x < 32; ++x)
            if (shape.texels[y * 32 + x] != 256) shape.texels[y * 32 + x] = 255;
    SilRects fill = {0}; SilLines boundary = {0}, normal = {0}, flipped = {0}, dense = {0};
    project(&shape, 1, &fill, &boundary);
    assert(SilHatch(&shape, &fill, 20, 10, 1, 1, light, palette, NULL, 0, &normal));
    assert(averageX(&normal) < 36);
    assert(SilHatchDetail(&shape, &fill, 20, 10, 1, 1, light, palette, NULL,
                          0, 0.7975, &dense));
    respectsOpacity(&dense, &fill); physicalSpacing(&dense, 0.7975);
    assert(dense.count > normal.count);
    project(&shape, -1, &fill, &boundary);
    assert(SilHatch(&shape, &fill, 52, 10, -1, 1, light, palette, NULL, 0, &flipped));
    assert(averageX(&flipped) > 36 && flipped.count == normal.count);
    respectsOpacity(&flipped, &fill);
    SilLinesFree(&normal); SilLinesFree(&flipped); SilLinesFree(&dense);
    SilRectsFree(&fill); SilLinesFree(&boundary); SilhouetteFree(&shape);
}

static void clipAndFailureCases(void)
{
    double low, high;
    assert(HatchClip(-1, 3, 0, 0, 2, 2, &low, &high));
    assert(fabs(low - 0.25) < 1e-9 && fabs(high - 0.75) < 1e-9);
    assert(HatchClip(1, 1, 0, 2, 2, 4, &low, &high));
    assert(fabs(low) < 1e-9 && fabs(high - 0.5) < 1e-9);
    assert(!HatchClip(5, 5, 0, 0, 2, 2, &low, &high));
    Silhouette shape = ring(); SilRects fill = {0}; SilLines boundary = {0}, ink = {0};
    project(&shape, 1, &fill, &boundary);
    assert(SilLineAdd(&ink, (MotionPoint){1, 2}, (MotionPoint){3, 4}));
    MotionSegment *owned = ink.data;
    assert(!SilHatch(&shape, &fill, 20, 10, 1, 1, light, palette, NULL, 3, &ink));
    assert(!SilHatchDetail(&shape, &fill, 20, 10, 1, 1, light, palette, NULL, 0, 0, &ink));
    assert(!SilHatchDetail(&shape, &fill, 20, 10, 1, 1, light, palette, NULL, 0, NAN, &ink));
    assert(ink.data == owned && ink.count == 1 && ink.data[0].a.x == 1);
    light[0] = 255;
    assert(SilHatch(&shape, &fill, 20, 10, 1, 1, light, palette, NULL, 0, &ink));
    assert(ink.count == 1); /* Bright success appends nothing and retains previous strokes. */
    SilLinesFree(&ink); SilRectsFree(&fill); SilLinesFree(&boundary); SilhouetteFree(&shape);
}

int main(void)
{
    patternsAndPhysicalSpacing(); mappedBrightness();
    flippedTextureAndDenseSpacing(); clipAndFailureCases();
    puts("Hatch geometry: mapped brightness, opacity, flips and physical spacing passed");
    return 0;
}
