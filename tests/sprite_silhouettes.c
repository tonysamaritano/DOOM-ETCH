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

static void expectedContour(const SilLines *actual, const MotionSegment *sides, size_t count)
{
    double covered[8] = {0}; assert(count <= 8);
    for (size_t i = 0; i < actual->count; ++i) {
        MotionSegment line = actual->data[i];
        size_t side;
        for (side = 0; side < count; ++side)
            if (onLine(line.a, sides[side]) && onLine(line.b, sides[side])) break;
        assert(side < count);
        covered[side] += lineLength(line.a, line.b);
    }
    for (size_t side = 0; side < count; ++side)
        assert(fabs(covered[side] - lineLength(sides[side].a, sides[side].b)) < 1e-8);
}
static void decodeHole(void)
{
    unsigned char patch[128]; size_t bytes = ringPatch(patch);
    Silhouette shape = {0}; assert(SilhouetteDecode(&shape, patch, bytes));
    assert(shape.width == 3 && shape.height == 3);
    assert(fabs(area(&shape.fill) - 8) < 1e-9);
    assert(filled(&shape.fill, 0.5, 1.5));
    assert(!filled(&shape.fill, 1.5, 1.5));
    const MotionSegment contour[] = {
        {{0, 0}, {3, 0}}, {{3, 0}, {3, 3}}, {{3, 3}, {0, 3}}, {{0, 3}, {0, 0}},
        {{1, 1}, {2, 1}}, {{2, 1}, {2, 2}}, {{2, 2}, {1, 2}}, {{1, 2}, {1, 1}}
    };
    expectedContour(&shape.outline, contour, 8);
    SilhouetteFree(&shape); SilhouetteFree(&shape);
    assert(!shape.width && !shape.height && !shape.fill.data && !shape.outline.data);
}

static size_t asymmetricPatch(unsigned char patch[128])
{
    memset(patch, 0, 128); patch[0] = 2; patch[2] = 3;
    little32(patch + 8, 16);
    size_t at = post(patch, 16, 0, 3); patch[at++] = 255;
    little32(patch + 12, (unsigned)at);
    at = post(patch, at, 0, 1); patch[at++] = 255;
    return at;
}
static void flippedProjection(void)
{
    unsigned char patch[128]; size_t bytes = asymmetricPatch(patch);
    Silhouette shape = {0}; assert(SilhouetteDecode(&shape, patch, bytes));
    short top[16], bottom[16];
    for (int x = 0; x < 16; ++x) { top[x] = -1; bottom[x] = 8; }
    SilRects fill = {0}; SilLines lines = {0};
    assert(SilProject(&shape, 5, 1, 2, 1, 0, 15, top, bottom, 16, 8, &fill, &lines));
    assert(fabs(area(&fill) - 8) < 1e-9);
    assert(filled(&fill, 6, 3) && !filled(&fill, 8, 3));
    assert(SilProject(&shape, 9, 1, -2, 1, 0, 15, top, bottom, 16, 8, &fill, &lines));
    assert(fabs(area(&fill) - 8) < 1e-9);
    assert(!filled(&fill, 6, 3) && filled(&fill, 8, 3));
    SilRectsFree(&fill); SilLinesFree(&lines); SilhouetteFree(&shape);
}

static void wallClippedProjection(void)
{
    unsigned char patch[128]; size_t bytes = asymmetricPatch(patch);
    Silhouette shape = {0}; assert(SilhouetteDecode(&shape, patch, bytes));
    short top[16], bottom[16];
    for (int x = 0; x < 16; ++x) { top[x] = 1; bottom[x] = 3; }
    SilRects fill = {0}; SilLines lines = {0};
    assert(SilProject(&shape, 5, 1, 2, 1, 0, 15, top, bottom, 16, 8, &fill, &lines));
    const MotionSegment rectangle[] = {
        {{5, 2}, {7, 2}}, {{7, 2}, {7, 3}}, {{7, 3}, {5, 3}}, {{5, 3}, {5, 2}}
    };
    assert(fabs(area(&fill) - 2) < 1e-9);
    expectedContour(&lines, rectangle, 4);
    top[5] = 2; /* First projected column is fully hidden, second remains. */
    assert(SilProject(&shape, 5, 1, 2, 1, 0, 15, top, bottom, 16, 8, &fill, &lines));
    assert(fabs(area(&fill) - 1) < 1e-9);
    assert(filled(&fill, 6.5, 2.5) && !filled(&fill, 5.5, 2.5));
    assert(SilProject(&shape, 5, 1, 2, 1, 7, 7, top, bottom, 16, 8, &fill, &lines));
    assert(!fill.count && !lines.count); /* Inclusive X range lies in transparent cut. */
    assert(SilProject(&shape, -100, 1, 1, 1, 0, 15, top, bottom, 16, 8, &fill, &lines));
    assert(!fill.count && !lines.count);
    SilRectsFree(&fill); SilLinesFree(&lines); SilhouetteFree(&shape);
}

static void holeRevealsEarlierLine(void)
{
    unsigned char patch[128]; size_t bytes = ringPatch(patch);
    Silhouette shape = {0}; assert(SilhouetteDecode(&shape, patch, bytes));
    SilLines scene = {0}, noOutline = {0};
    assert(SilLineAdd(&scene, (MotionPoint){-1, 1.5}, (MotionPoint){4, 1.5}));
    assert(SilComposite(&scene, &shape.fill, &noOutline));
    const MotionSegment visible[] = {
        {{-1, 1.5}, {0, 1.5}}, {{1, 1.5}, {2, 1.5}}, {{3, 1.5}, {4, 1.5}}
    };
    expectedContour(&scene, visible, 3);
    assert(hasLineAt(&scene, 1.5, 1.5));
    assert(!hasLineAt(&scene, 0.5, 1.5) && !hasLineAt(&scene, 2.5, 1.5));
    SilLinesFree(&scene); SilhouetteFree(&shape);
}
static void overlappingMasks(void)
{
    SilRects mask = {0}; SilLines scene = {0}, noOutline = {0};
    assert(SilRectAdd(&mask, (SilRect){0, 0, 3, 2}));
    assert(SilRectAdd(&mask, (SilRect){2, 0, 5, 2}));
    assert(SilLineAdd(&scene, (MotionPoint){-1, 1}, (MotionPoint){6, 1}));
    assert(SilComposite(&scene, &mask, &noOutline));
    const MotionSegment visible[] = {{{-1, 1}, {0, 1}}, {{5, 1}, {6, 1}}};
    expectedContour(&scene, visible, 2);
    SilRectsFree(&mask); SilLinesFree(&scene);
}

static void painterOrder(void)
{
    SilRects back = {0}, front = {0}; SilLines backLines = {0}, frontLines = {0};
    assert(SilRectAdd(&back, (SilRect){0, 0, 4, 4}));
    assert(SilRectAdd(&front, (SilRect){2, -1, 5, 5}));
    assert(SilContours(&back, &backLines) && SilContours(&front, &frontLines));
    SilLines forward = {0}, reverse = {0};
    assert(SilComposite(&forward, &back, &backLines));
    assert(SilComposite(&forward, &front, &frontLines));
    assert(!hasLineAt(&forward, 4, 2));
    assert(hasLineAt(&forward, 2, 2) && hasLineAt(&forward, 1, 0));
    assert(SilComposite(&reverse, &front, &frontLines));
    assert(SilComposite(&reverse, &back, &backLines));
    assert(hasLineAt(&reverse, 4, 2));
    assert(!hasLineAt(&reverse, 2, 2));
    SilRectsFree(&back); SilRectsFree(&front);
    SilLinesFree(&backLines); SilLinesFree(&frontLines);
    SilLinesFree(&forward); SilLinesFree(&reverse);
}
static void rejectDecode(Silhouette *shape, const void *patch, size_t bytes)
{
    Silhouette saved = *shape;
    assert(!SilhouetteDecode(shape, patch, bytes));
    assert(shape->fill.data == saved.fill.data && shape->outline.data == saved.outline.data);
    assert(shape->fill.count == saved.fill.count && shape->outline.count == saved.outline.count);
    assert(shape->width == 3 && shape->height == 3 && fabs(area(&shape->fill) - 8) < 1e-9);
}

static void malformedPatches(void)
{
    unsigned char patch[128], bad[128]; size_t bytes = ringPatch(patch);
    Silhouette shape = {0}; assert(SilhouetteDecode(&shape, patch, bytes));
    rejectDecode(&shape, NULL, bytes);
    rejectDecode(&shape, patch, 7);
    rejectDecode(&shape, patch, bytes - 1); /* Missing final column terminator. */
    memcpy(bad, patch, sizeof(bad)); little32(bad + 8, UINT32_MAX);
    rejectDecode(&shape, bad, bytes);
    memcpy(bad, patch, sizeof(bad)); little32(bad + 8, 0);
    rejectDecode(&shape, bad, bytes); /* A column offset cannot point into the header. */
    memcpy(bad, patch, sizeof(bad)); bad[21] = 4;
    rejectDecode(&shape, bad, bytes); /* Post reaches below declared patch height. */
    rejectDecode(&shape, patch, 24); /* Header exists, first post payload is truncated. */
    memcpy(bad, patch, sizeof(bad)); bad[0] = 0;
    rejectDecode(&shape, bad, bytes);
    memcpy(bad, patch, sizeof(bad)); bad[0] = 1; bad[1] = 16;
    rejectDecode(&shape, bad, bytes); /* 4097 columns exceed the bounded decoder. */
    SilhouetteFree(&shape);
}

static void failedProjectionPreservesGeometry(void)
{
    unsigned char patch[128]; size_t bytes = asymmetricPatch(patch);
    Silhouette shape = {0}; assert(SilhouetteDecode(&shape, patch, bytes));
    short top[16], bottom[16];
    for (int x = 0; x < 16; ++x) { top[x] = -1; bottom[x] = 8; }
    SilRects fill = {0}; SilLines lines = {0};
    assert(SilProject(&shape, 5, 1, 2, 1, 0, 15, top, bottom, 16, 8, &fill, &lines));
    SilRects savedFill = fill; SilLines savedLines = lines;
    assert(!SilProject(&shape, 0, 0, NAN, 1, 0, 15, top, bottom, 16, 8, &fill, &lines));
    assert(!SilProject(&shape, 0, 0, 0, 1, 0, 15, top, bottom, 16, 8, &fill, &lines));
    assert(!SilProject(&shape, 0, 0, 1, -1, 0, 15, top, bottom, 16, 8, &fill, &lines));
    assert(!SilProject(&shape, 0, 0, 1, 1, 0, 15, NULL, bottom, 16, 8, &fill, &lines));
    assert(fill.data == savedFill.data && fill.count == savedFill.count);
    assert(lines.data == savedLines.data && lines.count == savedLines.count);
    assert(fabs(area(&fill) - 8) < 1e-9);
    SilRectsFree(&fill); SilLinesFree(&lines); SilhouetteFree(&shape);
}
static void transparentPatch(void)
{
    unsigned char patch[18] = {2, 0, 3, 0};
    little32(patch + 8, 16); little32(patch + 12, 17);
    patch[16] = patch[17] = 255;
    Silhouette shape = {0};
    assert(SilhouetteDecode(&shape, patch, sizeof(patch)));
    assert(shape.width == 2 && shape.height == 3);
    assert(!shape.fill.count && !shape.outline.count);
    SilhouetteFree(&shape);
}

int main(void)
{
    decodeHole(); flippedProjection(); wallClippedProjection();
    holeRevealsEarlierLine(); overlappingMasks(); painterOrder();
    malformedPatches(); failedProjectionPreservesGeometry(); transparentPatch();
    puts("Sprite silhouettes: holes, projection, clipping, overlap and ownership passed");
    return 0;
}
