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
static int fail(MachineParseError *error,const char *message)
{
    if (error) { error->line=0; snprintf(error->message,sizeof(error->message),"%s",message); }
    return 0;
}
int MachineLiveSubmit(MachineLive *live,const MotionPath *path,MachineParseError *error)
{
    if (!MachineLiveNeedsPath(live) || !MotionValid(path) ||
        path->width_mm!=live->settings.width_mm || path->height_mm!=live->settings.height_mm)
        return fail(error,"live device is occupied or travel differs");
    FILE *stream=tmpfile();
    if (!stream) return fail(error,"cannot serialize live G-code");
    int ok=MotionWriteGCode(stream,path,3600);
    if (ok && fseek(stream,0,SEEK_SET)) ok=0;
    MachineSettings settings=live->settings;
    MachineProgram parsed={0}; MachineStepProgram program={0};
    if (ok) ok=MachineRead(stream,&settings,&parsed,error);
    if (fclose(stream)) ok=0;
    if (ok) ok=MachineSteppersCompile(&program,&parsed,&settings,error);
    if (ok) {
        MotionPoint cursor=EtchDeviceGetStatus(live->device).cursor_mm;
        if (program.start_steps[0]/settings.steps_per_mm[0]!=cursor.x ||
            program.start_steps[1]/settings.steps_per_mm[1]!=cursor.y)
            ok=fail(error,"live G-code required start differs from persistent cursor");
    }
    size_t count=program.count;
    if (count && program.moves[count-1].kind==MACHINE_MOVE_END) count--;
    if (ok && count) ok=MachineSubmitBatch(live->device,&program,0,count,error);
    if (ok) {
        live->phase_start=live->seconds;
        live->needs_path=count==0 || EtchDeviceReady(live->device);
    }
    MachineProgramFree(&parsed); MachineStepProgramFree(&program);
    if (!ok && error && !error->message[0]) fail(error,"cannot submit live G-code");
    return ok;
}
int MachineLiveErase(MachineLive *live)
{
    if (!live) return 0;
    if (EtchDeviceReady(live->device)) live->phase_start=live->seconds;
    live->needs_path=1;
    return EtchDeviceRequestErase(live->device);
}
int MachineLiveTick(MachineLive *live,double seconds,int active)
{
    if (!live || !isfinite(seconds) || seconds<0) return 0;
    if (!active || live->paused) return 1;
    while (seconds>0 && !EtchDeviceReady(live->device)) {
        EtchStatus before=EtchDeviceGetStatus(live->device);
        double left=before.drawing ? before.remaining_s : before.erase_left_s;
        double slice=fmin(seconds,left);
        if (!EtchDeviceTick(live->device,slice)) return 0;
        EtchStatus after=EtchDeviceGetStatus(live->device);
        double used=before.drawing ? after.elapsed_s-before.elapsed_s :
                                     before.erase_left_s-after.erase_left_s;
        double elapsed=before.drawing ? after.elapsed_s :
            live->settings.erase_seconds-after.erase_left_s;
        double clock=live->phase_start+elapsed;
        if (!isfinite(clock) || clock<live->seconds ||
            (elapsed>0 && clock<=live->phase_start)) return 0;
        live->seconds=clock; seconds=fmax(0,seconds-used);
        if (before.drawing && !after.drawing) live->phase_start=clock;
        if (EtchDeviceReady(live->device)) {
            if (before.erasing) { live->needs_path=1; break; }
            live->phase_start=clock;
            if (!EtchDeviceRequestErase(live->device)) return 0;
        }
        if (used<=0 && slice>0) return 0;
    }
    return 1;
}
