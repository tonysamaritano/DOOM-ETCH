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
