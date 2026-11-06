// SPDX-License-Identifier: GPL-2.0-or-later
#include "playback.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
struct MachinePlayback {
    MachineStepProgram program;
    EtchDevice *device;
    size_t next;
    double seconds, completed_seconds;
    int active,done,failed;
    MachineParseError error;
};
static int fail(MachineParseError *e,size_t line,const char *message)
{
    if (e) { e->line=line; snprintf(e->message,sizeof(e->message),"%s",message); }
    return 0;
}
static int runtime_fail(MachinePlayback *p,size_t line,const char *message)
{
    p->failed=1; return fail(&p->error,line,message);
}
void MachinePlaybackDestroy(MachinePlayback *p)
{
    if (p) { EtchDestroy(p->device); MachineStepProgramFree(&p->program); free(p); }
}
EtchDevice *MachinePlaybackDevice(MachinePlayback *p) { return p ? p->device : NULL; }
const MachineStepProgram *MachinePlaybackProgram(const MachinePlayback *p)
{
    return p ? &p->program : NULL;
}
int MachinePlaybackDone(const MachinePlayback *p) { return p && p->done; }
double MachinePlaybackSeconds(const MachinePlayback *p) { return p ? p->seconds : 0; }
const MachineParseError *MachinePlaybackError(const MachinePlayback *p) { return p ? &p->error : NULL; }
