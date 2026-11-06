// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef DOOM_ETCH_HATCH_H
#define DOOM_ETCH_HATCH_H
#include <math.h>
#include "motion.h"

#define HATCH_MM_X (MOTION_WIDTH_MM / 320.0)
#define HATCH_MM_Y (MOTION_HEIGHT_MM / 200.0)
#define HATCH_SPACING_MM 1.914
#define HATCH_WEAPON_SPACING_MM 0.7975

enum { HATCH_HORIZONTAL, HATCH_DIAGONAL, HATCH_CROSS, HATCH_PATTERN_COUNT };
static inline double HatchSlope(int pattern, int family)
{
    return pattern == HATCH_HORIZONTAL ? 0 : family ? -.65 : .65;
}
static inline int HatchFamilies(int pattern) { return pattern == HATCH_CROSS ? 2 : 1; }
static inline double HatchThreshold(int row, int family)
{
    static const double threshold[4] = {215, 115, 165, 65};
    return threshold[(unsigned)row & 3] - (family ? 85 : 0);
}
/* Clip a stroke to a strip whose upper and lower edges vary linearly. */
static inline int HatchClip(double y0, double y1, double top0, double top1,
    double bottom0, double bottom1, double *lo, double *hi)
{
    if (!lo || !hi || !isfinite(y0) || !isfinite(y1) || !isfinite(top0)
        || !isfinite(top1) || !isfinite(bottom0) || !isfinite(bottom1)) return 0;
    double q[2] = {y0 - top0, bottom0 - y0};
    double d[2] = {(y1 - top1) - q[0], (bottom1 - y1) - q[1]};
    double low = 0, high = 1;
    for (int i = 0; i < 2; i++) {
        if (!isfinite(q[i]) || !isfinite(d[i])) return 0;
        if (d[i] == 0) { if (q[i] < 0) return 0; }
        else {
            double t = -q[i] / d[i];
            if (d[i] > 0) low = fmax(low, t);
            else high = fmin(high, t);
        }
    }
    if (!(high > low)) return 0;
    *lo = low; *hi = high;
    return 1;
}
#endif
