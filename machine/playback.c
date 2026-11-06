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
static int advance(MachinePlayback *p)
{
    if (p->next==p->program.count) { p->done=1; p->active=0; return 1; }
    const MachineStep *s=&p->program.moves[p->next];
    if (s->kind==MACHINE_MOVE_END) { p->next++; p->done=1; p->active=0; return 1; }
    if (s->kind==MACHINE_MOVE_PAUSE) {
        p->next++; p->active=2;
        return EtchDeviceRequestErase(p->device) ? 1 : runtime_fail(p,s->line,"erase rejected");
    }
    size_t first=p->next;
    double feed=s->feed_mm_s;
    while (p->next<p->program.count) {
        const MachineStep *next=&p->program.moves[p->next];
        if (next->kind>=MACHINE_MOVE_PAUSE || next->feed_mm_s!=feed) break;
        p->next++;
    }
    if (!MachineSubmitBatch(p->device,&p->program,first,p->next-first,&p->error)) {
        p->failed=1; return 0;
    }
    p->active=1; return 1;
}
MachinePlayback *MachinePlaybackCreate(const MachineStepProgram *program,MachineParseError *e)
{
    if (!valid_program(program,e)) return NULL;
    MachinePlayback *p=calloc(1,sizeof(*p));
    if (!p) { fail(e,0,"out of memory"); return NULL; }
    p->program=*program; p->program.moves=NULL;
    if (program->count) {
        p->program.moves=malloc(program->count*sizeof(*program->moves));
        if (!p->program.moves) { fail(e,0,"out of memory"); MachinePlaybackDestroy(p); return NULL; }
        memcpy(p->program.moves,program->moves,program->count*sizeof(*program->moves));
    }
    MotionPoint start={program->start_steps[0]/program->settings.steps_per_mm[0],
                       program->start_steps[1]/program->settings.steps_per_mm[1]};
    p->device=EtchCreate(MachineSettingsConfig(&program->settings),start);
    if (!p->device) { fail(e,0,"invalid device configuration"); MachinePlaybackDestroy(p); return NULL; }
    if (!advance(p) || !MachinePlaybackTick(p,0)) {
        if (e) *e=p->error;
        MachinePlaybackDestroy(p); return NULL;
    }
    if (e) *e=(MachineParseError){0};
    return p;
}
int MachinePlaybackTick(MachinePlayback *p,double seconds)
{
    if (!p || !isfinite(seconds) || seconds<0 || p->failed) return 0;
    while (!p->done) {
        EtchStatus before=EtchDeviceGetStatus(p->device);
        double left=p->active==2 ? before.erase_left_s : before.remaining_s;
        double slice=fmin(seconds,left);
        if (!EtchDeviceTick(p->device,slice)) return runtime_fail(p,0,"device clock rejected tick");
        EtchStatus after=EtchDeviceGetStatus(p->device);
        double elapsed=p->active==2 ? p->program.settings.erase_seconds-after.erase_left_s : after.elapsed_s;
        double clock=p->completed_seconds+elapsed;
        if (!isfinite(clock) || clock<p->seconds) return runtime_fail(p,0,"invalid playback clock");
        p->seconds=clock; seconds=fmax(0,seconds-slice);
        if (EtchDeviceReady(p->device)) {
            p->completed_seconds=clock;
            if (!advance(p)) return 0;
        } else {
            if (!seconds) break;
            if (!slice) return runtime_fail(p,0,"device made no progress");
        }
    }
    return 1;
}
