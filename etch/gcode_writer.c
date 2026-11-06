// SPDX-License-Identifier: GPL-2.0-or-later
#include "motion.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

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

/* A .05 mm grid needs two decimals. A non-grid extent also contributes
 * its fractional phase after the Y reflection, so retain those digits. */
static int coordinate(char text[128], double value, double extent)
{
    for (int decimals = 2; decimals <= 17; decimals++) {
        char boundary[128];
        int length = snprintf(boundary, sizeof(boundary), "%.*f", decimals, extent);
        if (length <= 0 || length >= 128) return 0;
        if (strtod(boundary, NULL) != extent) continue;
        length = snprintf(text, 128, "%.*f", decimals, value + 0.0);
        return length > 0 && length < 128;
    }
    return number(text, value);
}

static double quantized(double units, unsigned source_extent, double extent_mm)
{
    double millimeters = (units / source_extent) * extent_mm;
    double last = floor(extent_mm / step_mm + 1e-9);
    double steps = fmin(round(millimeters / step_mm), last);
    return fmin(extent_mm, fmax(0.0, steps * step_mm));
}

int MotionQuantizePoint(const MotionPath *path, MotionPoint source, MotionPoint *device_mm)
{
    if (!path || !device_mm || !path->width || !path->height
        || !isfinite(path->width_mm) || !isfinite(path->height_mm)
        || path->width_mm <= 0 || path->height_mm <= 0
        || !isfinite(path->width_mm / step_mm) || !isfinite(path->height_mm / step_mm)
        || !isfinite(source.x) || !isfinite(source.y) || source.x < 0 || source.y < 0
        || source.x > path->width || source.y > path->height) return 0;
    *device_mm = (MotionPoint){quantized(source.x, path->width, path->width_mm),
        quantized(source.y, path->height, path->height_mm)};
    return 1;
}

static int position(FILE *stream, const char *command, const MotionPath *path, size_t index)
{
    MotionPoint device;
    if (!MotionQuantizePoint(path, path->points[index], &device)) return 0;
    double y = fmax(0.0, path->height_mm - device.y) + 0.0;
    char xs[128], ys[128], line[128];
    if (!coordinate(xs, device.x, path->width_mm)
        || !coordinate(ys, y, path->height_mm)) return 0;
    int length = snprintf(line, sizeof(line), "%s X%s Y%s\n", command, xs, ys);
    return length > 0 && length <= 81 && fputs(line, stream) >= 0;
}

int MotionWriteGCodeFrame(FILE *stream, const MotionPath *path, double feed_mm_min, int first, int last)
{
    if ((first != 0 && first != 1) || (last != 0 && last != 1)
        || !stream || ferror(stream) || !MotionValid(path)
        || !isfinite(feed_mm_min) || feed_mm_min <= 0
        || !isfinite(path->width_mm / step_mm) || !isfinite(path->height_mm / step_mm))
        return 0;
    char feed[128], width[128], height[128];
    if (!number(feed, feed_mm_min) || strlen(feed) + 1 > 80
        || !coordinate(width, path->width_mm, path->width_mm)
        || !coordinate(height, path->height_mm, path->height_mm)) return 0;
    if (first) {
        fputs("; Continuous pen-down drawing; work origin bottom-left, +Y up.\n"
            "; Jog to ETCH_START before running; no automatic positioning or pen lift.\n"
            "; ASSUMPTION: 0.05 mm step grid on both device axes.\n", stream);
        fprintf(stream, "; ETCH_SIZE X%s Y%s\n", width, height);
        if (!position(stream, "; ETCH_START", path, 0)) return 0;
        fputs("G21\nG90\nG94\nG61\n", stream);
    } else fputs("M0 ; shake to erase\n", stream);
    fprintf(stream, "F%s\n", feed);
    for (size_t i = 1; i < path->count; i++)
        if (!position(stream, "G1", path, i)) return 0;
    if (last) fputs("M2\n", stream);
    return !ferror(stream) && fflush(stream) == 0;
}

int MotionWriteGCode(FILE *stream, const MotionPath *path, double feed_mm_min)
{
    return MotionWriteGCodeFrame(stream, path, feed_mm_min, 1, 1);
}
