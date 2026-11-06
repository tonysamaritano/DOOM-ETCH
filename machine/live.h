// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef DOOM_MACHINE_LIVE_H
#define DOOM_MACHINE_LIVE_H
#include "bridge.h"
typedef struct MachineLive MachineLive;
/* Persistent physical device: defaults are assumed calibration, never rate-scaled. */
MachineLive *MachineLiveCreate(const MachineSettings *,MotionPoint start_mm);
void MachineLiveDestroy(MachineLive *);
const EtchDevice *MachineLiveDevice(const MachineLive *);
int MachineLiveNeedsPath(const MachineLive *);
int MachineLiveSubmit(MachineLive *,const MotionPath *,MachineParseError *);
int MachineLiveTick(MachineLive *,double seconds,int active);
void MachineLivePause(MachineLive *,int paused);
int MachineLivePaused(const MachineLive *);
int MachineLiveErase(MachineLive *);
double MachineLiveSeconds(const MachineLive *);
#endif
