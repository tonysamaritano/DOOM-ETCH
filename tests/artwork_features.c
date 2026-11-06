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
