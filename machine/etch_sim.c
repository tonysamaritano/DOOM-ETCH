// SPDX-License-Identifier: GPL-2.0-or-later
#include "etch_sim.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    MotionPoint start, end, unit, motor_start;
    double length, slack, travel, acceleration, peak, ramp, duration, begin, finish;
    int direction_x, direction_y;
} Segment;
struct EtchDevice {
    EtchConfig config;
    EtchStatus status;
    Segment *segments;
    size_t index;
    double segment_time, total, erase_time;
    int direction_x, direction_y, erase_requested;
    unsigned next_sample, erase_count;
    unsigned char ink[ETCH_INK_WIDTH * ETCH_INK_HEIGHT];
};
EtchConfig EtchAssumedDefaults(void)
{
    EtchAxisConfig axis = {ETCH_ASSUMED_SPEED_MM_S, ETCH_ASSUMED_ACCELERATION_MM_S2,
                          ETCH_ASSUMED_BACKLASH_MM, ETCH_ASSUMED_STEP_MM};
    return (EtchConfig){axis, axis, MOTION_WIDTH_MM, MOTION_HEIGHT_MM,
                       ETCH_ASSUMED_ERASE_SECONDS, ETCH_ASSUMED_GHOST_FRACTION};
}
static int axis_valid(EtchAxisConfig a, double extent)
{
    return isfinite(a.speed_mm_s) && a.speed_mm_s > 0 &&
        isfinite(a.acceleration_mm_s2) && a.acceleration_mm_s2 > 0 &&
        isfinite(a.backlash_mm) && a.backlash_mm >= 0 &&
        isfinite(a.step_mm) && a.step_mm > 0 && isfinite(extent) && extent > 0 &&
        isfinite(extent / a.step_mm) && extent / a.step_mm <= 0x1p52;
}
static double quantize(double value, double step, double extent)
{
    return fmin(round(value / step), floor(extent / step + 1e-10)) * step;
}
static int point(EtchConfig c, MotionPoint input, MotionPoint *out)
{
    if (!isfinite(input.x) || !isfinite(input.y) || input.x < 0 || input.y < 0 ||
        input.x > c.width_mm || input.y > c.height_mm) return 0;
    *out = (MotionPoint){quantize(input.x,c.x.step_mm,c.width_mm),
                        quantize(input.y,c.y.step_mm,c.height_mm)};
    return 1;
}
EtchDevice *EtchCreate(EtchConfig c, MotionPoint start)
{
    MotionPoint q;
    if (!axis_valid(c.x,c.width_mm) || !axis_valid(c.y,c.height_mm) ||
        !isfinite(c.erase_seconds) || c.erase_seconds <= 0 ||
        !isfinite(c.ghost_fraction) || c.ghost_fraction < 0 || c.ghost_fraction > 1 ||
        !point(c,start,&q)) return NULL;
    EtchDevice *d = calloc(1,sizeof(*d));
    if (d) { d->config=c; d->status.cursor_mm=q; d->status.motor_mm=q; }
    return d;
}
void EtchDestroy(EtchDevice *d)
{
    if (d) { free(d->segments); free(d); }
}
