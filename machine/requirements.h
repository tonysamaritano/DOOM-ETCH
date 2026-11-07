// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef DOOM_MACHINE_REQUIREMENTS_H
#define DOOM_MACHINE_REQUIREMENTS_H
#include "steppers.h"
typedef struct {
    size_t segments;
    double path_m, sum_sqrt_m, dmax_m;
    double no_stop_m_s, no_stop_mach, accel_m_s2, accel_g;
    double peak_m_s, peak_mach, knob_rpm;
} MachineRequirements;
typedef struct { size_t frame; MachineRequirements demand; } MachineRequirementsFrame;
typedef struct {
    MachineRequirementsFrame *items;
    size_t count;
    double fps, knob_mm_turn;
} MachineRequirementsFrames;
/* Complete decimal token, finite positive value; failure preserves output. */
int MachineRequirementsParseFPS(const char *, double *);
/* Commanded step geometry in [begin,end); preceding motion supplies start.
 * Validate the complete program. Zero moves count; pauses/end retain cursor.
 * Nonzero demand underflow/overflow fails without changing output. */
int MachineRequirementsCalculate(const MachineStepProgram *, size_t begin, size_t end,
    double fps, MachineRequirements *, MachineParseError *);
/* Zero-initialize owned frames. Build replaces only on success; M0 creates
 * the next zero-based frame even when empty. Free clears all fields. */
int MachineRequirementsFramesBuild(MachineRequirementsFrames *,
    const MachineStepProgram *, double fps, MachineParseError *);
void MachineRequirementsFramesFree(MachineRequirementsFrames *);
/* Prevalidate before writing. JSON writes one array, no trailing newline;
 * plain writes labeled records. Stream stays open; I/O failure may be partial.
 * Demand excludes backlash/shake/jerk; knob RPM is path-speed-equivalent. */
int MachineRequirementsFramesValid(const MachineRequirementsFrames *);
int MachineRequirementsFramesWrite(FILE *, const MachineRequirementsFrames *, int json);
#endif
