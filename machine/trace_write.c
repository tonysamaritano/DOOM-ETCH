// SPDX-License-Identifier: GPL-2.0-or-later
#include "trace.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int fail(MachineParseError *e,const char *message)
{
    if (e) { e->line=0; snprintf(e->message,sizeof(e->message),"%s",message); }
    return 0;
}
static int valid(const MachineStepProgram *p,const EtchTrace *t,double seconds)
{
    if (!p || !t || !t->completed || t->failed || !isfinite(seconds) || seconds<0 ||
        !isfinite(t->clock) || fabs(seconds-t->clock)>1e-9 ||
        (p->count && !p->moves) || (t->count && !t->segments) ||
        (t->erase_count && !t->erases) || !MachineSettingsValidate(&p->settings,NULL)) return 0;
    size_t segment=0,erase=0,frame=0; double end=0;
    for (size_t i=0;i<p->count;i++) {
        const MachineStep *move=&p->moves[i];
        if (move->kind<MACHINE_MOVE_PAUSE) {
            if (segment==t->count) return 0;
            const EtchTraceSegment *s=&t->segments[segment++];
            double fields[]={s->t0,s->duration,s->ramp,s->peak,s->accel,s->slack,s->length,
                s->x0,s->y0,s->ux,s->uy,s->motor_x0,s->motor_y0};
            for (unsigned j=0;j<13;j++) if (!isfinite(fields[j])) return 0;
            if (s->t0<end || s->duration<0 || s->ramp<0 || s->peak<0 || s->accel<0 ||
                s->slack<0 || s->length<0 || s->line!=move->line || s->frame!=frame) return 0;
            end=s->t0+s->duration;
        } else if (move->kind==MACHINE_MOVE_PAUSE) {
            if (erase==t->erase_count) return 0;
            EtchTraceErase e=t->erases[erase++];
            if (!isfinite(e.t0) || !isfinite(e.duration) || e.t0<end || e.duration<=0) return 0;
            end=e.t0+e.duration; frame++;
        }
    }
    return segment==t->count && erase==t->erase_count && isfinite(end) && fabs(end-t->clock)<=1e-9;
}
