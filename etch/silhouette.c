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
