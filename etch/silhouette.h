// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef DOOM_ETCH_SILHOUETTE_H
#define DOOM_ETCH_SILHOUETTE_H
#include "motion.h"

/* Initialize owned collections to zero. Geometry is finite, with coordinates
 * bounded to +/-1e9 and at most 400000 entries per collection. Rectangles
 * decoded/projected here have disjoint interiors; their union may have holes. */
typedef struct { double x0, y0, x1, y1; } SilRect;
typedef struct { SilRect *data; size_t count, capacity; } SilRects;
typedef struct { MotionSegment *data; size_t count, capacity; } SilLines;
typedef struct {
    unsigned width, height;
    SilRects fill;
    SilLines outline;
    unsigned short *texels; // Original palette indices; 256 means transparent.
} Silhouette;
void SilRectsFree(SilRects *rectangles);
void SilLinesFree(SilLines *lines);
void SilhouetteFree(Silhouette *shape);
int SilRectAdd(SilRects *rectangles, SilRect rectangle);
int SilLineAdd(SilLines *lines, MotionPoint a, MotionPoint b);
int SilhouetteDecode(Silhouette *shape, const void *patch, size_t bytes);
int SilContours(const SilRects *rectangles, SilLines *outline);
int SilFeatures(const Silhouette *shape, const unsigned char *palette, SilLines *outline);
/* first/last are inclusive. top is the last occluded row; bottom is exclusive. */
int SilProject(const Silhouette *source, double ox, double oy, double sx, double sy,
    int first, int last, const short *top, const short *bottom,
    unsigned width, unsigned height, SilRects *fill, SilLines *outline);
/* Subtract the opaque mask from existing lines, then append its outline. */
int SilComposite(SilLines *scene, const SilRects *mask, const SilLines *outline);
#endif
