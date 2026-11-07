// SPDX-License-Identifier: GPL-2.0-or-later
#include "silhouette.h"
#include "hatch.h"
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
    free(shape->texels);
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
        for (unsigned k = 0; k < length; k++)
            shape->texels[(size_t)(y + k) * shape->width + x] = patch[at + 3 + k];
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
    size_t texels = (size_t)width * height;
    result.texels = malloc(texels * sizeof(*result.texels));
    if (!result.texels) return 0;
    for (size_t i = 0; i < texels; i++) result.texels[i] = 256;
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

int SilProject(const Silhouette *source, double ox, double oy, double sx, double sy,
    int first, int last, const short *top, const short *bottom,
    unsigned width, unsigned height, SilRects *fill, SilLines *outline)
{
    if (!fill || !outline || !shape_valid(source) || !top || !bottom
        || !width || !height || width > MAX_PATCH_EXTENT || height > MAX_PATCH_EXTENT
        || !isfinite(ox) || !isfinite(oy) || !isfinite(sx) || !isfinite(sy) || sx == 0 || sy <= 0)
        return 0;
    if (first < 0) first = 0;
    if (last >= (int)width) last = (int)width - 1;
    SilRects next_fill = {0}; SilLines next_outline = {0};
    if (first <= last) {
        for (size_t i = 0; i < source->fill.count; i++)
            if (!project_rectangle(source->fill.data[i], ox, oy, sx, sy,
                    first, last, top, bottom, height, &next_fill)) goto fail;
    }
    // Rebuild contours after clipping so newly exposed boundaries remain closed.
    if (!SilContours(&next_fill, &next_outline)) goto fail;
    SilRectsFree(fill); SilLinesFree(outline);
    *fill = next_fill; *outline = next_outline;
    return 1;
fail:
    SilRectsFree(&next_fill); SilLinesFree(&next_outline);
    return 0;
}

typedef struct { double low, high; } Interval;
static int compare_interval(const void *left, const void *right)
{
    const Interval *a = left, *b = right;
    return a->low < b->low ? -1 : a->low > b->low;
}
static int clip_interval(double p, double q, double *low, double *high)
{
    if (p == 0) return q >= 0;
    double t = q / p;
    if (p < 0) {
        if (t > *high) return 0;
        if (t > *low) *low = t;
    } else {
        if (t < *low) return 0;
        if (t < *high) *high = t;
    }
    return 1;
}
static MotionPoint along(MotionSegment segment, double t)
{
    return (MotionPoint){segment.a.x + (segment.b.x - segment.a.x) * t,
        segment.a.y + (segment.b.y - segment.a.y) * t};
}

static size_t covered_intervals(MotionSegment s, const SilRects *mask, Interval *intervals)
{
    size_t count = 0;
    double dx = s.b.x - s.a.x, dy = s.b.y - s.a.y;
    for (size_t j = 0; j < mask->count; j++) {
        SilRect r = mask->data[j];
        if (fmax(s.a.x, s.b.x) < r.x0 || fmin(s.a.x, s.b.x) > r.x1
            || fmax(s.a.y, s.b.y) < r.y0 || fmin(s.a.y, s.b.y) > r.y1) continue;
        double low = 0, high = 1;
        if (clip_interval(-dx, s.a.x - r.x0, &low, &high)
            && clip_interval(dx, r.x1 - s.a.x, &low, &high)
            && clip_interval(-dy, s.a.y - r.y0, &low, &high)
            && clip_interval(dy, r.y1 - s.a.y, &low, &high) && high > low)
            intervals[count++] = (Interval){low, high};
    }
    if (count) qsort(intervals, count, sizeof(*intervals), compare_interval);
    return count;
}
static int uncovered_lines(SilLines *result, MotionSegment s, const Interval *intervals, size_t count)
{
    double at = 0;
    for (size_t i = 0; i < count; i++) {
        if (intervals[i].low > at
            && !SilLineAdd(result, along(s, at), along(s, intervals[i].low))) return 0;
        at = fmax(at, intervals[i].high);
    }
    return at >= 1 || SilLineAdd(result, along(s, at), s.b);
}

static SilRect mask_bounds(const SilRects *mask)
{
    SilRect box = mask->data[0];
    for (size_t j = 1; j < mask->count; j++) {
        SilRect r = mask->data[j];
        box = (SilRect){fmin(box.x0, r.x0), fmin(box.y0, r.y0), fmax(box.x1, r.x1), fmax(box.y1, r.y1)};
    }
    return box;
}
static int touches(MotionSegment s, SilRect r)
{
    return !(fmax(s.a.x, s.b.x) < r.x0 || fmin(s.a.x, s.b.x) > r.x1
        || fmax(s.a.y, s.b.y) < r.y0 || fmin(s.a.y, s.b.y) > r.y1);
}

/* Every patch is composited over the scene so far. Only lines near the patch
 * can change, so lines before the first one near it stay where they are and
 * the rest are tested against the mask only when they touch its bounds. */
int SilComposite(SilLines *scene, const SilRects *mask, const SilLines *outline)
{
    if (!lines_valid(scene) || !rectangles_valid(mask) || !lines_valid(outline)) return 0;
    size_t first = scene->count;
    SilRect box = {0};
    if (mask->count) {
        box = mask_bounds(mask);
        for (size_t i = 0; i < scene->count; i++)
            if (touches(scene->data[i], box)) { first = i; break; }
    }
    Interval *intervals = mask->count ? malloc(mask->count * sizeof(*intervals)) : NULL;
    if (mask->count && !intervals) return 0;
    SilLines tail = {0};
    for (size_t i = first; i < scene->count; i++) {
        MotionSegment segment = scene->data[i];
        size_t count = touches(segment, box) ? covered_intervals(segment, mask, intervals) : 0;
        if (!uncovered_lines(&tail, segment, intervals, count)) goto fail;
    }
    for (size_t i = 0; i < outline->count; i++)
        if (!SilLineAdd(&tail, outline->data[i].a, outline->data[i].b)) goto fail;
    size_t total = first + tail.count;
    if (total > MAX_ITEMS) goto fail;
    if (total > scene->capacity) {
        MotionSegment *grown = realloc(scene->data, total * sizeof(*grown));
        if (!grown) goto fail;
        scene->data = grown; scene->capacity = total;
    }
    if (tail.count) memcpy(scene->data + first, tail.data, tail.count * sizeof(*tail.data));
    scene->count = total;
    free(intervals); SilLinesFree(&tail);
    return 1;
fail:
    free(intervals); SilLinesFree(&tail);
    return 0;
}

static int strong_boundary(unsigned a, unsigned b, const double tone[256])
{
    return a < 256 && b < 256 && fabs(tone[a] - tone[b]) >= 36
        && (int)tone[a] / 64 != (int)tone[b] / 64;
}

static int feature_run(const Silhouette *shape, const double tone[256],
                        int axis, unsigned fixed, SilLines *result)
{
    unsigned length = axis ? shape->height : shape->width;
    int begin = -1;
    for (unsigned run = 0; run <= length; run++) {
        int edge = 0;
        if (run < length) {
            size_t at = axis ? (size_t)run * shape->width + fixed
                : (size_t)fixed * shape->width + run;
            unsigned a = shape->texels[at];
            unsigned b = shape->texels[at - (axis ? 1 : shape->width)];
            edge = strong_boundary(a, b, tone);
        }
        if (edge && begin < 0) begin = (int)run;
        if (!edge && begin >= 0) {
            MotionPoint a = axis ? (MotionPoint){fixed, begin} : (MotionPoint){begin, fixed};
            MotionPoint b = axis ? (MotionPoint){fixed, run} : (MotionPoint){run, fixed};
            if (!SilLineAdd(result, a, b)) return 0;
            begin = -1;
        }
    }
    return 1;
}

int SilFeatures(const Silhouette *shape, const unsigned char *palette, SilLines *outline)
{
    if (!shape_valid(shape) || !shape->texels || !palette || !outline) return 0;
    for (size_t i = 0; i < (size_t)shape->width * shape->height; i++)
        if (shape->texels[i] > 256) return 0;
    double tone[256];
    for (unsigned i = 0; i < 256; i++)
        tone[i] = .2126 * palette[3 * i] + .7152 * palette[3 * i + 1] + .0722 * palette[3 * i + 2];
    SilLines result = {0};
    for (int axis = 0; axis < 2; axis++) {
        unsigned limit = axis ? shape->width : shape->height;
        for (unsigned fixed = 1; fixed < limit; fixed++) {
            if (!feature_run(shape, tone, axis, fixed, &result)) {
                SilLinesFree(&result);
                return 0;
            }
        }
    }
    SilLinesFree(outline); *outline = result;
    return 1;
}

typedef struct {
    const Silhouette *shape;
    const unsigned char *light, *palette, *translation;
    double ox, oy, sx, sy, min_spacing_mm;
} HatchContext;
typedef struct { MotionSegment line; int family, row; } HatchStroke;
typedef struct { HatchStroke *data; size_t count, capacity; } HatchStrokes;

static int hatch_spacing(const HatchContext *context, double slope)
{
    double vertical_mm = context->sy * HATCH_MM_Y;
    double physical_slope = slope == 0 ? 0
        : slope * vertical_mm / (context->sx * HATCH_MM_X);
    double needed = context->min_spacing_mm * hypot(1, physical_slope);
    if (!isfinite(needed) || !(vertical_mm > 0)) return 0;
    int spacing = 1;
    while (spacing < 1048576 && spacing * vertical_mm < needed) spacing *= 2;
    return spacing * vertical_mm >= needed ? spacing : 0;
}

static int stroke_add(HatchStrokes *strokes, MotionSegment line, int family, int row)
{
    HatchStroke *next = reserve(strokes->data, &strokes->capacity, strokes->count, sizeof(*next));
    if (!next) return 0;
    strokes->data = next;
    strokes->data[strokes->count++] = (HatchStroke){line, family, row};
    return 1;
}

static int hatch_tone(const HatchContext *context, double u, double v, double *tone)
{
    const Silhouette *shape = context->shape;
    double fx = fmin(shape->width, 1 / fabs(context->sx));
    double fy = fmin(shape->height, context->min_spacing_mm / (context->sy * HATCH_MM_Y));
    double sum = 0;
    unsigned count = 0;
    for (int a = 0; a < 4; a++) {
        for (int b = 0; b < 4; b++) {
            double x = floor(u + ((a + .5) / 4 - .5) * fx);
            double y = floor(v + ((b + .5) / 4 - .5) * fy);
            if (x < 0 || x >= shape->width || y < 0 || y >= shape->height) continue;
            unsigned index = shape->texels[(size_t)y * shape->width + (size_t)x];
            if (index == 256) continue;
            if (context->translation) index = context->translation[index];
            const unsigned char *rgb = context->palette + 3 * context->light[index];
            sum += .2126 * rgb[0] + .7152 * rgb[1] + .0722 * rgb[2];
            count++;
        }
    }
    if (count) *tone = sum / count;
    return count != 0;
}

static int hatch_piece(const HatchContext *context, HatchStrokes *strokes,
    SilRect rectangle, double row, int spacing, double slope, int family, double x, double end)
{
    double y0 = context->oy + (row + slope * (x - context->ox) / context->sx) * context->sy;
    double y1 = context->oy + (row + slope * (end - context->ox) / context->sx) * context->sy;
    double low, high;
    if (!HatchClip(y0, y1, rectangle.y0 + .35, rectangle.y0 + .35,
            rectangle.y1 - .35, rectangle.y1 - .35, &low, &high)) return 1;
    double x0 = x + (end - x) * low, x1 = x + (end - x) * high;
    double u = ((x0 + x1) * .5 - context->ox) / context->sx;
    double v = row + slope * u, tone;
    if (!hatch_tone(context, u, v, &tone) || tone >= HatchThreshold((int)(row / spacing), family))
        return 1;
    MotionSegment line = {{x0, y0 + (y1 - y0) * low}, {x1, y0 + (y1 - y0) * high}};
    return stroke_add(strokes, line, family, (int)row);
}

static int hatch_rectangle(const HatchContext *context, SilRect *rectangle)
{
    double end_x = context->ox + context->shape->width * context->sx;
    double end_y = context->oy + context->shape->height * context->sy;
    if (!isfinite(end_x) || !isfinite(end_y)) return -1;
    rectangle->x0 = fmax(rectangle->x0, fmax(0, fmin(context->ox, end_x)));
    rectangle->x1 = fmin(rectangle->x1, fmin(320, fmax(context->ox, end_x)));
    rectangle->y0 = fmax(rectangle->y0, fmax(0, context->oy));
    rectangle->y1 = fmin(rectangle->y1, fmin(200, end_y));
    return rectangle->x0 < rectangle->x1 && rectangle->y0 + .35 < rectangle->y1 - .35;
}

static int hatch_rows(const HatchContext *context, HatchStrokes *strokes,
                       SilRect rectangle, double slope, int family, int spacing)
{
    int visible = hatch_rectangle(context, &rectangle);
    if (visible <= 0) return visible == 0;
    double u0 = (rectangle.x0 - context->ox) / context->sx;
    double u1 = (rectangle.x1 - context->ox) / context->sx;
    double first = ceil(((rectangle.y0 + .35 - context->oy) / context->sy
        - fmax(slope * u0, slope * u1)) / spacing) * spacing;
    double last = (rectangle.y1 - .35 - context->oy) / context->sy - fmin(slope * u0, slope * u1);
    if (!isfinite(first) || !isfinite(last) || fabs(first) > 1e9 || fabs(last) > 1e9) return 0;
    for (double row = first; row <= last; row += spacing) {
        for (double x = rectangle.x0; x < rectangle.x1;) {
            double end = fmin(rectangle.x1, floor(x) + 1);
            if (!hatch_piece(context, strokes, rectangle, row, spacing, slope, family, x, end)) return 0;
            x = end;
        }
    }
    return 1;
}

static int compare_stroke(const void *left, const void *right)
{
    const HatchStroke *a = left, *b = right;
    if (a->family != b->family) return a->family < b->family ? -1 : 1;
    if (a->row != b->row) return a->row < b->row ? -1 : 1;
    return a->line.a.x < b->line.a.x ? -1 : a->line.a.x > b->line.a.x;
}

static int publish_hatches(HatchStrokes *strokes, SilLines *out)
{
    SilLines result = {0};
    for (size_t i = 0; i < out->count; i++)
        if (!SilLineAdd(&result, out->data[i].a, out->data[i].b)) goto fail;
    if (strokes->count) qsort(strokes->data, strokes->count, sizeof(*strokes->data), compare_stroke);
    for (size_t i = 0; i < strokes->count;) {
        HatchStroke stroke = strokes->data[i++];
        MotionSegment line = stroke.line;
        while (i < strokes->count && strokes->data[i].family == stroke.family
            && strokes->data[i].row == stroke.row
            && fabs(strokes->data[i].line.a.x - line.b.x) < 1e-9
            && fabs(strokes->data[i].line.a.y - line.b.y) < 1e-9)
            line.b = strokes->data[i++].line.b;
        if (!SilLineAdd(&result, line.a, line.b)) goto fail;
    }
    SilLinesFree(out); *out = result;
    return 1;
fail:
    SilLinesFree(&result);
    return 0;
}

int SilHatchDetail(const Silhouette *shape, const SilRects *visible, double ox, double oy,
    double sx, double sy, const unsigned char *light, const unsigned char *palette,
    const unsigned char *translation, int pattern, double min_spacing_mm, SilLines *out)
{
    if (!shape_valid(shape) || !shape->texels || !rectangles_valid(visible) || !lines_valid(out)
        || !light || !palette || !isfinite(ox) || !isfinite(oy) || !isfinite(sx) || !isfinite(sy)
        || sx == 0 || sy <= 0 || pattern < 0 || pattern >= HATCH_PATTERN_COUNT
        || !isfinite(min_spacing_mm) || min_spacing_mm <= 0) return 0;
    for (size_t i = 0; i < (size_t)shape->width * shape->height; i++)
        if (shape->texels[i] > 256) return 0;
    HatchContext context = {shape, light, palette, translation, ox, oy, sx, sy, min_spacing_mm};
    HatchStrokes strokes = {0};
    int success = 0;
    for (int family = 0; family < HatchFamilies(pattern); family++) {
        double slope = HatchSlope(pattern, family);
        int spacing = hatch_spacing(&context, slope);
        if (!spacing) goto done;
        for (size_t i = 0; i < visible->count; i++)
            if (!hatch_rows(&context, &strokes, visible->data[i], slope, family, spacing)) goto done;
    }
    success = publish_hatches(&strokes, out);
done:
    free(strokes.data);
    return success;
}

int SilHatch(const Silhouette *shape, const SilRects *visible, double ox, double oy,
    double sx, double sy, const unsigned char *light, const unsigned char *palette,
    const unsigned char *translation, int pattern, SilLines *out)
{
    return SilHatchDetail(shape, visible, ox, oy, sx, sy, light, palette, translation,
        pattern, HATCH_SPACING_MM, out);
}
