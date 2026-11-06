// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef DOOM_MACHINE_PLAYBACK_H
#define DOOM_MACHINE_PLAYBACK_H
#include "bridge.h"
typedef struct MachinePlayback MachinePlayback;
/* Owns a complete compiled-program copy and newly configured physical device. */
MachinePlayback *MachinePlaybackCreate(const MachineStepProgram *, MachineParseError *);
void MachinePlaybackDestroy(MachinePlayback *);
/* Borrowed read-only device; only playback may advance or mutate it. */
const EtchDevice *MachinePlaybackDevice(const MachinePlayback *);
const MachineStepProgram *MachinePlaybackProgram(const MachinePlayback *);
/* Finite nonnegative wall-clock seconds. Surplus crosses feed groups and M0
 * shakes; Seconds excludes idle surplus after end and includes erase time. */
int MachinePlaybackTick(MachinePlayback *, double seconds);
int MachinePlaybackDone(const MachinePlayback *);
double MachinePlaybackSeconds(const MachinePlayback *);
const MachineParseError *MachinePlaybackError(const MachinePlayback *);
#endif
