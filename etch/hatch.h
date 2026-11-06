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
#endif
