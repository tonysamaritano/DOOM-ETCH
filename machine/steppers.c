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
