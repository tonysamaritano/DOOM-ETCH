// SPDX-License-Identifier: GPL-2.0-or-later
#include "motion.h"

static MotionPoint physical(const MotionPath *path, size_t index)
{
    MotionPoint point = path->points[index];
    return (MotionPoint){(point.x / path->width) * path->width_mm,
        (point.y / path->height) * path->height_mm};
}

int MotionWriteSVG(FILE *stream, const MotionPath *path)
{
    if (!stream || ferror(stream) || !MotionValid(path)) return 0;
    fprintf(stream, "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "width=\"%.17gmm\" height=\"%.17gmm\" viewBox=\"0 0 %.17g %.17g\">\n",
        path->width_mm, path->height_mm, path->width_mm, path->height_mm);
    fputs("<path fill=\"none\" stroke=\"black\" stroke-width=\"0.15\" d=\"", stream);
    for (size_t i = 0; i < path->count; i++) {
        MotionPoint point = physical(path, i);
        fprintf(stream, "%c %.17g %.17g ", i ? 'L' : 'M', point.x + 0.0, point.y + 0.0);
    }
    fputs("\"/>\n</svg>\n", stream);
    return !ferror(stream) && fflush(stream) == 0;
}
