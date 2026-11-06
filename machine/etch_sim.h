// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef DOOM_ETCH_SIM_H
#define DOOM_ETCH_SIM_H
#include "motion.h"

#define ETCH_ASSUMED_SPEED_MM_S 60.0
#define ETCH_ASSUMED_ACCELERATION_MM_S2 250.0
#define ETCH_ASSUMED_BACKLASH_MM 0.15
#define ETCH_ASSUMED_STEP_MM 0.05
#define ETCH_ASSUMED_ERASE_SECONDS 2.5
#define ETCH_ASSUMED_GHOST_FRACTION 0.03
/* Assumed verification ink grid, not a framebuffer preview or opacity model. */
enum { ETCH_INK_WIDTH = 320, ETCH_INK_HEIGHT = 200, ETCH_MAX_POINTS = 400000 };
enum { ETCH_INK_EMPTY, ETCH_INK_FRESH, ETCH_INK_GHOST };
typedef struct {
    double speed_mm_s, acceleration_mm_s2, backlash_mm, step_mm;
} EtchAxisConfig;
typedef struct {
    EtchAxisConfig x, y;
    double width_mm, height_mm, erase_seconds, ghost_fraction;
} EtchConfig;
typedef struct {
    MotionPoint cursor_mm, motor_mm, motor_velocity_mm_s;
    double elapsed_s, remaining_s, erase_left_s;
    size_t completed, count; // Completed and submitted segment counts, excluding start.
    int drawing, erasing;
} EtchStatus;
typedef struct EtchDevice EtchDevice;
EtchConfig EtchAssumedDefaults(void);
/* All positions are millimetres from top-left, +Y down; start is step-quantized. */
EtchDevice *EtchCreate(EtchConfig configuration, MotionPoint start_mm);
void EtchDestroy(EtchDevice *device);
/* Busy/shaking submission fails unchanged. The owned copied path must begin at
 * the current quantized cursor. Positive finite feed caps path speed only. */
int EtchDeviceSubmit(EtchDevice *device, const MotionPoint *points_mm, size_t point_count,
                     double path_feed_mm_s);
int EtchDeviceSubmitHardware(EtchDevice *device, const MotionPoint *points_mm, size_t point_count);
/* Tick accepts finite nonnegative seconds. elapsed_s is drawing-only time,
 * resets on successful submission, and clamps to exact completed profiles.
 * Idle/shake time is excluded; surplus tick time advances a requested shake. */
int EtchDeviceTick(EtchDevice *device, double seconds);
/* Finish the current segment, cancel later segments, then shake. Idle starts
 * immediately; repeated requests do not restart the shake. Cursor/motors stay. */
int EtchDeviceRequestErase(EtchDevice *device);
int EtchDeviceReady(const EtchDevice *device);
EtchStatus EtchDeviceGetStatus(const EtchDevice *device);
/* Borrowed persistent classifications (not intensities); valid until destruction. */
const unsigned char *EtchDeviceInk(const EtchDevice *device);
#endif
