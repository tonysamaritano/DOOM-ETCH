// SPDX-License-Identifier: GPL-2.0-or-later
#include "run.h"
#include <inttypes.h>
#include <math.h>
static int fail(MachineParseError *e,const char *message)
{
    if (e) { e->line=0; snprintf(e->message,sizeof(e->message),"%s",message); }
    return 0;
}
MachinePlayback *MachineLoad(const char *filename,const char *const *assignments,
                             size_t count,MachineParseError *e)
{
    if (!filename || (count && !assignments)) { fail(e,"missing file or settings"); return NULL; }
    FILE *stream=fopen(filename,"rb");
    if (!stream) { fail(e,"cannot open G-code file"); return NULL; }
    MachineSettings settings; MachineSettingsDefault(&settings);
    MachineProgram parsed={0}; MachineStepProgram compiled={0};
    int ok=MachineRead(stream,&settings,&parsed,e);
    if (fclose(stream) && ok) ok=fail(e,"cannot close G-code file");
    for (size_t i=0;ok && i<count;i++) ok=MachineSettingsParse(&settings,assignments[i],e);
    if (ok) ok=MachineSteppersCompile(&compiled,&parsed,&settings,e);
    MachinePlayback *play=ok ? MachinePlaybackCreate(&compiled,e) : NULL;
    MachineProgramFree(&parsed); MachineStepProgramFree(&compiled);
    return play;
}
int MachineRun(MachinePlayback *play)
{
    if (!play) return 0;
    while (!MachinePlaybackDone(play)) {
        EtchStatus status=EtchDeviceGetStatus(MachinePlaybackDevice(play));
        double left=status.erasing ? status.erase_left_s : status.remaining_s;
        if (!MachinePlaybackTick(play,left)) return 0;
    }
    return 1;
}
