// SPDX-License-Identifier: GPL-2.0-or-later
#include "motion.h"
#include <math.h>
#include <string.h>

// ASSUMPTION: the device advances in 0.05 mm increments on each axis.
static const double step_mm = 0.05;

/* Fixed decimal notation is accepted by GRBL; exponent notation is not.
 * Seventeen significant digits retain the chosen finite double coordinate. */
static int number(char text[128], double value)
{
    if (!isfinite(value) || value < 0) return 0;
    int decimals = value > 0 ? 16 - (int)floor(log10(value)) : 0;
    if (decimals < 0) decimals = 0;
    int length = snprintf(text, 128, "%.*f", decimals, value + 0.0);
    if (length <= 0 || length >= 128) return 0;
    if (strchr(text, '.')) {
        while (length && text[length - 1] == '0') text[--length] = 0;
        if (length && text[length - 1] == '.') text[--length] = 0;
    }
    return length > 0;
}

static double quantized(double units, unsigned source_extent, double extent_mm)
{
    double millimeters = (units / source_extent) * extent_mm;
    double last = floor(extent_mm / step_mm + 1e-9);
    double steps = fmin(round(millimeters / step_mm), last);
    return fmin(extent_mm, fmax(0.0, steps * step_mm));
}

static int position(FILE *stream, const char *command, const MotionPath *path, size_t index)
{
    MotionPoint point = path->points[index];
    double x = quantized(point.x, path->width, path->width_mm);
    double device_y = quantized(point.y, path->height, path->height_mm);
    double y = fmax(0.0, path->height_mm - device_y) + 0.0;
    char xs[128], ys[128], line[128];
    if (!number(xs, x) || !number(ys, y)) return 0;
    int length = snprintf(line, sizeof(line), "%s X%s Y%s\n", command, xs, ys);
    return length > 0 && length <= 81 && fputs(line, stream) >= 0;
}
