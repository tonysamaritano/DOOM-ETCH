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
