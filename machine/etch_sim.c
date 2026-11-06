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
