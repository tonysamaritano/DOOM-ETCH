// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef DOOM_MACHINE_RUN_H
#define DOOM_MACHINE_RUN_H
#include "playback.h"
/* Load a file, then apply explicit calibration assignments transactionally. */
MachinePlayback *MachineLoad(const char *filename,const char *const *assignments,
                             size_t count,MachineParseError *error);
int MachineRun(MachinePlayback *playback);
/* Full-file physical results only; callers must establish actual completion. */
int MachineReport(FILE *stream,const MachineStepProgram *program,double seconds,int json);
#endif
