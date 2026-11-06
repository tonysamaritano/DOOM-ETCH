// SPDX-License-Identifier: GPL-2.0-or-later
#include <assert.h>
#include <string.h>
#include "silhouette.h"

static void grayscale(unsigned char *palette, unsigned index, unsigned char value)
{
    for (unsigned channel = 0; channel < 3; channel++) palette[index * 3 + channel] = value;
}
static void vertical_edge(const SilLines *lines)
{
    assert(lines->count == 1);
    MotionSegment edge = lines->data[0];
    assert(edge.a.x == 1 && edge.a.y == 0 && edge.b.x == 1 && edge.b.y == 3);
}
int main(void)
{
    // Two opaque columns (black and white), followed by a transparent column.
    unsigned char patch[] = {3,0,3,0,0,0,0,0,20,0,0,0,28,0,0,0,36,0,0,0,
        0,3,0,0,0,0,0,255, 0,3,0,1,1,1,0,255, 255};
    unsigned char palette[768] = {0};
    grayscale(palette, 1, 255);
    Silhouette shape = {0}; SilLines features = {0};
    assert(SilhouetteDecode(&shape, patch, sizeof(patch)));
    for (unsigned y = 0; y < 3; y++) {
        assert(shape.texels[y * 3] == 0); // Palette zero remains opaque.
        assert(shape.texels[y * 3 + 1] == 1);
        assert(shape.texels[y * 3 + 2] == 256);
    }
    memset(patch, 0, sizeof(patch)); // Decoder owns colors independently of lump storage.
    assert(SilFeatures(&shape, palette, &features)); vertical_edge(&features);
    unsigned short *owned = shape.texels;
    assert(!SilhouetteDecode(&shape, patch, 8));
    assert(shape.texels == owned && shape.texels[1] == 1);
    grayscale(palette, 2, 30); // Weak difference: below the 36-luminance threshold.
    grayscale(palette, 3, 50); // Strong difference but remains in the same 64-level bin.
    grayscale(palette, 4, 80); // Strong difference across a bin boundary.
    for (unsigned index = 2; index <= 4; index++) {
        for (unsigned y = 0; y < 3; y++) shape.texels[y * 3 + 1] = index;
        assert(SilFeatures(&shape, palette, &features));
        if (index == 4) vertical_edge(&features);
        else assert(features.count == 0);
    }
    grayscale(palette, 5, 63); grayscale(palette, 6, 65);
    for (unsigned y = 0; y < 3; y++) {
        shape.texels[y * 3] = 5; shape.texels[y * 3 + 1] = 6;
    }
    assert(SilFeatures(&shape, palette, &features) && features.count == 0);
    // A horizontal black/white boundary merges across all three source columns.
    for (unsigned y = 0; y < 3; y++)
        for (unsigned x = 0; x < 3; x++) shape.texels[y * 3 + x] = y ? 1 : 0;
    assert(SilFeatures(&shape, palette, &features) && features.count == 1);
    MotionSegment edge = features.data[0];
    assert(edge.a.x == 0 && edge.a.y == 1 && edge.b.x == 3 && edge.b.y == 1);
    MotionSegment *previous = features.data;
    assert(!SilFeatures(&shape, NULL, &features));
    assert(features.data == previous && features.count == 1);
    shape.texels[0] = 300;
    assert(!SilFeatures(&shape, palette, &features));
    assert(features.data == previous && features.count == 1);
    assert(features.data[0].a.y == 1 && features.data[0].b.x == 3);
    shape.texels[0] = 0;
    shape.texels = NULL;
    assert(!SilFeatures(&shape, palette, &features) && features.data == previous);
    shape.texels = owned;
    SilLinesFree(&features); SilhouetteFree(&shape);
    assert(!shape.texels && !shape.fill.data && !shape.outline.data && !shape.width);
    return 0;
}
