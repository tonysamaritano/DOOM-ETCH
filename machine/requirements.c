// SPDX-License-Identifier: GPL-2.0-or-later
#include "requirements.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>
/* Reporting assumptions; no physical simulator parameters are changed. */
#define ASSUMED_SOUND_M_S 343.0
#define STANDARD_GRAVITY_M_S2 9.80665
static int fail(MachineParseError *e,size_t line,const char *message)
{
    if (e) { e->line=line; snprintf(e->message,sizeof(e->message),"%s",message); }
    return 0;
}
int MachineRequirementsParseFPS(const char *text,double *out)
{
    if (!text || !out) return 0;
    const char *p=text;
    if (*p=='+' || *p=='-') p++;
    size_t digits=0;
    while (*p>='0' && *p<='9') { p++; digits++; }
    if (*p=='.') {
        p++;
        while (*p>='0' && *p<='9') { p++; digits++; }
    }
    if (!digits) return 0;
    if (*p=='e' || *p=='E') {
        p++;
        if (*p=='+' || *p=='-') p++;
        const char *start=p;
        while (*p>='0' && *p<='9') p++;
        if (start==p) return 0;
    }
    if (*p) return 0;
    errno=0; char *end;
    double value=strtod(text,&end);
    if (*end || errno==ERANGE || !isfinite(value) || value<=0) return 0;
    *out=value; return 1;
}
static int point_valid(const MachineSettings *s,const int64_t point[2])
{
    for (unsigned a=0;a<2;a++) {
        double extent=a ? s->height_mm : s->width_mm;
        if (point[a]<0 || point[a]>0x1p52 ||
            point[a]>floor(extent*s->steps_per_mm[a]) ||
            point[a]/s->steps_per_mm[a]>extent) return 0;
    }
    return 1;
}
static int program_valid(const MachineStepProgram *p,MachineParseError *e)
{
    if (!p || (p->count && !p->moves) || p->count>SIZE_MAX/sizeof(*p->moves))
        return fail(e,0,"invalid requirements program storage");
    if (!MachineSettingsValidate(&p->settings,e)) return 0;
    if (!point_valid(&p->settings,p->start_steps))
        return fail(e,0,"required start outside commanded travel");
    for (size_t i=0;i<p->count;i++) {
        const MachineStep *m=&p->moves[i];
        if (m->kind<MACHINE_MOVE_DRAW || m->kind>MACHINE_MOVE_END ||
            (i && p->moves[i-1].kind==MACHINE_MOVE_END))
            return fail(e,m->line,"invalid move kind or move after end");
        if (!isfinite(m->feed_mm_s) ||
            (m->kind==MACHINE_MOVE_DRAW ? m->feed_mm_s<=0 : m->feed_mm_s!=0))
            return fail(e,m->line,"invalid commanded feed");
        if (m->kind<MACHINE_MOVE_PAUSE && !point_valid(&p->settings,m->target_steps))
            return fail(e,m->line,"target outside commanded travel");
    }
    return 1;
}
static int accumulate(MachineRequirements *r,const MachineSettings *s,
                      const int64_t from[2],const int64_t to[2])
{
    double dx=(to[0]-from[0])/s->steps_per_mm[0]/1000;
    double dy=(to[1]-from[1])/s->steps_per_mm[1]/1000;
    double distance=hypot(dx,dy);
    if (!isfinite(distance) || ((to[0]!=from[0] || to[1]!=from[1]) && distance==0) ||
        r->segments==SIZE_MAX) return 0;
    r->segments++;
    r->path_m+=distance;
    r->sum_sqrt_m+=sqrt(distance);
    r->dmax_m=fmax(r->dmax_m,distance);
    return isfinite(r->path_m) && isfinite(r->sum_sqrt_m);
}
static int finish(MachineRequirements *r,double fps,double knob)
{
    double scale=2*r->sum_sqrt_m*fps;
    r->no_stop_m_s=r->path_m*fps;
    r->no_stop_mach=r->no_stop_m_s/ASSUMED_SOUND_M_S;
    r->accel_m_s2=scale*scale;
    r->accel_g=r->accel_m_s2/STANDARD_GRAVITY_M_S2;
    r->peak_m_s=scale*sqrt(r->dmax_m);
    r->peak_mach=r->peak_m_s/ASSUMED_SOUND_M_S;
    r->knob_rpm=r->no_stop_m_s/knob*1000*60;
    double fields[]={scale,r->no_stop_m_s,r->no_stop_mach,r->accel_m_s2,
        r->accel_g,r->peak_m_s,r->peak_mach,r->knob_rpm};
    for (size_t i=0;i<sizeof(fields)/sizeof(*fields);i++)
        if (!isfinite(fields[i]) || (r->path_m>0 && fields[i]<=0)) return 0;
    return 1;
}
int MachineRequirementsCalculate(const MachineStepProgram *p,size_t begin,size_t end,
    double fps,MachineRequirements *out,MachineParseError *e)
{
    if (!out || !isfinite(fps) || fps<=0 || !p || begin>end || end>p->count)
        return fail(e,0,"invalid requirements range or frame rate");
    if (!program_valid(p,e)) return 0;
    MachineRequirements next={0};
    int64_t cursor[2]={p->start_steps[0],p->start_steps[1]};
    for (size_t i=0;i<end;i++) {
        const MachineStep *m=&p->moves[i];
        if (m->kind>=MACHINE_MOVE_PAUSE) continue;
        if (i>=begin && !accumulate(&next,&p->settings,cursor,m->target_steps))
            return fail(e,m->line,"commanded distance overflow or underflow");
        cursor[0]=m->target_steps[0]; cursor[1]=m->target_steps[1];
    }
    if (!finish(&next,fps,p->settings.knob_mm_turn))
        return fail(e,0,"drawing demand overflow or underflow");
    *out=next; if (e) *e=(MachineParseError){0};
    return 1;
}
void MachineRequirementsFramesFree(MachineRequirementsFrames *frames)
{
    if (frames) { free(frames->items); *frames=(MachineRequirementsFrames){0}; }
}
