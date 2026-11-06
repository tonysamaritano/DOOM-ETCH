// SPDX-License-Identifier: GPL-2.0-or-later
#include "live.h"
#include <math.h>
#include <stdlib.h>
struct MachineLive {
    EtchDevice *device;
    MachineSettings settings;
    double seconds,phase_start;
    int paused,needs_path;
};
MachineLive *MachineLiveCreate(const MachineSettings *settings,MotionPoint start)
{
    MachineSettings defaults;
    if (!settings) { MachineSettingsDefault(&defaults); settings=&defaults; }
    if (!MachineSettingsValidate(settings,NULL)) return NULL;
    MachineLive *live=calloc(1,sizeof(*live));
    if (!live) return NULL;
    live->settings=*settings;
    live->device=EtchCreate(MachineSettingsConfig(settings),start);
    if (!live->device) { free(live); return NULL; }
    live->needs_path=1;
    return live;
}
void MachineLiveDestroy(MachineLive *live)
{
    if (live) { EtchDestroy(live->device); free(live); }
}
const EtchDevice *MachineLiveDevice(const MachineLive *live) { return live ? live->device : NULL; }
int MachineLiveNeedsPath(const MachineLive *live)
{
    return live && !live->paused && live->needs_path && EtchDeviceReady(live->device);
}
void MachineLivePause(MachineLive *live,int paused) { if (live) live->paused=paused!=0; }
int MachineLivePaused(const MachineLive *live) { return live && live->paused; }
double MachineLiveSeconds(const MachineLive *live) { return live ? live->seconds : 0; }
