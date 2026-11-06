// SPDX-License-Identifier: GPL-2.0-or-later
#include "steppers.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
static int fail(MachineParseError *e,size_t line,const char *message)
{
    if (e) { e->line=line; snprintf(e->message,sizeof(e->message),"%s",message); }
    return 0;
}
void MachineSettingsDefault(MachineSettings *s)
{
    if (!s) return;
    *s=(MachineSettings){.steps_per_mm={20,20},.max_rate_mm_min={3600,3600},
        .acceleration_mm_s2={250,250},.width_mm=MOTION_WIDTH_MM,.height_mm=MOTION_HEIGHT_MM,
        .backlash_mm=ETCH_ASSUMED_BACKLASH_MM,.erase_seconds=ETCH_ASSUMED_ERASE_SECONDS,
        .ghost_fraction=ETCH_ASSUMED_GHOST_FRACTION,.knob_mm_turn=MACHINE_ASSUMED_KNOB_MM_TURN};
}
EtchConfig MachineSettingsConfig(const MachineSettings *s)
{
    if (!s) return (EtchConfig){0};
    return (EtchConfig){
        {s->max_rate_mm_min[0]/60,s->acceleration_mm_s2[0],s->backlash_mm,1/s->steps_per_mm[0]},
        {s->max_rate_mm_min[1]/60,s->acceleration_mm_s2[1],s->backlash_mm,1/s->steps_per_mm[1]},
        s->width_mm,s->height_mm,s->erase_seconds,s->ghost_fraction};
}
void MachineStepProgramFree(MachineStepProgram *p)
{
    if (p) { free(p->moves); *p=(MachineStepProgram){0}; }
}
int MachineSettingsValidate(const MachineSettings *s,MachineParseError *e)
{
    if (!s || !isfinite(s->width_mm) || s->width_mm<=0 ||
        !isfinite(s->height_mm) || s->height_mm<=0 ||
        !isfinite(s->backlash_mm) || s->backlash_mm<0 ||
        !isfinite(s->erase_seconds) || s->erase_seconds<=0 ||
        !isfinite(s->ghost_fraction) || s->ghost_fraction<0 || s->ghost_fraction>1 ||
        !isfinite(s->knob_mm_turn) || s->knob_mm_turn<=0)
        return fail(e,0,"invalid physical settings");
    for (unsigned a=0;a<2;a++) {
        double extent=a ? s->height_mm : s->width_mm;
        if (!isfinite(s->steps_per_mm[a]) || s->steps_per_mm[a]<=0 ||
            !isfinite(1/s->steps_per_mm[a]) || 1/s->steps_per_mm[a]<=0 ||
            !isfinite(extent*s->steps_per_mm[a]) || extent*s->steps_per_mm[a]>0x1p52 ||
            !isfinite(s->max_rate_mm_min[a]) || s->max_rate_mm_min[a]<=0 ||
            s->max_rate_mm_min[a]/60<=0 || !isfinite(s->acceleration_mm_s2[a]) ||
            s->acceleration_mm_s2[a]<=0) return fail(e,0,"invalid axis settings");
    }
    return 1;
}
static double *setting(MachineSettings *s,const char *key)
{
    const char *axis_keys[]={"$100","$101","$110","$111","$120","$121"};
    double *axes[]={&s->steps_per_mm[0],&s->steps_per_mm[1],&s->max_rate_mm_min[0],
        &s->max_rate_mm_min[1],&s->acceleration_mm_s2[0],&s->acceleration_mm_s2[1]};
    for (unsigned i=0;i<6;i++) if (!strcmp(key,axis_keys[i])) return axes[i];
    const char *keys[]={"width","height","backlash","erase","ghost","knob_mm_turn"};
    double *values[]={&s->width_mm,&s->height_mm,&s->backlash_mm,&s->erase_seconds,
        &s->ghost_fraction,&s->knob_mm_turn};
    for (unsigned i=0;i<6;i++) if (!strcmp(key,keys[i])) return values[i];
    return NULL;
}
int MachineSettingsParse(MachineSettings *out,const char *assignment,MachineParseError *e)
{
    if (!out || !assignment) return fail(e,0,"missing settings assignment");
    const char *equal=strchr(assignment,'=');
    if (!equal || equal==assignment || (size_t)(equal-assignment)>31)
        return fail(e,0,"expected setting=value");
    char key[32]; size_t length=(size_t)(equal-assignment);
    memcpy(key,assignment,length); key[length]=0;
    MachineSettings next=*out; double *target=setting(&next,key);
    if (!target) return fail(e,0,"unknown setting");
    char *end; double value=strtod(equal+1,&end);
    if (end==equal+1) return fail(e,0,"missing setting value");
    while (*end==' ' || *end=='\t' || *end=='\r' || *end=='\n') end++;
    if (*end || !isfinite(value)) return fail(e,0,"invalid setting value");
    *target=value;
    if (!MachineSettingsValidate(&next,e)) return 0;
    *out=next; if (e) *e=(MachineParseError){0};
    return 1;
}
/* Compile on the physical top-left grid. CNC Y is reflected exactly once. */
static int target(const MachineSettings *s,MotionPoint cnc,int64_t steps[2])
{
    if (!isfinite(cnc.x) || !isfinite(cnc.y) || cnc.x<0 || cnc.y<0 ||
        cnc.x>s->width_mm || cnc.y>s->height_mm) return 0;
    double physical[2]={cnc.x,s->height_mm-cnc.y};
    for (unsigned a=0;a<2;a++) {
        double rate=s->steps_per_mm[a],extent=a ? s->height_mm : s->width_mm;
        double index=round(physical[a]*rate);
        if (a==1) {
            /* Compare in CNC space: subtraction roundoff must not turn an
             * exact half-step into the lower step or swallow its neighbours. */
            double lower=floor(physical[a]*rate);
            double boundary=extent-(lower+.5)/rate;
            index=lower+(cnc.y<=boundary);
        }
        index=fmin(index,floor(extent*rate));
        if (index/rate>extent) index=fmax(0,index-1);
        steps[a]=(int64_t)index;
    }
    return 1;
}
static int compile_move(MachineStepProgram *p,const MachineMove *move,size_t i,
                        int direction[2],MachineParseError *e)
{
    MachineStep *step=&p->moves[i];
    if (move->kind<MACHINE_MOVE_DRAW || move->kind>MACHINE_MOVE_END ||
        !isfinite(move->feed_mm_s) ||
        (move->kind==MACHINE_MOVE_DRAW ? move->feed_mm_s<=0 : move->feed_mm_s!=0))
        return fail(e,move->line,"invalid move kind or feed");
    *step=(MachineStep){.feed_mm_s=move->feed_mm_s,.kind=move->kind,.line=move->line};
    if (!target(&p->settings,move->cnc_mm,step->target_steps))
        return fail(e,move->line,"target outside physical travel");
    for (unsigned a=0;a<2;a++) {
        int64_t delta=step->target_steps[a]-p->final_steps[a];
        if (move->kind>=MACHINE_MOVE_PAUSE && delta)
            return fail(e,move->line,"pause/end changes cursor");
        uint64_t magnitude=(uint64_t)(delta<0 ? -delta : delta);
        if (UINT64_MAX-p->total_steps[a]<magnitude)
            return fail(e,move->line,"step counter overflow");
        p->total_steps[a]+=magnitude;
        if (delta) {
            int sign=delta>0 ? 1 : -1;
            if (direction[a] && direction[a]!=sign) {
                if (p->reversals[a]==UINT64_MAX) return fail(e,move->line,"reversal overflow");
                p->reversals[a]++;
            }
            direction[a]=sign;
        }
        p->final_steps[a]=step->target_steps[a];
    }
    return 1;
}
int MachineSteppersCompile(MachineStepProgram *out,const MachineProgram *in,
                           const MachineSettings *settings,MachineParseError *e)
{
    if (!out || !in || (in->count && !in->moves) ||
        in->count>SIZE_MAX/sizeof(MachineStep)) return fail(e,0,"invalid program");
    if (!MachineSettingsValidate(settings,e)) return 0;
    MachineStepProgram next={.settings=*settings,.count=in->count};
    if (!target(settings,in->start_cnc_mm,next.start_steps))
        return fail(e,0,"required start outside travel");
    memcpy(next.final_steps,next.start_steps,sizeof(next.start_steps));
    next.moves=in->count ? calloc(in->count,sizeof(*next.moves)) : NULL;
    if (in->count && !next.moves) return fail(e,0,"out of memory");
    int direction[2]={0};
    for (size_t i=0;i<in->count;i++) {
        if ((i && in->moves[i-1].kind==MACHINE_MOVE_END) ||
            !compile_move(&next,&in->moves[i],i,direction,e)) {
            if (i && in->moves[i-1].kind==MACHINE_MOVE_END)
                fail(e,in->moves[i].line,"move after end");
            MachineStepProgramFree(&next); return 0;
        }
    }
    MachineStepProgramFree(out); *out=next;
    if (e) *e=(MachineParseError){0};
    return 1;
}
