// SPDX-License-Identifier: GPL-2.0-or-later
#include "silhouette.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_ITEMS = 400000, MAX_PATCH_EXTENT = 4096 };
static int coordinate(double value) { return isfinite(value) && fabs(value) <= 1e9; }
static int rectangle_valid(SilRect r)
{
    return coordinate(r.x0) && coordinate(r.y0) && coordinate(r.x1) && coordinate(r.y1);
}
static int collection_valid(const void *data, size_t count, size_t capacity)
{
    return count <= capacity && capacity <= MAX_ITEMS && (!capacity || data);
}
static void *reserve(void *data, size_t *capacity, size_t count, size_t stride)
{
    if (count < *capacity) return data;
    if (count >= MAX_ITEMS || stride > SIZE_MAX / MAX_ITEMS) return NULL;
    size_t next = *capacity ? *capacity * 2 : 64;
    if (next > MAX_ITEMS) next = MAX_ITEMS;
    void *allocation = realloc(data, next * stride);
    if (allocation) *capacity = next;
    return allocation;
}

void SilRectsFree(SilRects *rectangles)
{
    if (rectangles) { free(rectangles->data); *rectangles = (SilRects){0}; }
}
void SilLinesFree(SilLines *lines)
{
    if (lines) { free(lines->data); *lines = (SilLines){0}; }
}
void SilhouetteFree(Silhouette *shape)
{
    if (!shape) return;
    SilRectsFree(&shape->fill); SilLinesFree(&shape->outline);
    *shape = (Silhouette){0};
}
int SilRectAdd(SilRects *rectangles, SilRect rectangle)
{
    if (!rectangles || !collection_valid(rectangles->data, rectangles->count, rectangles->capacity)
        || !rectangle_valid(rectangle)) return 0;
    if (rectangle.x0 >= rectangle.x1 || rectangle.y0 >= rectangle.y1) return 1;
    if (rectangles->count) {
        SilRect *last = &rectangles->data[rectangles->count - 1];
        if (last->y0 == rectangle.y0 && last->y1 == rectangle.y1 && last->x1 == rectangle.x0) {
            last->x1 = rectangle.x1; return 1;
        }
    }
    SilRect *next = reserve(rectangles->data, &rectangles->capacity, rectangles->count, sizeof(*next));
    if (!next) return 0;
    rectangles->data = next; rectangles->data[rectangles->count++] = rectangle;
    return 1;
}

int SilLineAdd(SilLines *lines, MotionPoint a, MotionPoint b)
{
    if (!lines || !collection_valid(lines->data, lines->count, lines->capacity)
        || !coordinate(a.x) || !coordinate(a.y) || !coordinate(b.x) || !coordinate(b.y)) return 0;
    if (a.x == b.x && a.y == b.y) return 1;
    MotionSegment *next = reserve(lines->data, &lines->capacity, lines->count, sizeof(*next));
    if (!next) return 0;
    lines->data = next; lines->data[lines->count++] = (MotionSegment){a, b};
    return 1;
}
static int rectangles_valid(const SilRects *rectangles)
{
    if (!rectangles || !collection_valid(rectangles->data, rectangles->count, rectangles->capacity))
        return 0;
    for (size_t i = 0; i < rectangles->count; i++) {
        SilRect r = rectangles->data[i];
        if (!rectangle_valid(r) || r.x0 >= r.x1 || r.y0 >= r.y1) return 0;
    }
    return 1;
}
static int lines_valid(const SilLines *lines)
{
    if (!lines || !collection_valid(lines->data, lines->count, lines->capacity)) return 0;
    for (size_t i = 0; i < lines->count; i++) {
        MotionSegment s = lines->data[i];
        if (!coordinate(s.a.x) || !coordinate(s.a.y) || !coordinate(s.b.x) || !coordinate(s.b.y))
            return 0;
    }
    return 1;
}

/* Signed edge events cancel shared borders between disjoint opaque rectangles. */
typedef struct { int axis, delta; double fixed, at; } EdgeEvent;
static int compare_event(const void *left, const void *right)
{
    const EdgeEvent *a = left, *b = right;
    if (a->axis != b->axis) return a->axis < b->axis ? -1 : 1;
    if (a->fixed != b->fixed) return a->fixed < b->fixed ? -1 : 1;
    return a->at < b->at ? -1 : a->at > b->at;
}
static void rectangle_events(EdgeEvent *events, SilRect r)
{
    events[0] = (EdgeEvent){0, 1, r.y0, r.x0};
    events[1] = (EdgeEvent){0, -1, r.y0, r.x1};
    events[2] = (EdgeEvent){0, -1, r.y1, r.x0};
    events[3] = (EdgeEvent){0, 1, r.y1, r.x1};
    events[4] = (EdgeEvent){1, 1, r.x0, r.y0};
    events[5] = (EdgeEvent){1, -1, r.x0, r.y1};
    events[6] = (EdgeEvent){1, -1, r.x1, r.y0};
    events[7] = (EdgeEvent){1, 1, r.x1, r.y1};
}

static int sweep_events(const EdgeEvent *events, size_t count, SilLines *result)
{
    for (size_t i = 0; i < count;) {
        int axis = events[i].axis, balance = 0;
        double fixed = events[i].fixed, begin = 0;
        while (i < count && events[i].axis == axis && events[i].fixed == fixed) {
            double at = events[i].at;
            int delta = 0;
            do { delta += events[i++].delta; }
            while (i < count && events[i].axis == axis && events[i].fixed == fixed && events[i].at == at);
            int next = balance + delta;
            if (!balance && next) begin = at;
            if (balance && !next) {
                MotionPoint a = axis ? (MotionPoint){fixed, begin} : (MotionPoint){begin, fixed};
                MotionPoint b = axis ? (MotionPoint){fixed, at} : (MotionPoint){at, fixed};
                if (!SilLineAdd(result, a, b)) return 0;
            }
            balance = next;
        }
    }
    return 1;
}
int SilContours(const SilRects *rectangles, SilLines *outline)
{
    if (!outline || !rectangles_valid(rectangles)) return 0;
    size_t count = rectangles->count * 8;
    EdgeEvent *events = count ? malloc(count * sizeof(*events)) : NULL;
    if (count && !events) return 0;
    for (size_t i = 0; i < rectangles->count; i++) rectangle_events(events + i * 8, rectangles->data[i]);
    if (count) qsort(events, count, sizeof(*events), compare_event);
    SilLines result = {0};
    int success = sweep_events(events, count, &result);
    free(events);
    if (!success) { SilLinesFree(&result); return 0; }
    SilLinesFree(outline); *outline = result;
    return 1;
}

static unsigned read_u16(const unsigned char *p)
{
    return p[0] | ((unsigned)p[1] << 8);
}
static uint32_t read_u32(const unsigned char *p)
{
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int decode_column(Silhouette *shape, const unsigned char *patch,
                         size_t bytes, unsigned x, size_t at)
{
    unsigned last = 0, posts = 0;
    for (;;) {
        if (at >= bytes) return 0;
        if (patch[at] == 255) return 1;
        if (bytes - at < 4 || ++posts > MAX_PATCH_EXTENT) return 0;
        unsigned y = patch[at], length = patch[at + 1];
        if (bytes - at < (size_t)length + 4 || y < last || y + length > shape->height)
            return 0;
        if (length && !SilRectAdd(&shape->fill, (SilRect){x, y, x + 1, y + length}))
            return 0;
        last = y + length;
        at += length + 4;
    }
}

int SilhouetteDecode(Silhouette *shape, const void *data, size_t bytes)
{
    if (!shape || !data || bytes < 8) return 0;
    const unsigned char *patch = data;
    unsigned width = read_u16(patch), height = read_u16(patch + 2);
    size_t header = 8 + (size_t)width * 4;
    if (!width || !height || width > MAX_PATCH_EXTENT || height > MAX_PATCH_EXTENT || bytes < header)
        return 0;
    Silhouette result = {.width = width, .height = height};
    for (unsigned x = 0; x < width; x++) {
        size_t offset = read_u32(patch + 8 + x * 4);
        if (offset < header || !decode_column(&result, patch, bytes, x, offset)) goto fail;
    }
    if (!SilContours(&result.fill, &result.outline)) goto fail;
    SilhouetteFree(shape); *shape = result;
    return 1;
fail:
    SilhouetteFree(&result);
    return 0;
}

static int shape_valid(const Silhouette *shape)
{
    if (!shape || !shape->width || !shape->height || shape->width > MAX_PATCH_EXTENT
        || shape->height > MAX_PATCH_EXTENT || !rectangles_valid(&shape->fill)
        || !lines_valid(&shape->outline)) return 0;
    for (size_t i = 0; i < shape->fill.count; i++) {
        SilRect r = shape->fill.data[i];
        if (r.x0 < 0 || r.y0 < 0 || r.x1 > shape->width || r.y1 > shape->height) return 0;
    }
    return 1;
}

static int project_rectangle(SilRect r, double ox, double oy, double sx, double sy,
    int first, int last, const short *top, const short *bottom, unsigned height, SilRects *fill)
{
    double a = ox + r.x0 * sx, b = ox + r.x1 * sx;
    double y0 = oy + r.y0 * sy, y1 = oy + r.y1 * sy;
    if (!isfinite(a) || !isfinite(b) || !isfinite(y0) || !isfinite(y1)) return 0;
    if (a > b) { double swap = a; a = b; b = swap; }
    double left = fmax(first, a), right = fmin(last + 1, b);
    if (left >= right) return 1;
    for (int x = (int)floor(left); x < right;) {
        int end = x + 1;
        while (end < right && top[end] == top[x] && bottom[end] == bottom[x]) end++;
        double low = fmax(y0, fmax(0, top[x] + 1));
        double high = fmin(y1, fmin(height, bottom[x]));
        if (!SilRectAdd(fill, (SilRect){fmax(left, x), low, fmin(right, end), high})) return 0;
        x = end;
    }
    return 1;
}
