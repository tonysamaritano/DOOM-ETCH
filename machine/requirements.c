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
