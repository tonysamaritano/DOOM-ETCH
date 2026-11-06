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
static int valid_program(const MachineStepProgram *p,MachineParseError *e)
{
    if (!p || (p->count && !p->moves) || p->count>SIZE_MAX/sizeof(*p->moves))
        return fail(e,0,"invalid compiled program");
    if (!MachineSettingsValidate(&p->settings,e)) return 0;
    int64_t previous[2]={p->start_steps[0],p->start_steps[1]};
    for (unsigned a=0;a<2;a++) {
        double extent=a ? p->settings.height_mm : p->settings.width_mm;
        if (previous[a]<0 || previous[a]/p->settings.steps_per_mm[a]>extent)
            return fail(e,0,"required start outside travel");
    }
    for (size_t i=0;i<p->count;i++) {
        const MachineStep *s=&p->moves[i];
        if (s->kind<MACHINE_MOVE_DRAW || s->kind>MACHINE_MOVE_END ||
            !isfinite(s->feed_mm_s) || (s->kind==MACHINE_MOVE_DRAW ?
            s->feed_mm_s<=0 : s->feed_mm_s!=0)) return fail(e,s->line,"invalid compiled move");
        if (s->kind==MACHINE_MOVE_END && i+1!=p->count)
            return fail(e,s->line,"move after end");
        for (unsigned a=0;a<2;a++) {
            double extent=a ? p->settings.height_mm : p->settings.width_mm;
            if (s->target_steps[a]<0 || s->target_steps[a]/p->settings.steps_per_mm[a]>extent)
                return fail(e,s->line,"compiled target outside travel");
            if (s->kind>=MACHINE_MOVE_PAUSE && s->target_steps[a]!=previous[a])
                return fail(e,s->line,"control move changes cursor");
            previous[a]=s->target_steps[a];
        }
    }
    return 1;
}
