// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef DOOM_MACHINE_STEPPERS_H
#define DOOM_MACHINE_STEPPERS_H
#include "gcode.h"
#include "etch_sim.h"
#include <stdint.h>
#define MACHINE_ASSUMED_STEPS_PER_MM 20.0
#define MACHINE_ASSUMED_KNOB_MM_TURN 20.0
/* Defaults are assumptions, not measured calibration. */
typedef struct {
    double steps_per_mm[2], max_rate_mm_min[2], acceleration_mm_s2[2];
    double width_mm, height_mm, backlash_mm, erase_seconds, ghost_fraction, knob_mm_turn;
} MachineSettings;
typedef struct {
    int64_t target_steps[2]; /* Device/top-left grid, +Y down. */
    double feed_mm_s; /* Standard path cap; zero for G0 hardware-only. */
    MachineMoveKind kind;
    size_t line;
} MachineStep;
typedef struct {
    MachineStep *moves;
    size_t count;
    int64_t start_steps[2], final_steps[2];
    uint64_t total_steps[2], reversals[2]; /* Exclude backlash take-up. */
    MachineSettings settings;
} MachineStepProgram;
void MachineSettingsDefault(MachineSettings *);
int MachineSettingsValidate(const MachineSettings *, MachineParseError *);
int MachineSettingsParse(MachineSettings *, const char *, MachineParseError *);
EtchConfig MachineSettingsConfig(const MachineSettings *);
/* Owned outputs must start zeroed; compilation fails without changing them. */
int MachineSteppersCompile(MachineStepProgram *, const MachineProgram *, const MachineSettings *, MachineParseError *);
void MachineStepProgramFree(MachineStepProgram *);
#endif
