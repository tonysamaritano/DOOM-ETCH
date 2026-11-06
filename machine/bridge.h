// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef DOOM_MACHINE_BRIDGE_H
#define DOOM_MACHINE_BRIDGE_H
#include "steppers.h"
/* Dedicated leading ETCH_SIZE/ETCH_START comments establish travel/start.
 * Neither metadata nor adapters move the cursor to a required start. */
int MachineRead(FILE *, MachineSettings *, MachineProgram *, MachineParseError *);
/* Batch-only: contiguous motion records with the same feed, no M0/end.
 * Device configuration must match program settings. Full programs use playback. */
int MachineSubmitBatch(EtchDevice *, const MachineStepProgram *, size_t first,
                       size_t count, MachineParseError *);
/* Genuine direct IR route: raw physical scaling, no writer or parser. */
int MachineSubmitMotion(EtchDevice *, const MotionPath *, double feed_mm_min, MachineParseError *);
#endif
