// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef DOOM_ETCH_TIMELINE_H
#define DOOM_ETCH_TIMELINE_H
#include "motion.h"

/* Metal-compatible 32-byte record: normalized XY endpoints and start/end
 * seconds. timing[2..3] are reserved zeros. This is constant-speed drawing,
 * separate from a physical acceleration/backlash simulation. */
typedef struct { float ends[4], timing[4]; } MotionTimedSegment;

/* Free any previously owned *segments before calling. Every non-NULL output
 * is reset on entry, including on failure. On success the caller owns the
 * malloc-compatible segment buffer; a start-only path returns NULL/0/0. */
int MotionTimelineBuild(const MotionPath *path, double speed_mm_s,
    MotionTimedSegment **segments, size_t *count, double *duration);
#endif
